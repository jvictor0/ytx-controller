# WRLD USB OUT reset implementation plan

**Goal:** Restore host-to-controller MIDI after a host bus reset followed by configuration, without rebooting WRLD.

**Authorized scope:** Implement and test on `a2468bb5e3330648827afd97f7a7722b9a577259` in the managed worktree. Do not flash, restart devices/apps, change USB connections, edit the primary checkout, or change the installed Arduino core.

**Design:** Add an in-place `DoubleBufferedEPOutHandler::reset()` called by its constructor and by `initEP` when the handler already exists. Quiesce OUT, discard both receive buffers' indexes/readiness/notification state, restore its descriptor and bulk type, acknowledge stale completion, clear the OUT data toggle for DATA0, and rearm. Reuse both allocated buffers. Protect the complete handler `recv` and `available` operations with the existing PRIMASK-preserving guard so reconfiguration cannot reset indexes while a reader is using them. MIDIUSB must recheck its ring after attempting an accept; an empty ring returns a zero packet even when a preceding availability check succeeded.

Reconstructing the handler would allocate in the USB ISR and leak its owned malloc buffers without additional lifetime changes. A reset-generation protocol could avoid the bounded critical section, but would add state and retries to a maximum 256-byte copy. Prefer the existing guard; verify emitted ARM code and preserve the previous interrupt mask.

## Implementation and verification

- [x] Add `tools/test_usb_reset.py` and `tools/usb_reset/` C++ fixtures. Compile verbatim handler/guard, endpoint initialization, receive APIs, and MIDIUSB read/accept source against a small register-admission model. Require packet reception after reset; test partial/two-full buffers, zero-length transfers, repeated configuration, stable allocations, masked interrupts, and reset injections at critical-section boundaries. Require empty MIDI reads when availability becomes stale.
- [x] Run on the unmodified sources and record expected failures before editing firmware.
- [x] Implement reset/rearm in `hardware/yaeltexv2/samd/cores/arduino/USB/SAMD21_USBDevice.h`; invoke it in `USBCore.cpp`; fix the empty-ring read in `libraries/MIDIUSB/src/MIDIUSB.cpp`.
- [x] Run the host tests with sanitizers plus the existing quarter-step test. Review higher-layer queues and document remaining limits.
- [x] Build baseline and patched MAIN using `ARDUINO_DIRECTORIES_USER="$PWD" arduino-cli compile --clean --verbose --fqbn yaeltexv2:samd:kilomuxv2-main` with separate temporary build/output directories. Verify compiler input paths, dependency files, source hashes, and linked ARM reset/guard instructions.
- [x] Obtain independent code review, resolve findings, commit code/tests/findings, and retain local artifacts for a separately approved hardware trial.

The host model covers source behavior and selected deterministic interrupt interleavings. It is not a wire simulator, exhaustive concurrency proof, or physical-device validation. The required trial is repeated upstream iPad-to-powered-hub cable reconnects while WRLD stays powered, with bidirectional MIDI verified each time and OUT errors compared to the original failure.

Review found that endpoint disable does not guarantee OUT toggle initialization. Added an explicit W1C toggle clear plus odd/even packet-count regression; the new case failed before the correction and passed afterward. Independent follow-up review reported no further findings.
