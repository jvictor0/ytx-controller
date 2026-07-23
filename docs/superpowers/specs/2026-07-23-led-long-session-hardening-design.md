# LED Long-Session Hardening Design

## Goal

Remove the main-controller feedback-queue race that can accumulate hidden LED
updates during long sessions, and independently harden two aux-controller
failure modes: unsafe stack placement and ambiguous full/empty ring-buffer
state.

## Scope

This change contains three independently committed firmware fixes:

1. Parse USB MIDI in the main loop while retaining the low-latency DIN
   Clock/Start/Continue/Stop forwarding timer.
2. Port the upstream SAMD11 stack and heap-boundary hardening.
3. Track aux feedback-ring occupancy explicitly so all 128 entries remain
   usable and a full queue cannot appear empty.

The changes do not redesign NeoPixel transmission, change the feedback wire
format, modify the host application, or add a new test framework.

## Fix 1: Serialize USB MIDI Feedback Processing

`MIDIpull_Handler()` remains a 7 kHz timer handler, but it is responsible only
for draining the DIN UART and forwarding the four supported real-time
transport bytes to USB. It must not call `MIDI.read()` or inspect feedback
queue state.

The main loop calls one guarded `MIDI.read()` before input processing and
`feedbackHw.Update()`. Keeping the existing queue-capacity and
`SendingData()` guard preserves the current backpressure policy. USB MIDI
callbacks and feedback-queue consumption consequently execute in one context,
so the interrupt can no longer overwrite a main-loop update to
`fbItemsToSend`, queue indices, or coalescing metadata.

The timer and the SysEx timer stop/start protection remain because the timer
still uses the USB MIDI object to forward real-time transport.

## Fix 2: Harden the SAMD11 Stack Boundary

The aux linker script places `_estack` at the top of the 4 KiB RAM region and
defines `_sstack` 512 bytes below it. This replaces the current fixed 256-byte
stack section placed immediately after BSS.

The `_sbrk` implementation treats `_sstack` as a hard heap ceiling. A request
that would cross the reserved stack floor fails with `ENOMEM` and returns
`(caddr_t)-1`; successful requests retain the existing behavior.

Only the linker and syscall changes from upstream commit `04ebe3d` are ported.
The stale Microchip project reference to a nonexistent `fault-handlers.c` file
and unrelated whitespace changes are excluded.

## Fix 3: Distinguish Full and Empty Aux Feedback Queues

The aux ring keeps its existing 128-frame storage and read/write indices. A
`volatile uint16_t feedbackFramesPending` counter distinguishes:

- empty: `feedbackFramesPending == 0`
- full: `feedbackFramesPending == FEEDBACK_BUFFER_LENGTH`

The serial receive ISR is the only producer. After validating and copying a
frame, it advances `writeIdx` and increments the pending count. If the queue is
already full, it must not overwrite unread data. It reports the existing burst
frame error, resets the receive state, and enters the existing discard/retry
path.

The main loop is the only consumer. For each frame, it briefly disables
interrupts, copies the queued structure into a local value, advances
`readIdx`, and decrements the pending count. It then re-enables interrupts and
performs all LED calculations outside the critical section. This makes the
shared count update atomic without extending the UART blackout across LED
processing.

`feedbackDataAvailable()` reads the pending count rather than comparing the
indices. No protocol constants, frame layouts, or buffer capacity change.

## Error Handling

- A full aux queue is treated as a recoverable burst failure rather than
  silently overwriting an unread frame.
- Existing checksum retry and fresh-`BURST_INIT` resynchronization remain the
  recovery mechanism.
- Heap growth into the reserved stack area fails explicitly instead of
  corrupting stack memory.

## Verification

The repository has firmware builds but no unit-test harness for these embedded
paths. Per project direction, no host-side test infrastructure will be
invented.

Each independent commit is verified with the narrow established build:

- Fix 1: `make main`
- Fix 2: `make aux`, plus symbol/map inspection confirming the 512-byte stack
  reservation and heap ceiling symbols
- Fix 3: `make aux`, plus source inspection of all producer and consumer index
  updates

After all commits, `make all` verifies the combined firmware. Hardware flashing
and the long-session behavioral test are left for the user after all three
fixes are complete.

## Commit Boundaries

1. `fix: serialize USB MIDI feedback processing`
2. `fix: harden aux controller stack bounds`
3. `fix: track aux feedback queue occupancy`

The design and implementation plan are documentation commits separate from
the three firmware commits.

