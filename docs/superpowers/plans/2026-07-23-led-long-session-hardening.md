# LED Long-Session Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate the long-session LED feedback queue race and independently harden the aux controller's stack boundary and 128-frame feedback ring.

**Architecture:** Keep the 7 kHz interrupt solely for raw DIN real-time transport and serialize USB MIDI callbacks with feedback consumption in the main loop. On the SAMD11, reserve a guarded 512-byte stack at the top of RAM and distinguish a full feedback ring from an empty one with an interrupt-safe occupancy count.

**Tech Stack:** Arduino SAMD21 C++, Microchip ASF SAMD11 C, GNU Arm Embedded linker, existing Makefiles

## Global Constraints

- Preserve the 7 kHz raw DIN Clock/Start/Continue/Stop forwarding path.
- Preserve the current feedback wire format and all 128 aux queue slots.
- Do not redesign NeoPixel transmission or add host-side test infrastructure.
- Use the repository's existing `make main`, `make aux`, and `make all` verification.
- Produce exactly three independently reviewable firmware commits in task order.
- Use native `gpt-5.5` for implementers and xagent Claude for reviewers.
- Do not flash hardware; the user will test after all three fixes are complete.

---

### Task 1: Serialize USB MIDI Feedback Processing

**Files:**
- Modify: `ytx-main-controller/interrupts.ino:5-32`
- Modify: `ytx-main-controller/loop.ino:33-62`

**Interfaces:**
- Consumes: existing `MIDI`, `feedbackHw`, `FEEDBACK_UPDATE_BUFFER_SIZE`, and `MIDIpullTask`
- Produces: a timer handler that forwards only DIN transport bytes and a main-loop USB MIDI parse point

- [ ] **Step 1: Confirm the failing architecture**

Run:

```bash
rg -n "MIDI\\.read|MIDIpull_Handler|feedbackHw\\.Update" \
  ytx-main-controller/interrupts.ino ytx-main-controller/loop.ino
```

Expected: `MIDI.read()` appears inside `MIDIpull_Handler()`, while
`feedbackHw.Update()` appears in `loop()`.

- [ ] **Step 2: Remove USB parsing from the timer**

Delete this complete block from `MIDIpull_Handler()`:

```cpp
  // Call USB MIDI read functions and run callbacks if message arrived
  if(feedbackHw.fbItemsToSend < FEEDBACK_UPDATE_BUFFER_SIZE && !feedbackHw.SendingData()){
    MIDI.read();
  }
```

Leave the `while(Serial1.available())` real-time transport forwarding code and
the timer setup unchanged.

- [ ] **Step 3: Add the guarded USB parse point to the main loop**

Insert this block after the pending diagnostics early return and before CDC
processing:

```cpp
  // Parse USB MIDI in the main loop so feedback callbacks cannot race
  // feedbackHw.Update() from the periodic DIN transport interrupt.
  if(feedbackHw.fbItemsToSend < FEEDBACK_UPDATE_BUFFER_SIZE && !feedbackHw.SendingData()){
    MIDI.read();
  }
```

- [ ] **Step 4: Verify source ownership and build the main firmware**

Run:

```bash
rg -n "MIDI\\.read|MIDIpull_Handler|feedbackHw\\.Update" \
  ytx-main-controller/interrupts.ino ytx-main-controller/loop.ino
make main
```

Expected: `MIDI.read()` appears only in `loop.ino`; the DIN timer handler and
`feedbackHw.Update()` remain; Arduino CLI exits 0.

- [ ] **Step 5: Commit only Task 1**

```bash
git add ytx-main-controller/interrupts.ino ytx-main-controller/loop.ino
git commit -m "fix: serialize USB MIDI feedback processing"
```

### Task 2: Harden the SAMD11 Stack Boundary

**Files:**
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/ASF/sam0/utils/linker_scripts/samd11/gcc/samd11d14am_flash.ld:56-57,145-156`
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/ASF/sam0/utils/syscalls/gcc/syscalls.c:37-72`

**Interfaces:**
- Consumes: linker-provided `_end` and the SAMD11 RAM range `0x20000000..0x20001000`
- Produces: `_estack = 0x20001000`, `_sstack = 0x20000e00`, and a bounded `_sbrk`

- [ ] **Step 1: Confirm the current unsafe stack layout**

Run:

```bash
rg -n "STACK_SIZE|\\.stack|_sstack|_estack|_sbrk|heap \\+=" \
  ytx-aux-controller/SAMD11-NeoPixel/src/ASF/sam0/utils/linker_scripts/samd11/gcc/samd11d14am_flash.ld \
  ytx-aux-controller/SAMD11-NeoPixel/src/ASF/sam0/utils/syscalls/gcc/syscalls.c
```

Expected: the linker reserves a fixed `0x100`-byte `.stack` after BSS and
`_sbrk` advances the heap without checking `_sstack`.

- [ ] **Step 2: Place the stack at the top of RAM**

Remove the `STACK_SIZE` definition and the complete `.stack (NOLOAD)` output
section. Keep `_end` aligned after BSS, then define:

```ld
    . = ALIGN(4);
    _end = . ;

    /* Stack at top of RAM, grows downward. _sstack is the minimum floor;
       heap is never allowed to grow past it. */
    _estack = ORIGIN(ram) + LENGTH(ram);
    _sstack = _estack - 0x200;
```

- [ ] **Step 3: Bound heap growth at the stack floor**

Add:

```c
#include <errno.h>
```

Declare the linker symbol after the syscall prototypes:

```c
extern char _sstack;
```

Replace `_sbrk`'s unconditional advance with:

```c
	if ((heap + incr) > (unsigned char *)&_sstack) {
		errno = ENOMEM;
		return (caddr_t)-1;
	}

	prev_heap = heap;
	heap += incr;
```

- [ ] **Step 4: Build and inspect the linked stack symbols**

Run:

```bash
make aux
/Users/joyo/arm-gnu-toolchain/bin/arm-none-eabi-nm -n \
  ytx-aux-controller/SAMD11-NeoPixel/build/SAMD11-NeoPixel.elf |
  rg " (_end|_sstack|_estack)$"
```

Expected: aux build exits 0, `_sstack` is `20000e00`, `_estack` is
`20001000`, and `_end` is below `_sstack`.

- [ ] **Step 5: Commit only Task 2**

```bash
git add \
  ytx-aux-controller/SAMD11-NeoPixel/src/ASF/sam0/utils/linker_scripts/samd11/gcc/samd11d14am_flash.ld \
  ytx-aux-controller/SAMD11-NeoPixel/src/ASF/sam0/utils/syscalls/gcc/syscalls.c
git commit -m "fix: harden aux controller stack bounds"
```

### Task 3: Track Aux Feedback Queue Occupancy

**Files:**
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/variables.h:41-43`
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/variables.c:38-40`
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c:169-210`
- Modify: `ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c:113-129,385-405`

**Interfaces:**
- Consumes: `FeedbackFramesBuffer`, `FEEDBACK_BUFFER_LENGTH`, `readIdx`, `writeIdx`, and the existing checksum retry protocol
- Produces: `volatile uint16_t feedbackFramesPending` shared by the single ISR producer and main-loop consumer

- [ ] **Step 1: Confirm the full/empty ambiguity**

Run:

```bash
rg -n "readIdx != writeIdx|\\+\\+readIdx|\\+\\+writeIdx|FEEDBACK_BUFFER_LENGTH" \
  ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/variables.c
```

Expected: both empty detection and a completely full 128-entry ring can
produce `readIdx == writeIdx`.

- [ ] **Step 2: Declare explicit pending-frame state**

Add to `variables.h` beside the indices:

```c
extern volatile uint16_t feedbackFramesPending;
```

Define it in `variables.c` beside the indices:

```c
volatile uint16_t feedbackFramesPending = 0;
```

- [ ] **Step 3: Guard producer insertion and publish occupancy**

In the valid-checksum branch, before writing `FeedbackFramesBuffer[writeIdx]`,
handle a full queue with the existing recoverable burst-error sequence:

```c
if (feedbackFramesPending >= FEEDBACK_BUFFER_LENGTH) {
	failsPerSecond++;
	SendToMain(CHECKSUM_ERROR);
	SendDataToMain(burstFrameIndex);
	SendDataToMain(burstFrameIndex);
	receivedBytes = 0;
	receivingBank = false;
	receivingFeedbackData = false;
	discardingBurst = true;
	return;
}
```

After advancing `writeIdx`, publish the completed frame:

```c
feedbackFramesPending++;
```

- [ ] **Step 4: Atomically claim each frame before processing it**

Change `feedbackDataAvailable()` to:

```c
bool feedbackDataAvailable(){
	return (feedbackFramesPending > 0);
}
```

At the start of `feedbackDataUpdate()`, replace the index-comparison loop and
direct buffer field reads with:

```c
	while(feedbackFramesPending > 0){
		FeedbackFrameData frameData;

		__disable_irq();
		if(feedbackFramesPending == 0){
			__enable_irq();
			break;
		}

		frameData = FeedbackFramesBuffer[readIdx];
		if(++readIdx >= FEEDBACK_BUFFER_LENGTH)
			readIdx = 0;
		feedbackFramesPending--;
		__enable_irq();

		uint8_t frame = frameData.updateFrame;
		uint8_t elementToChange = frameData.updateN;
		uint8_t orientationMeta = frameData.updateO;
		bool isBlendFrame = (frame == ENCODER_BLEND_FRAME);
		bool vertical = isBlendFrame ? (orientationMeta & 0x01) : orientationMeta;
		uint16_t newState = frameData.updateState;
		uint8_t intR = frameData.updateR;
		uint8_t intG = frameData.updateG;
		uint8_t intB = frameData.updateB;
```

At the end of the loop, use `frameData.updateN` and
`frameData.updateFrame` for strip selection, and remove the old `readIdx`
advance because the frame was already claimed.

- [ ] **Step 5: Build and inspect every queue-state access**

Run:

```bash
make aux
rg -n "feedbackFramesPending|readIdx != writeIdx|\\+\\+readIdx|\\+\\+writeIdx" \
  ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/variables.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/variables.h
```

Expected: aux build exits 0; availability uses the pending count; the ISR
checks capacity and increments after publishing a frame; the consumer claims
and decrements under a short interrupt-disabled section; no
`readIdx != writeIdx` availability test remains.

- [ ] **Step 6: Commit only Task 3**

```bash
git add \
  ytx-aux-controller/SAMD11-NeoPixel/src/variables.h \
  ytx-aux-controller/SAMD11-NeoPixel/src/variables.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/interrupts.c \
  ytx-aux-controller/SAMD11-NeoPixel/src/feedback.c
git commit -m "fix: track aux feedback queue occupancy"
```

### Task 4: Combined Verification

**Files:**
- Verify only; no production files should change

**Interfaces:**
- Consumes: the three preceding firmware commits
- Produces: final build evidence and review package

- [ ] **Step 1: Run the complete established build**

Run:

```bash
make all
```

Expected: both SAMD11 aux and SAMD21 main firmware builds exit 0.
Pre-existing compiler warnings may remain, but the three commits must not add
new warnings.

- [ ] **Step 2: Verify commit boundaries and worktree cleanliness**

Run:

```bash
git log --oneline -5
git diff --check HEAD~3..HEAD
git status --short
```

Expected: the last three firmware commits match the required task order;
`git diff --check` exits 0; only generated build directories are untracked.

