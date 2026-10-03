# USB MIDI receive after upstream reconnect

Implemented on base `a2468bb5e3330648827afd97f7a7722b9a577259` (the `joyo` checkout) in branch `codex/wrld-usb-reset`.

## Defect and change

A host USB reset clears non-control endpoint configuration and endpoint interrupt enables/flags. The old `initEP()` restored bulk IN every time but configured bulk OUT only when allocating its handler. Handler pointers survive in SRAM, so the second configuration skipped OUT entirely. This independently matches [Microchip DS40001882G §32.6.2.4, printed page 696](https://ww1.microchip.com/downloads/en/DeviceDoc/SAM-D21DA1-Family-Data-Sheet-DS40001882G.pdf). [Arduino upstream](https://github.com/arduino/ArduinoCore-samd/blob/master/cores/arduino/USB/USBCore.cpp) recreates the handler; this older fork instead needs to retain its two malloc-owned buffers, which have no destructor.

`DoubleBufferedEPOutHandler::reset()` now disables OUT while replacing its receive state, clears both buffers' counters/readiness and the backpressure notification, restores the descriptor, acknowledges old completion, initializes DATA0, and rearms OUT. Existing handlers are reset on every configuration without replacement or new allocation. First configuration retains the existing allocation behavior. The explicit OUT toggle clear also prevents losing the first packet after repeated configuration without a bus reset: disabling an endpoint does not promise to reset that toggle (§32.6.2.2; W1C operation in §32.12.2). [TinyUSB's SAMD endpoint-open implementation](https://github.com/hathach/tinyusb/blob/master/src/portable/microchip/samd/dcd_samd.c) uses the same explicit toggle initialization.

The complete `recv()` and `available()` operations use the core's existing PRIMASK guard. This prevents the USB reset/configuration ISR from changing indexes during a copy or count calculation. The copy is bounded by one 256-byte buffer (normal MIDI reads copy four bytes). Nested guards preserve an already-disabled interrupt mask. IRQ latency on hardware has not been measured. USB DMA can continue filling the other buffer during a guarded read; reset disables the endpoint before rewriting its descriptor.

MIDIUSB now returns an empty packet if `accept()` did not fill its ring. Previously a reset between `USB_Available()` and `USB_Recv()` could expose an old ring slot, including repeatedly while unconfigured. The MIDIUSB ring, transport ring and MIDI parser were reviewed: queued complete events and partial SysEx can survive reconnect, but they drain or reach the parser's 512-byte limit. This patch does not define new session-wide queue purging behavior. Some finite stale feedback or loss of a new prefix after an interrupted SysEx remains possible; none of these queues explains a permanently disabled OUT endpoint.

## Verification

Run from this checkout:

```sh
python3 tools/test_usb_reset.py
python3 tools/test_quarter_step_table.py
```

The USB runner compiles the actual guard/handler, endpoint initialization, receive APIs, and MIDIUSB accept/read source against a host model using Clang with ASan and UBSan. Seventeen cases cover cold startup, reset/configuration, partial and full buffers, pending completion, 1,000 repeated configurations with stable allocations, ZLP, 256-byte transfers, MIDI ring wrap, DATA0 after odd/even prior packet counts, selected interrupt boundaries, existing PRIMASK, and stale availability/empty-ring reads.

The original source passed cold startup and failed 13 of the initial 14 cases. A reset-only implementation still failed the receive interrupt race and three MIDI stale-read cases. The toggle regression failed before adding the explicit clear. All 17 final cases and the three existing encoder checks pass. Independent review found the toggle issue; follow-up review confirmed its correction and reported no further findings.

This is a source-based register-admission model with selected deterministic IRQ interleavings, not a USB wire simulator, an exhaustive concurrency proof, or hardware validation. It does not execute the full EP0 control-request ISR, model transaction timing/analog behavior, or prove which firmware is currently flashed.

## Build and provenance

Use the checkout itself as the Arduino sketchbook so its `hardware/` platform and bundled libraries are selected. Plain `make main` can otherwise select the separately installed core.

```sh
artifact_dir="$(mktemp -d /private/tmp/ytx-usb-build.XXXXXX)"
ARDUINO_DIRECTORIES_USER="$PWD" arduino-cli compile \
  --clean --verbose --fqbn yaeltexv2:samd:kilomuxv2-main \
  --build-path "$artifact_dir/build" --output-dir "$artifact_dir/output" \
  ytx-main-controller > "$artifact_dir/build.log" 2>&1
```

Both baseline and final MAIN builds succeeded with Arduino CLI 1.4.1 and the installed ARM GCC 7-2017q4 toolchain. Compiler commands and `.d` dependencies selected this worktree's `USBCore.cpp`, `SAMD21_USBDevice.h`, and `MIDIUSB.cpp`. Linked ARM disassembly contains the reset call on the existing-handler branch, register restoration, and PRIMASK/cpsid/conditional cpsie/ISB guards. Primary checkout and installed-core hashes still match the base commit.

| Measurement | Baseline | Final |
| --- | ---: | ---: |
| CLI reported program bytes | 110,628 | 110,780 |
| ELF `.data` bytes | 448 | 448 |
| ELF `.bss` bytes | 15,360 | 15,360 |

The one macro-whitespace warning (`ARDUINO_KILOMUXV2-MAIN`) occurs in both builds. Raw logs, binaries, maps, disassembly and `final-provenance.json` remain local at `/private/tmp/wrld-usb-reset-build/`.

Trial image: `/private/tmp/wrld-usb-reset-build/final-output/ytx-main-controller.ino.app.bin`

SHA-256: `96579c17b96c7b5bc6e61fc162d8d642799a3d3256406a2ee631c73fee8ddb53`

No firmware was flashed. No controller/iPad/app restart or USB-connection change was performed. AUX firmware and the globally installed Arduino core were not changed.

## Physical trial still required — separate approval

1. Preserve the currently failed setup and existing diagnostics until approval to end that observation. Record the trial image hash when approval is given.
2. Under the separately approved deployment procedure, install the MAIN image, then establish a working baseline: controller controls reach the iPad, and known iPad feedback messages change the expected WRLD indicators. Keep the app and powered hub configuration consistent with the reproduction.
3. With WRLD continuously powered, unplug and reconnect **only the iPad-to-powered-hub cable**. Do not restart the app or power-cycle WRLD to recover. After ports reopen, verify both directions, including a known feedback message and a sustained feedback sequence.
4. Repeat at least ten times, including idle and active MIDI traffic. Record reconnect times, successful feedback receipt, and host OUT completion errors. Success requires recovery on every cycle without an app restart or WRLD power cycle, and absence of the previous sustained OUT-error condition.
5. If the fault returns, preserve that state and collect new app/host evidence before any additional restart. Host “pipe stalled” is an OS completion status, not proof of a literal USB STALL handshake. Maya audio faults remain a separate investigation.

The local code defect is established and covered by regression tests. Only this physical trial can establish whether the patch resolves the observed WRLD/iPad failure.
