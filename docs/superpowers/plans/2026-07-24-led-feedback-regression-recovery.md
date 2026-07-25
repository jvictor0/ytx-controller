# LED Feedback Regression Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep USB MIDI and LED feedback responsive under sustained traffic, lost aux show notifications, and full aux feedback queues.

**Architecture:** Decouple host MIDI parsing from LED backpressure on the main controller and add bounded recovery for stale aux-show state. On the aux controller, bound each feedback-drain pass, preserve newest data under overload without retry storms, and exclude NeoPixel transmission from active feedback bursts.

**Tech Stack:** Arduino SAMD21 C++, Microchip ASF SAMD11 C, GNU Arm Embedded toolchain, existing Makefiles

## Global Constraints

- Preserve the 7 kHz raw DIN Clock/Start/Continue/Stop forwarding path.
- Preserve 9-bit command framing and all 128 aux queue slots. The corrective
  review may extend `BURST_END` with repeated delivery-count bytes.
- Keep USB MIDI callbacks out of interrupt context.
- Keep the SAMD11 reset SP at `0x20001000` and its 512-byte stack floor at `0x20000e00`.
- Do not add a new test framework; use the repository's existing firmware builds and source invariant checks.
- Commit every firmware behavior change independently.
- Use native Codex implementers and xagent Claude Opus reviewers.
- Final review must cover the entire resulting branch, not only the reported regression.

## Whole-Firmware Re-Review Corrective Addendum

The final whole-firmware re-review extends the plan with these independently
committed corrections:

1. Configure the aux SysTick at one millisecond and preserve elapsed time across
   interrupt-masked NeoPixel output.
2. Reject out-of-burst frames, guard/reissue `BURST_INIT`, and retain overflow
   repaint requests through retries.
3. Announce every live LED show and serialize aux TX register access across ISR
   and main contexts.
4. Service reset recovery ahead of ordinary processing/memory-error gates and
   confirm memory errors before latching them.
5. Preserve bank-stage sentinel order under full-queue pressure.
6. Handle and count aux SERCOM receive errors, roll back partial pixel
   allocation, make 256-entry wrapping explicit, and remove obsolete state.

## Confirmation Review Corrective Addendum

The final confirmation review adds these independently committed corrections:

1. Preserve the idle `SHOW_END` heartbeat when a show-rate interval is latched
   but no changed pixels are waiting.
2. Stop a rejected burst between payload frames and enter retry immediately.
3. Complete both main-side and ISR-side feedback state reset before every
   `Init()` exit.
4. Require the explicit one-millisecond aux tick unit and retain the original
   masked-show arithmetic headroom.
5. Arm burst ACK state atomically before transmitting the terminator.
6. Defer command-driven physical shows until aux reception is atomically
   verified idle, retaining the command across deferral.
7. Record the 200-millisecond stale-pixel tradeoff and the exact limits of the
   tick-unit compile-time and linked-image checks.

---

### Task 1: Keep USB MIDI Responsive During LED Backlog

**Files:**
- Modify: `ytx-main-controller/headers/Defines.h`
- Modify: `ytx-main-controller/loop.ino`

**Interfaces:**
- Consumes: the existing `MIDI` instance and main-loop scheduling
- Produces: `ServiceUsbMidi()` and `USB_MIDI_MESSAGES_PER_LOOP`

- [ ] **Step 1: Record the failing invariant**

Run:

```bash
rg -n "MIDI\\.read|fbItemsToSend < FEEDBACK_UPDATE_BUFFER_SIZE|SendingData" \
  ytx-main-controller/loop.ino ytx-main-controller/feedback.ino
```

Expected: the only `MIDI.read()` is gated by feedback queue capacity and
`SendingData()`.

- [ ] **Step 2: Implement bounded, unconditional MIDI service**

Add beside the feedback scheduling constants:

```cpp
#define USB_MIDI_MESSAGES_PER_LOOP     8
```

Add before `loop()`:

```cpp
static void ServiceUsbMidi()
{
  for(uint8_t messagesRead = 0; messagesRead < USB_MIDI_MESSAGES_PER_LOOP; messagesRead++){
    if(!MIDI.read()){
      break;
    }
  }
}
```

Replace the gated single `MIDI.read()` block with:

```cpp
ServiceUsbMidi();
```

- [ ] **Step 3: Verify and commit**

Run:

```bash
make main
rg -n "MIDI\\.read|fbItemsToSend < FEEDBACK_UPDATE_BUFFER_SIZE|ServiceUsbMidi" \
  ytx-main-controller/loop.ino
git diff --check
```

Expected: the main build exits 0; only `ServiceUsbMidi()` calls `MIDI.read()`;
the loop call is not conditional on feedback state.

Commit:

```bash
git add ytx-main-controller/headers/Defines.h ytx-main-controller/loop.ino
git commit -m "fix: keep USB MIDI responsive during LED backlog"
```

### Task 2: Recover Stale Aux Show State

**Files:**
- Modify: `ytx-main-controller/headers/Defines.h`
- Modify: `ytx-main-controller/ytx-main-controller.ino`
- Modify: `ytx-main-controller/interrupts.ino`
- Modify: `ytx-main-controller/loop.ino`

**Interfaces:**
- Consumes: `fbShowInProgress`, `antMicrosAuxShow`, aux protocol commands, `micros()`
- Produces: `RecoverStaleAuxShow()` and `AUX_SHOW_TIMEOUT_US`

- [ ] **Step 1: Record the permanent-state path**

Run:

```bash
rg -n "fbShowInProgress|antMicrosAuxShow|RESET_HAPPENED" \
  ytx-main-controller
```

Expected: `antMicrosAuxShow` is written but never read, and
`RESET_HAPPENED` has no recovery behavior.

- [ ] **Step 2: Implement bounded recovery**

Add:

```cpp
#define AUX_SHOW_TIMEOUT_US            250000UL
```

Make `antMicrosAuxShow` a `volatile uint32_t`.

Add before `loop()`:

```cpp
static void RecoverStaleAuxShow()
{
  if(fbShowInProgress && ((uint32_t)(micros() - antMicrosAuxShow) >= AUX_SHOW_TIMEOUT_US)){
    fbShowInProgress = false;
  }
}
```

Call it before `ServiceUsbMidi()`. In the `RESET_HAPPENED` handler, clear
`fbShowInProgress`, `waitingForAck`, and any partial error-index receive state.

- [ ] **Step 3: Verify and commit**

Run:

```bash
make main
rg -n "AUX_SHOW_TIMEOUT_US|RecoverStaleAuxShow|RESET_HAPPENED|volatile uint32_t antMicrosAuxShow" \
  ytx-main-controller
git diff --check
```

Commit:

```bash
git add ytx-main-controller/headers/Defines.h \
  ytx-main-controller/ytx-main-controller.ino \
  ytx-main-controller/interrupts.ino \
  ytx-main-controller/loop.ino
git commit -m "fix: recover stale aux show state"
```

### Task 3: Bound Aux Feedback Processing

**Files:**
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/defines.h`
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c`

**Interfaces:**
- Consumes: `feedbackFramesPending` and the existing atomic frame claim
- Produces: `FEEDBACK_FRAMES_PER_UPDATE`

- [ ] **Step 1: Record the unbounded drain**

Run:

```bash
rg -n "while\\(feedbackFramesPending > 0\\)|feedbackDataUpdate" \
  ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c
```

Expected: the consumer drains until the shared pending count reaches zero.

- [ ] **Step 2: Add the per-pass bound**

Add:

```c
#define FEEDBACK_FRAMES_PER_UPDATE 8
```

Track `uint8_t framesProcessed = 0`; require both pending data and
`framesProcessed < FEEDBACK_FRAMES_PER_UPDATE` in the loop condition; increment
the processed count after each claimed frame is applied.

- [ ] **Step 3: Verify and commit**

Run:

```bash
make aux
rg -n "FEEDBACK_FRAMES_PER_UPDATE|framesProcessed|while\\(" \
  ytx-aux-controller/SAMD11-NeoPixel/src/defines.h \
  ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c
git diff --check
```

Commit:

```bash
git add ytx-aux-controller/SAMD11-NeoPixel/src/defines.h \
  ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c
git commit -m "fix: bound aux feedback processing"
```

### Task 4: Make Aux Queue Overload Self-Damping

**Files:**
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c`

**Interfaces:**
- Consumes: `readIdx`, `writeIdx`, `feedbackFramesPending`, and the existing normal ACK path
- Produces: a drop-oldest-and-accept-newest full-queue policy

- [ ] **Step 1: Record the retry-storm path**

Run:

```bash
sed -n '165,220p' ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c
```

Expected: a full queue sends `CHECKSUM_ERROR`, clears receive state, and enters
`discardingBurst`.

- [ ] **Step 2: Replace capacity NAK with drop-oldest**

When the queue is full, increment and wrap `readIdx`, decrement
`feedbackFramesPending`, and increment `failsPerSecond`. Continue through the
normal write, pending increment, burst index, and ACK logic. Do not send
`CHECKSUM_ERROR`, clear receive state, or set `discardingBurst` for capacity.

- [ ] **Step 3: Verify and commit**

Run:

```bash
make aux
sed -n '165,225p' ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c
git diff --check
```

Expected: the full path makes exactly one slot available and the normal publish
returns pending occupancy to 128.

Commit:

```bash
git add ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c
git commit -m "fix: make aux queue overload self-damping"
```

### Task 5: Prevent Shows During Feedback Bursts

**Files:**
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/main.c`

**Interfaces:**
- Consumes: `showNow`, `receivingBank`, and `receivingFeedbackData`
- Produces: strict show/burst exclusion

- [ ] **Step 1: Record the permissive condition**

Run:

```bash
rg -n "showNow.*receivingBank.*receivingFeedbackData" \
  ytx-aux-controller/SAMD11-NeoPixel/src/main.c
```

Expected: the show condition uses logical OR.

- [ ] **Step 2: Require both receive states to be idle**

Change the condition to:

```c
if(showNow && !receivingBank && !receivingFeedbackData){
```

- [ ] **Step 3: Verify and commit**

Run:

```bash
make aux
rg -n "showNow.*receivingBank.*receivingFeedbackData" \
  ytx-aux-controller/SAMD11-NeoPixel/src/main.c
git diff --check
```

Commit:

```bash
git add ytx-aux-controller/SAMD11-NeoPixel/src/main.c
git commit -m "fix: prevent LED shows during feedback bursts"
```

### Task 6: Combined Verification and Broad Review

**Files:**
- Verify only unless review findings require fixes

**Interfaces:**
- Consumes: Tasks 1 through 5 and the three earlier hardening commits
- Produces: complete build evidence and a broad Claude review

- [ ] **Step 1: Run established verification**

Run:

```bash
make all
git diff --check 7feefa1..HEAD
git status --short
git log --oneline -12
```

Expected: both firmware targets build; diff check exits 0; only generated build
directories are untracked.

- [ ] **Step 2: Confirm memory invariants**

Run:

```bash
/Users/joyo/arm-gnu-toolchain/bin/arm-none-eabi-nm -n \
  ytx-aux-controller/SAMD11-NeoPixel/build/SAMD11-NeoPixel.elf |
  rg " (_end|_sstack|_estack)$"
```

Expected: `_estack=0x20001000`, `_sstack=0x20000e00`, and `_end < _sstack`.

- [ ] **Step 3: Dispatch the broad review**

Generate one review package for `7feefa1..HEAD` and send it to Claude Opus
through xagent. Require findings first, ordered by severity, with concrete
file/line references. The review must cover all changed code and surrounding
interactions: correctness, concurrency, protocol state machines, interrupt
safety, bounds, memory layout, error handling, performance, maintainability,
and test/build adequacy.

- [ ] **Step 4: Address findings once and re-review**

If Claude finds Critical or Important issues, dispatch one Codex fix agent with
the complete findings list. Build again and send the fix range to Claude for one
scoped re-review. Do not leave load-bearing findings unresolved.
