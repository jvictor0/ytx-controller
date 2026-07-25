# Post-hardware final fix report

Review base: `565f8d5d2973d99307947e37e72f29ec5109bd16`

Behavior-fix range: `9ba6c1b^..8f70e10`

Outcome: `DONE_WITH_CONCERNS`

All confirmed Critical and Important findings were fixed. All cheap, safe Minor
findings were fixed. The unsupported serial-buffer claim was adjudicated from
the active Yaeltex UART implementation. The required stack, framing, queue,
DIN-realtime, and USB-main-context constraints remain intact.

## Finding disposition

### C1 — Boot rainbow restart livelock: confirmed and fixed

Commit: `9ba6c1b fix: make startup rainbow bounded and resumable`

- The rainbow now retains its frame across an idle deferral instead of
  restarting at frame zero.
- It renders one frame per aux main-loop pass and has an 8-second aux-side
  deadline.
- Main waits up to 9 seconds, covering the aux deadline and protocol overhead.
- For the largest verified shipping layout (32 encoders and 96 digitals), one
  rainbow frame transmits 1,824 WS2812 bytes, or about 18.24 ms at 10 us/byte.
  The 256 frames plus the existing 2 ms inter-frame delay are about 5.18
  seconds before loop/protocol overhead, so the old 5-second main timeout was
  too short and the new bounds cover it.
- Allocation failure completes the rainbow command immediately rather than
  entering an animation with no pixel storage.

### C2 — Permanent allocation-failure latch: confirmed and fixed

Commits:

- `ba821ac fix: reboot aux before accepting changed LED layouts`
- `8d47435 fix: bound recovery from aux allocation faults`

The aux now reboots when a post-initialization `INIT_VALUES` frame changes the
allocated layout, and also reboots to retry after an allocation failure rather
than acknowledging stale strip sizes.

The main now treats a confirmed memory report as recoverable first: it performs
at most two clean aux reset/reinitialization attempts while USB MIDI continues
to run. A controller that stays healthy for 500 ms replenishes that transient
recovery budget. A failure that survives both clean attempts remains latched as
an unsupported layout or persistent allocator fault.

Current linked symbols are:

```text
_end    = 0x2000069c
_sstack = 0x20000e00
_estack = 0x20001000
```

This leaves 1,892 bytes between BSS and the fixed stack floor. The largest
verified shipping layout needs:

```text
32 encoders: 32 * 16 * 3 bytes + 2 * 8-byte malloc overhead = 1,552
96 digitals: 96 * 3 bytes      + 2 * 8-byte malloc overhead =   304
pixel allocation total                                          1,856
required safety margin                                             32
checked total                                                   1,888
```

It therefore fits with 4 bytes beyond the required margin. Arbitrary
combinations up to the main firmware's generic 256-digital ceiling are not
promised alongside 32 encoders; impossible combinations are rejected rather
than reclaiming the protected stack.

### I3 — Hot-path show race: confirmed and fixed

Commit: `7294736 fix: exclude hot shows from aux reception`

The 66 Hz show path now disables interrupts, verifies both receive state and
pending SERCOM RX/error flags, and holds the exclusion through the physical
show. It restores the caller's prior `PRIMASK`, matching the command-driven
show discipline.

### I4/I5 — Stale replacement and overflow repaint storm: confirmed and fixed

Commit: `078cef1 fix: converge LED repaint after queue loss`

Main-side replacement now requests a repaint whenever it displaces a different
logical control. Aux-overflow reports feed the same repaint latch. The latch is
serviced only after the main feedback pipeline is completely idle and has been
quiet for 50 ms, and it is cleared before scheduling one bank repaint. A retry
therefore requires a new, observed loss event; it cannot recursively amplify
pressure by itself.

The ISR overflow flag is now captured and cleared in one interrupt-disabled
section, so a report arriving after the clear remains set for the next pass.

### M1 — Dead out-of-burst send: confirmed and removed

Commit: `60a84d5 refactor: remove unreachable empty-bank send`

The zero-digital branch could not have a populated feedback frame and was
removed.

### M2 — Changed counts acknowledged without reallocation: confirmed and fixed

Commit: `ba821ac fix: reboot aux before accepting changed LED layouts`

Same-layout healthy reinitialization remains supported. A changed layout or a
retry after allocation failure resets the aux so allocation is performed from
a clean boot before acknowledgement.

### M3 — Concurrent overflow report loss: confirmed and fixed

Commit: `078cef1 fix: converge LED repaint after queue loss`

Covered by the atomic capture-and-clear described under I4/I5.

### M4 — Stray ordinary ACK: confirmed and fixed

Commit: `99bae91 fix: tag ordinary aux acknowledgements`

Brightness acknowledgements are now preceded by a `CHANGE_BRIGHTNESS` command
tag. Main only completes an ordinary wait after that tag, an initialization
wait after the existing `INIT_VALUES` tag, and a burst wait while the burst ACK
is explicitly armed. Untagged stray ACKs no longer complete an unrelated wait.

### M5 — Per-update batch exceeds serial buffer: not applicable

The active main-to-aux `Serial` object uses
`cores/arduino/customUART/customUart.cpp`, whose `write()` and `write9bit()`
call `SERCOM::writeDataUART*()` directly. Those routines wait for DRE before
writing; they do not use `RingBuffer.h`'s 150-byte UART buffer. The existing
eight-frame per-`Update()` bound and 128-slot protocol queue were therefore
left unchanged.

### M6 — SysTick wrap sampling order: confirmed and fixed

Commit: `0b255a8 fix: sample SysTick wraps consistently during LED shows`

Start and end samples now use retry loops around `CTRL`/`VAL`. A wrap detected
across a sample causes a post-wrap resample, while the end helper retains
whether any wrap occurred during the masked show.

### M7 — Stack reserve evidence: preserved

The linker still fixes `_estack` at `0x20001000`, `_sstack` at `0x20000e00`,
and asserts `_end <= _sstack`. Fresh `.su` artifacts report the deepest static
frame as 136 bytes (`configure_usart`), unchanged from the review evidence.
The 512-byte reserve was not reduced.

### M8 — All-on sequence restarts delay stages: confirmed and fixed

Commit: `deeb025 fix: resume all-on diagnostics after receive contention`

The developer all-on sequence now advances one retained color stage per
successful idle show. A deferral retries the current stage and no longer
repeats already completed 1.5-second delays. Existing BSS was repurposed for
the stage, keeping `_end` at `0x2000069c`.

### M9 — Encoder diagnostics block USB MIDI: confirmed and fixed

Commit: `8f70e10 fix: service MIDI and LEDs during encoder diagnostics`

The bounded 10–5,000 ms diagnostic capture now services up to the normal USB
MIDI per-loop budget and calls `feedbackHw.Update()` once per millisecond.
Raw DIN realtime forwarding remains in the 7 kHz callback.

## Verification

Fresh verification after `make clean`:

- `make all`: passed.
- Aux image: text 11,684 bytes, data 92 bytes, BSS 1,600 bytes.
- Main image: 111,732 bytes, 48% of available program storage.
- `git diff --check 7feefa1..HEAD`: passed.
- Link symbols: `_end=0x2000069c`, `_sstack=0x20000e00`,
  `_estack=0x20001000`.
- Deepest reported aux static frame: 136 bytes.
- Aux queue remains `FEEDBACK_BUFFER_LENGTH=128`.
- Main uses `SERIAL_9O2`; aux uses odd parity and two stop bits.
- Raw DIN task remains `MIDIpull_Handler` at 7 kHz.
- USB `MIDI.read()` remains out of interrupt context.
- Worktree status contains only the two generated build directories plus this
  report before its commit.

No new test framework was introduced. The build still emits pre-existing aux
warnings, including ASF signature/redefinition warnings and old NeoPixel
warnings outside this fix range.

## Remaining concerns

- The 32-encoder/96-digital shipping layout has only 4 bytes beyond the
  explicit 32-byte pixel-heap safety margin. Future aux BSS additions must keep
  the link-map/capacity check load-bearing.
- This final fix wave was clean-built and statically checked but was not
  reflashed for another whole-controller soak. The earlier matched-9O2 image
  passed hardware testing; the user-planned long-session test remains the
  acceptance test for this final range.
- Encoder diagnostics now preserve USB MIDI and LED transport, but intentionally
  continue to suspend ordinary input scanning while they exclusively sample
  the selected encoder.
