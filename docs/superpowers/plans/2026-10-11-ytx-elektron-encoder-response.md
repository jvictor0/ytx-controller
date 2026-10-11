# YTX Elektron-inspired encoder response implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. The user has already requested implementation in a separate Codex chat; proceed with that handoff without another design-approval round.

**Goal:** Give WRLD.BLDR's endless pots one tunable, controller-side acceleration profile inspired by Digitone II filter cutoff, transmitting normalized relative movement over an analytic exponential MIDI encoding.

**Architecture:** MAIN firmware aggregates the existing endless-pot direction reports into short time frames, applies per-physical-knob acceleration and decay, and encodes the resulting movement. SmartGrid decodes the movement and passes it directly into its existing normalized parameter-increment operation. An opt-in rotary mode and distinct MIDI channel preserve the existing controller protocol.

**Tech Stack:** YTX SAMD21 Arduino C++11 firmware, existing SPIEndlessPot modules, USB MIDI CC, Python unittest and host C++ tests.

## Global constraints

- One response profile for all parameters; no parameter-specific coefficients or pressed-turn behavior.
- MIDI value 64 is ignored entirely. Value 63 is the smallest negative movement; 65 is the smallest positive movement; 0 and 127 are the largest negative and positive single-message movements.
- Decode with an exponential function of `abs(value - 64)`. No maintained or generated lookup table for this new codec or acceleration profile.
- MAIN firmware owns acceleration, timing, reversal handling, and motion quantization. SmartGrid owns parameter values, limits, selector quantization, gestures, and feedback.
- Acceleration surviving SmartGrid page changes is acceptable. Do not invent a page-change handshake.
- Target a tunable approximation. Do not describe the proposed defaults as recovered cutoff constants or an exact Elektron emulation.
- This implementation chat changes YTX only. The companion SmartGrid requirements below are a separate deliverable, required before enabling the new profile for normal use.
- Do not flash firmware, upload controller configuration, send diagnostic SysEx, or deploy SmartGrid as part of implementation. Produce the build and configuration artifacts for review.
- Follow repository instructions. For new C++ use structs with public members, `m_` members, `x_` constants, HammerCase methods, matched braces on new lines, explicit arguments, and C++ casts. New comments end with a separate `//` line. Do not broadly reformat old code.
- Keep `main` and the existing YTX checkout clean; implement in a managed worktree. The inspected YTX checkout is on `joyo`, HEAD `3cc49d6afb906c75c78cca4338bf96ac03a7eff6`. It contains important recent USB fixes and unrelated untracked binaries. Use this inspected revision as the implementation base rather than losing those fixes. The remote default ref is not configured locally; pass the explicit ref to the managed worktree tool.
- The user's infrastructure stop rule refers to Codex itself. Ordinary compiler, Ghidra, or local build problems can be investigated normally.

## Evidence and limits

The previous investigation is saved at:

`/Users/joyo/.codex/visualizations/2026/09/13/01a0988e-b6f2-75f1-962d-dc04ab76622e/digitone-encoder-research/digitone-ii-encoder-math-1.10D.md`

Its companion `encoder-range.c`, `encoder-pass2.c`, `profiles.c`, and `timer-range.c` contain the inspected assembly/decompilation. This is static reconstruction from MAIN OS 1.10D, SHA-256 `8caf761317ec25848e47bd6c6907188e8d4de180e744a350fd8d4df8e5715a3f`. Its attempted isolated instruction-emulation verification did not run. Do not claim a passing verification from that attempt.

The reconstruction supports signed acceleration state, reset on reversal, output using the previous acceleration state, and decay `a = trunc(2*a/3)`. One continuous built-in profile has `(k, divisor, step) = (18, 1, 1)`. The parameter caller selects a behavior and applies additional input scaling. Assignment of that profile to cutoff, physical input units, and timer duration remain unproven. Timer registration argument 10 means every third parent dispatch, not a proven 10 ms period. The [official manual](https://www.elektron.se/wp-content/uploads/2025/10/Digitone-2-User-Manual_ENG_OS1.10D_251022.pdf) does not specify these acceleration constants.

Exact reconstruction would require tracing cutoff's parameter descriptor, its input scale, upstream event grouping, and the timer's real frequency. The user explicitly accepts tuning the feel, so those are not prerequisites for this implementation.

The existing module firmware (`modules/SPImoduleEndlessPot/analog.ino` and `SPImoduleEndlessPot.ino`) smooths two analog tracks and reports activity plus direction, not a magnitude counter or timestamped queue. MAIN cannot recover discarded movement. Start with the deployed module protocol; MAIN can still approximate acceleration from the density of reported activity. Revisit module firmware only if hardware evaluation demonstrates lost movement or a speed plateau that tuning cannot fix. Do not silently claim this is angle-accurate.

## Response contract

Five initial feel controls, all centralized in one configuration struct:

| C++ constant | Initial value | Meaning |
| --- | ---: | --- |
| `x_baseStep` | `1.0 / 1024.0` | Normalized movement per reported pulse at minimum gain |
| `x_frameUs` | `8000` | Same-direction pulse aggregation interval |
| `x_accelerationCoefficient` | `18` | Acceleration increment coefficient |
| `x_decayUs` | `10000` | Time between decay operations |
| `x_maxGain` | `16` | Maximum magnitude of acceleration state |

Only the coefficient comes from a recovered candidate profile; its use for cutoff is unverified. The other values are engineering starting points. Keep the reconstructed `2/3` decay ratio and integer division by 8 fixed initially. Varying the decay interval is enough to tune persistence without introducing another feel control. The reconstructed movement gate is deliberately omitted: its units are unknown, the pot modules already apply hysteresis, and copying the gate with guessed input units could swallow slow motion.

Per physical knob maintain signed integer acceleration `a`, signed frame pulse sum `q`, last direction, frame/decay timestamps, and codec remainder. Frames align relative to that knob's initialized clock. Process all elapsed frame and decay boundaries in time order; decay runs before frame output when timestamps tie. A pulse exactly on a frame boundary belongs to the next frame. Use unsigned elapsed-time arithmetic that survives `micros()` rollover; an explicit initialized flag avoids treating timestamp zero as uninitialized.

At a nonempty frame boundary:

```text
movement = baseStep * q * max(1, abs(a))
if abs(q) > 1 or abs(a) > 1:
    a = clamp(a + trunc(accelerationCoefficient*q/8), -maxGain, maxGain)
q = 0
emit movement through the codec
```

At each decay boundary, `a = trunc(2*a/3)`. Empty frames and decay alone generate no movement. Unit pulses in separate frames stay at minimum gain. Multiple pulses in a frame seed acceleration; subsequent motion receives the increased gain. The cap is an engineering bound absent from the recovered function.

On direction reversal, first advance due boundaries and flush the old direction's incomplete frame at its current gain. Drain that frame's representable codec movement, then clear acceleration before collecting the new direction. This preserves actual prior motion without carrying its gain into the reversed gesture. A sub-minimum residual can remain as signed numerical error; it must never generate an opposite-direction correction packet by itself. No release, switch, or press state alters this response.

For core recurrence tests without elapsed decay, inputs `q = 1, 2, 2, 2, -2` produce movement in base-step units `1, 2, 8, 16, -2` and acceleration `0, 4, 8, 12, -4`. Decay from 12 gives `8, 5, 3, 2, 1, 0`. Hardware-clock tests separately exercise framing and decay together.

Long idle periods must not cause loops over millions of empty frames: settle the bounded acceleration decay, then skip empty time to the current boundaries. No heap allocation, blocking delay, or USB-arrival timing in the response path.

## Wire contract: exponential relative v1

Use ordinary three-byte CC messages. The WRLD.BLDR profile sends CC 0–15 on **MIDI channel 9**, represented as channel **8** in the zero-based configuration and SmartGrid parser. Buttons stay on their existing channel; ring/value feedback stays on channel 1 (zero-based 0). Channel 9 currently has no WRLD.BLDR input mapping in SmartGrid, so an old receiver ignores this protocol instead of interpreting its values linearly.

Two codec constants are fixed protocol values, not ordinary feel controls:

```text
minimumDelta = 1 / 65536
codesPerDoubling = 8

if value == 64: ignore the entire message
z = int(value) - 64
magnitude = minimumDelta * expm1(log(2) * abs(z) / codesPerDoubling)
                            / expm1(log(2) / codesPerDoubling)
delta = sign(z) * magnitude
```

Normalized delta refers to the full `[0,1]` parameter range, not Hz, MIDI absolute value, or selector index. Both sides must agree on these two constants; future feel tuning changes the five firmware response controls, not the codec.

| Value | Decoded normalized delta |
| ---: | ---: |
| 0 | -0.042990704731587 |
| 1 | -0.039408657687819 |
| 32 | -0.002528864984211 |
| 56 | -0.0001685909989474 |
| 63 | -0.0000152587890625 |
| 64 | ignored |
| 65 | +0.0000152587890625 |
| 72 | +0.0001685909989474 |
| 96 | +0.002528864984211 |
| 127 | +0.039408657687819 |

There are 64 negative codes and 63 positive codes. Thus 0 is slightly larger in magnitude than 127. This is intentional: no alias and no second meaning for 64. Splitting large deltas into several messages preserves symmetric total movement.

For firmware encoding, select the largest decodable magnitude no greater than the remaining movement. Invert the equation analytically:

```text
candidate = floor(codesPerDoubling * log2(
    1 + abs(remaining) * expm1(log(2)/codesPerDoubling) / minimumDelta))
candidate = clamp(candidate, 0, remaining > 0 ? 63 : 64)
```

Correct a possible adjacent-code floating-point rounding error by evaluating the decoder at the candidate and its neighbor. Zero candidate means retain the sub-minimum remainder and send nothing. Otherwise send `64 + sign(remaining)*candidate`, subtract its decoded delta, and continue. Never emit 64. Use `double` for remainder accounting and ordinary libm functions; no maintained tables.

Retain signed sub-minimum error across motion events so repeated fine movement is conserved. Drain each accepted response movement before accepting an opposite movement; do not cancel two representable opposing gestures inside a single remainder. The existing scan guard allows one read per distinct `millis()` bucket, which is not the same as a full 1000 microseconds between reads. An 8 ms frame can intersect nine such buckets. With the gain cap this bounds a frame to `9 * 16 / 1024 = 0.140625` normalized movement, plus a sub-minimum codec residual. Derive this pulse bound from the configured frame duration if tuning it. Establish the maximum packet count for this bounded movement in the codec test and document it; do not truncate or discard movement to meet a guessed packet budget. For the initial constants, 12 iterations is a conservative analytical ceiling: at most three full-size positive chunks followed by a rapidly shrinking remainder (the negative maximum is larger). Check that bound with dense tests, and derive it again if tuning changes the frame maximum. Reject nonfinite/out-of-contract inputs explicitly in the host harness; the firmware input adapter only supplies bounded physical frames. Do not accept an unbounded firmware loop.

## Files and responsibilities

YTX paths below are relative to its new worktree, not this SmartGrid worktree.

- Create `ytx-main-controller/headers/ExponentialRelativeCodec.h`: standalone formula, inverse, remainder-aware bounded packet emission.
- Create `ytx-main-controller/headers/EncoderResponse.h`: standalone per-knob response state and clock handling, with no Arduino or MIDI dependency.
- Modify `ytx-main-controller/headers/types.h`: add opt-in rotary mode value 6 without changing packed layout or existing enum numbers.
- Modify `ytx-main-controller/headers/EncoderInputs.h`, `ytx-main-controller/encoders.ino`, and `ytx-main-controller/loop.ino`: state allocation, raw direction routing, fair sampling, timer service, explicit MIDI emission.
- Create `tools/encoder_response/cases.cpp` and `tools/test_encoder_response.py`: compile and exercise the actual firmware helper headers and relevant integration seams.
- Create `tools/make_exponential_encoder_profile.py`, `tools/test_exponential_encoder_profile.py`, and `configs/WRLD.BLDR.exponential-v1.ytx`: reproducible opt-in configuration, without uploading it.
- Create `docs/encoder-response.md` and `tools/encoder_response/protocol-v1-vectors.json`: protocol contract, tunable constants, evaluated examples, compatibility, hardware evaluation instructions.
- Copy this plan to `docs/superpowers/plans/2026-10-11-ytx-elektron-encoder-response.md` in YTX for progress tracking. Do not modify the original SmartGrid source during the YTX implementation.

### Task 1: Implement and verify the analytic wire codec

**Files:** `ExponentialRelativeCodec.h`, `tools/encoder_response/cases.cpp`, `tools/test_encoder_response.py`, `protocol-v1-vectors.json`.

**Interfaces:** Public struct `ExponentialRelativeCodec` exposes `static double Decode(uint8_t value)`, `static uint8_t EncodeFloor(double magnitude, bool positive)`, `void Reset()`, and `template<typename Sink> void Add(double delta, Sink& sink)`. `Decode(64)` returns zero for arithmetic/testing convenience, but receiver parsing still ignores 64 entirely. `EncodeFloor` returns 64 when the magnitude is below minimum; `Add` never forwards it. `Sink` provides `void Emit(uint8_t value)`. A new codec object starts with zero remainder; the signed remainder is public `double m_remainder`.

- [ ] Add a host C++ runner using `clang++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined`, following the existing USB-test runner's subprocess/temp-directory pattern. Compile actual headers rather than a duplicated C++ model. The first run, `python3 tools/test_encoder_response.py`, should fail because the codec does not yet exist.
- [ ] Add exact boundary and qualitative assertions. Example test body:

```cpp
assert(ExponentialRelativeCodec::Decode(64) == 0.0);
assert(ExponentialRelativeCodec::Decode(65) == 1.0 / 65536.0);
assert(ExponentialRelativeCodec::Decode(63) == -1.0 / 65536.0);
assert(ExponentialRelativeCodec::Decode(0) < ExponentialRelativeCodec::Decode(1));
assert(ExponentialRelativeCodec::Decode(127) > ExponentialRelativeCodec::Decode(126));
```

- [ ] Implement the analytic decoder, inverse, adjacent-code correction, and signed remainder as specified above. Add a sink fixture that accumulates decoded emitted movement and records every byte.
- [ ] Test every code and every interval boundary: monotonicity, sign, positive/negative symmetry for magnitudes 1–63, inverse at each decoded value and immediately to either side, no emitted 64, and no byte outside 0–127. Independently calculate expected values in Python using `math.expm1`, not by invoking the C++ decoder as its own oracle. Use a tight numerical tolerance for transcendental calculations rather than requiring every platform to have bit-identical libm results.
- [ ] Exercise repeated fractions smaller than the minimum, repeated base-step movements, alternating signs, zero input, and full frame movements up to ±0.140625. For each drain assert conservation `sum(decoded output) + remainder == sum(input)` within floating-point tolerance, `abs(remainder) < minimumDelta`, and packet signs consistent with the motion being drained. Exhaustively test all `q = -9..9` and gains `1..16` with residuals at zero and just below both minimum-delta boundaries; supplement with a deterministic dense sweep across the bounded range. Record the observed maximum and verify the conservative termination bound from the shrinking residual.
- [ ] Save only compact test vectors as JSON for the future SmartGrid receiver. These are tests, not a runtime lookup table. Run the runner successfully and commit this independently usable codec.

### Task 2: Implement per-knob response and deterministic timing

**Files:** `EncoderResponse.h`, `tools/encoder_response/cases.cpp`, `tools/test_encoder_response.py`.

**Interfaces:** Public `EncoderResponseConfig` contains the five `x_` constants above. Public struct `EncoderResponse` exposes `void Reset(uint32_t nowUs)`, `template<typename Sink> void Pulse(int8_t direction, uint32_t nowUs, Sink& sink)`, `template<typename Sink> void Advance(uint32_t nowUs, Sink& sink)`, and `template<typename Sink> void Flush(uint32_t nowUs, Sink& sink)`. `Pulse` accepts exactly -1 or +1. Sink supplies `void AddMovement(double delta)`; the hardware adapter feeds that method into `ExponentialRelativeCodec::Add`. `Flush` first advances due boundaries, then emits the incomplete frame without inventing a decay tick. Include a small public `ApplyFrame(int32_t q)` method returning the normalized movement and updating acceleration for direct recurrence tests.

- [ ] Add failing tests for the recovered recurrence example and decay sequence before implementing the response struct. Example recurrence check after reset, with no clock service between calls:

```cpp
assert(response.ApplyFrame(1) == 1.0 / 1024.0);
assert(response.ApplyFrame(2) == 2.0 / 1024.0);
assert(response.ApplyFrame(2) == 8.0 / 1024.0);
assert(response.ApplyFrame(2) == 16.0 / 1024.0);
assert(response.ApplyFrame(-2) == -2.0 / 1024.0);
```

- [ ] Implement signed truncation, reversal reset, previous-gain output, cap, and empty-frame silence. `ApplyFrame` must also reset acceleration on a q/a sign mismatch so its standalone recurrence contract is correct.
- [ ] Implement the chronological frame/decay service, including tie ordering, direction-change partial-frame flush, initialization at timestamp zero, and unsigned wraparound. No MIDI timestamps participate. If `Pulse` arrives after a long gap, settle previous due work before adding that pulse.
- [ ] Add traces proving isolated pulses remain at base gain, dense same-direction pulses accelerate, a pause restores fine control, reversal starts at base gain, and idle service emits no spontaneous motion. Run the same timestamped pulse trace with frequent versus delayed `Advance` calls and assert identical deltas/timing at logical boundaries. Include wrap at `UINT32_MAX`, a long idle interval, multiple independent knobs, and a bounded-state saturation trace.
- [ ] Exercise response plus codec as a pipeline and assert signed movement conservation. Document the per-knob state size and bounded work in the test output. Run the runner and commit the response helper.

### Task 3: Integrate the new opt-in mode into MAIN

**Files:** `headers/types.h`, `headers/EncoderInputs.h`, `encoders.ino`, `loop.ino`, plus the integration test fixtures under `tools/encoder_response/`.

**Interfaces:** Add `rot_rel_exponential = 6` to the existing `rotaryModes` enum. Add explicit methods `bool IsExponentialEncoder(uint8_t encNo)`, `void ServiceExponentialEncoders(uint32_t nowUs)`, `void ResetExponentialEncoders(uint32_t nowUs)`, and `void SendExponentialRotaryMessage(uint8_t encNo, uint8_t value)`. Store response and codec state per physical encoder, outside `encoderBankData`. Integration sink adapters expose the exact `AddMovement` and `Emit` contracts from Tasks 1–2 and carry the physical encoder index.

- [ ] Add failing integration tests with fake clock/SPI/MIDI sinks for: new mode bypasses existing acceleration and prescaling; legacy mode still reaches its existing path; 64 is never sent; startup dump and switch-generated rotary sends produce no exponential movement; configured channel and CC are honored; host feedback does not emit a movement event. Exercise actual integration methods or small extracted production helpers, not assertions against source text alone.
- [ ] Add enum value 6 without growing `rotBehaviour.hwMode : 3`. Preserve values 0–5 and the serialized configuration layout. Search all mode checks so unsupported new-mode configurations cannot fall through to an absolute/zero-valued rotary message. New mode requires endless-pot E41H/E41V hardware and ordinary CC; reject unsupported rotary message/module combinations for this mode instead of interpreting them as legacy absolute mode.
- [ ] Allocate per-physical response/codec state through the existing `memHost->AllocateRAM` initialization path. Include its real size in the existing RAM budget check and initialize every object explicitly. No per-bank duplication or runtime allocation. Keep existing feedback-value state for incoming LED/ring values.
- [ ] In `DecodeRotaryDirection`, new mode consumes each reported activity bit with prescaler 1 regardless of legacy `speed`, `encFineAdjust`, or pressed state. In `EncoderAction`, intercept new mode before the existing speed-table logic, call `Pulse(direction, micros(), sink)`, and return. Do not store encoded values in `nextJump`, which is only a four-bit field.
- [ ] While any configured encoder uses the new mode, read all configured encoder modules once on every existing 1 ms scan. Bypass the current priority-list shortcut, which can stop polling other modules when two are active. Avoid double-reading priority modules. Keep the old scan policy when the new mode is absent. Test three simultaneously active modules, not only one knob.
- [ ] Call `ServiceExponentialEncoders(micros())` beside `encoderHw.Read()` while input processing is enabled, including iterations where `Read` returns for its minimum scan interval. Reset pending state when processing is disabled/reconfigured so resumption cannot release stale movement. Guard startup/uninitialized/no-encoder paths. A host page change does not touch this state.
- [ ] Implement a separate CC emitter using configured `rotaryConfig` CC, MIDI port bits, and one-based MIDI-library channel. Its essential send operation is:

```cpp
MIDI.sendControlChange(parameter, value, channelZeroBased + 1);
```

  Respect the USB bit for that call and the DIN bit for `MIDIHW.sendControlChange`. The supplied profile enables USB only. Return immediately for 64. Never update a guessed absolute parameter or local ring based on the encoded value.
- [ ] Add early exits for new mode to both old `SendRotaryMessage` and `SendRotaryAltMessage`. Only the new response sink can emit new-mode movement; init dumps, buttons, alt/shift actions, and direct value feedback cannot accidentally send 0. Buttons continue their existing separate messages. On a controller firmware bank/configuration route change, flush current motion to the old route before changing pointers, then reset response and codec state; this is distinct from SmartGrid page changes.
- [ ] Run the new host tests, `python3 tools/test_quarter_step_table.py`, `python3 tools/test_usb_reset.py`, and `make main`. Fix failures and commit integration. Report compiler RAM/flash usage and actual per-knob state cost. Do not run `make install`.

### Task 4: Generate the opt-in WRLD.BLDR profile and publish the contract

**Files:** `tools/make_exponential_encoder_profile.py`, `tools/test_exponential_encoder_profile.py`, `configs/WRLD.BLDR.exponential-v1.ytx`, `docs/encoder-response.md`.

**Interfaces:** CLI `python3 tools/make_exponential_encoder_profile.py INPUT OUTPUT` writes a separate JSON `.ytx`; refuse identical input/output paths. Use Python's standard library only. No device connection or message transmission.

- [ ] Add failing tests for transformation, source preservation, rejection of malformed/unsupported rotary configurations, and every unchanged configuration field. Validate the WRLD.BLDR source has 16 encoders, CC 0–15, supported endless-pot hardware, ordinary CC rotary messages, and compatible button modes before producing the profile.
- [ ] Implement a deep-copy transformation of each bank's encoder entries. These are the only three changed leaf values per encoder:

```python
entry['encoder_mode']['hw_mode'] = 6
entry['rotary_config']['channel'] = 8
entry['rotary_config']['midi_port'] = 1
```

  Leave speed fields, min/max, switch settings, channel-0 rotation feedback, hardware identifiers, and all non-encoder controls intact. USB-only rotation avoids saturating DIN with the fine delta stream. Require valid CC numbers rather than silently masking incorrect configuration. Do not open/save the new mode through an older editor that might coerce an unknown mode.
- [ ] Generate `configs/WRLD.BLDR.exponential-v1.ytx` from the read-only source `/Users/joyo/.codex/worktrees/f8f7/theallelectricsmartgrid/patches/WRLD.BLDR.ytx`. Record the source SHA-256 and transformation command in the documentation. Confirm exactly 48 changed leaves for its single bank of 16 encoders; the source remains unchanged.
- [ ] Write `docs/encoder-response.md` containing the equations, all constants and example vectors above, channel numbering, opt-in/rollback procedure, distinction between response and codec tuning, compatibility matrix, state/memory and packet bounds, and the unverified Elektron provenance. State that the old profile continues to work with the new firmware; the new profile requires a new SmartGrid receiver; old SmartGrid ignores its rotation channel. Do not imply the host application has already been changed.
- [ ] Include this hardware evaluation checklist for a later authorized deployment: compare slow full-range travel and micro-adjustments first; then medium/fast sweeps; stop/restart; immediate reversal; three simultaneous knobs on different modules; rapid SmartGrid page changes; boundary clamping; a discrete selector; ring feedback; startup dump; and USB disconnect/reconnect. Tune base sensitivity first, then grouping and acceleration coefficient, then decay, then maximum gain. Keep both codec constants fixed.
- [ ] Run `python3 tools/test_exponential_encoder_profile.py`, the response suite, existing quarter-step and USB suites, `make main`, and `git diff --check`. Commit artifacts and documentation. Report the branch/worktree, exact test results, build path, and remaining hardware evaluation. Stop with reviewable artifacts; do not flash or change the live configuration.

## Required companion SmartGrid change (handoff contract, not YTX implementation scope)

Yes, a small receiver change is necessary. Current `EncoderMidi::FromMidi` subtracts 64 on channel 0, and `Encoder::HandleIncDec` adds its own timing-dependent acceleration. Neither is appropriate for an already accelerated exponential delta.

1. Add `MessageIn::Mode::EncoderExponentialDelta` in `private/src/MessageIn.hpp`, carrying the raw encoded byte in its existing integer `m_amount` field. Add the standalone v1 decoder with identical constants and shared test vectors.
2. In `private/src/WrldBLDRMidi.hpp`, recognize CC 0–15 on zero-based channel 8, route to the existing encoder grid position, and ignore value 64 before producing any event. Do not globally reinterpret other devices' channel 8 messages.
3. In `EncoderBank::Apply` in `private/src/EncoderBank.hpp`, look up the visible `BankedEncoderCell`, decode the byte, and call `cell->Increment(delta)` directly. Do not call `HandleIncDec`, `SetToValue`, or `IncrementInternal`. `Increment` preserves the existing gesture/scene behavior while avoiding the old acceleration layer.
4. Keep legacy channel-0 `EncoderIncDec` processing, buttons, feedback, and all parameter mappings unchanged. Continuous parameters receive normalized deltas; discrete selectors retain their existing normalized accumulation and rounding.
5. Verify all 128 values, invalid CC range, no-event 64, existing controller compatibility, identical results under different MIDI packet arrival timing, continuous and discrete parameters, gesture/scene handling, and feedback. MIDI callback/transport changes and SysEx negotiation are unnecessary.

## Completion criteria

- New mode and generated configuration exist, all host/regression tests pass, and MAIN compiles.
- Existing modes keep their protocol and response; the new mode never accidentally sends motion on dump/button/feedback paths.
- Every emitted movement is controller-accelerated, analytically encoded, bounded, and accounted for to sub-minimum residual accuracy.
- The handoff states what is built versus what remains: SmartGrid receiver integration and physical feel calibration are still needed. No claim of a verified Elektron cutoff match or hardware success without measurements.
