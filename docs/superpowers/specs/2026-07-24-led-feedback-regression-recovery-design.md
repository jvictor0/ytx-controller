# LED Feedback Regression Recovery Design

## Goal

Eliminate every material firmware mechanism identified by the three regression
reviews that can freeze LED output or make host-to-controller MIDI stop while
physical controller input continues to reach the host.

## Findings Being Addressed

The reviewed firmware has five interacting failure mechanisms:

1. USB MIDI parsing is gated by LED queue capacity and processes only one
   message per main-loop pass.
2. A lost aux `SHOW_END` leaves `fbShowInProgress` set forever.
3. The aux feedback consumer can remain in its drain loop indefinitely under
   sustained ingress.
4. A full aux queue sends a negative acknowledgement that causes the main
   controller to retry into the same full queue.
5. The aux may begin a NeoPixel show between frames of an active burst because
   its eligibility condition uses logical OR instead of logical AND.

The SAMD11 stack/heap change is not part of the runtime regression. Its reset
stack position and heap ceiling are correct, and retaining the 512-byte stack
floor avoids restoring the stack-corruption risk it removed. This recovery pass
does not weaken that boundary.

## Main-Controller MIDI Service

USB MIDI input must remain responsive regardless of LED queue occupancy. The
main loop will service up to eight complete USB MIDI messages per pass before
input scanning and feedback output. The service function will not inspect
`fbItemsToSend` or `SendingData()`.

Eight messages per pass bounds the time spent in MIDI callbacks while matching
the existing maximum of eight feedback frames emitted per pass. Calling
`MIDI.read()` repeatedly also drains the USB transport ring faster than the
single-message implementation without reintroducing interrupt-context
callbacks.

The 7 kHz timer remains dedicated to low-latency DIN
Clock/Start/Continue/Stop forwarding.

## Stale Aux-Show Recovery

The main controller will treat an aux show as stale after 250 milliseconds.
This is longer than two aux `SHOW_END` refresh periods and comfortably longer
than the maximum expected NeoPixel transmission. The main loop will clear a
stale `fbShowInProgress` before servicing MIDI or feedback.

`antMicrosAuxShow` will be volatile because it is written by the aux receive
callback and read by the main loop. Receiving `RESET_HAPPENED` will also clear
show and acknowledgement state so an aux reset cannot preserve stale protocol
state.

This timeout is recovery, not normal flow: a valid `SHOW_END` continues to
clear the flag immediately.

## Aux Feedback Scheduling

`feedbackDataUpdate()` will process at most eight queued frames per call. The
existing atomic claim-and-copy remains: each frame is removed under a short
interrupt-disabled section and all LED calculations happen after interrupts
are restored.

The bound guarantees that the aux superloop regularly reaches
`feedbackShow()`, command flags, and periodic `SHOW_END` emission even when
feedback arrives continuously.

## Aux Queue Overload

When the 128-frame queue is full, the receive ISR will discard the oldest queued
frame, accept the newest valid frame, and continue the burst normally. The
pending count stays at 128. The burst is ACKed at `BURST_END`, allowing the main
controller to retire its transmitted updates instead of retrying indefinitely.

This policy is deliberately lossy under overload but preserves the newest LED
state, bounds memory, and is self-damping. It sends no multi-byte error response
from the full-queue ISR path.

The same behavior is valid for burst and individual frames: individual frames
are accepted after making capacity and receive their normal ACK.

## Show/Burst Exclusion

The aux may call `feedbackShow()` only when neither a burst nor an individual
frame is being received:

```c
!receivingBank && !receivingFeedbackData
```

This prevents `SHOW_IN_PROGRESS` from being asserted between frames of a burst
and removes the main controller's silent mid-burst frame-loss path.

## Error Handling

- Checksum and malformed-frame failures retain the existing retry protocol.
- Queue capacity is no longer reported as a checksum failure.
- Stale show state recovers locally without resetting either controller.
- USB MIDI parsing continues even if the LED queue is saturated or the aux is
  recovering.

## Verification

The repository has no unit-test harness for these embedded protocol paths. Per
project direction, this pass will not invent one. Each behavior change will be
verified by its narrow firmware build and source-level invariant checks, then
the combined result will be verified with `make all`.

The controller may be flashed only after the complete build is reviewed. The
final branch diff will receive a broad Claude Opus review through xagent,
covering correctness, concurrency, protocol behavior, memory safety, and
maintainability rather than only the reported symptom.

## Commit Boundaries

1. Keep USB MIDI responsive during LED backlog.
2. Recover stale aux show state.
3. Bound aux feedback processing.
4. Make aux queue overload self-damping.
5. Prevent NeoPixel shows during feedback bursts.

Each firmware behavior change is committed independently.
