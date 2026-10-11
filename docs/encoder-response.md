# Exponential encoder response v1

This opt-in implementation changes MAIN firmware only. Mode 6 implements a tunable approximation inspired by static Digitone II research; it is not a measured Elektron cutoff match. Module firmware, AUX, SPI protocol and hardware remain unchanged. Existing modules report activity and direction, without a magnitude counter or timestamped queue. Motion can be lost before MAIN receives it; acceleration approximates the density of received reports.

## Response and timing

MAIN owns acceleration, timing, reversal and wire quantization. SmartGrid owns parameter values, limits, selector quantization, gestures and feedback. One response applies to all parameters, with no pressed-turn behavior.

| Response control | Initial value |
| --- | ---: |
| `x_baseStep` | 1/1024 normalized range per reported pulse |
| `x_frameUs` | 8000 µs |
| `x_accelerationCoefficient` | 18 |
| `x_decayUs` | 10000 µs |
| `x_maxGain` | 16 |

Each physical knob holds signed acceleration `a`, frame pulse sum `q`, direction, frame/decay clocks and signed codec residual. At a nonempty frame boundary:

```text
movement = baseStep * q * max(1, abs(a))
if abs(q) > 1 or abs(a) > 1:
    a = clamp(a + trunc(accelerationCoefficient*q/8), -maxGain, maxGain)
q = 0
```

The output uses the previous gain. At each decay boundary `a = trunc(2*a/3)`. Signed division truncates toward zero. Empty frames and decay emit nothing. Isolated single pulses retain minimum gain. Without elapsed decay, frames `1, 2, 2, 2, -2` produce base-step movements `1, 2, 8, 16, -2` and acceleration `0, 4, 8, 12, -4`; decay from 12 produces `8, 5, 3, 2, 1, 0`.

Frame boundaries are relative to each knob's initialized clock. Elapsed frame/decay boundaries run chronologically, with decay first at ties. A boundary pulse belongs to the next frame. Unsigned elapsed-time arithmetic handles micros rollover, including initialization at zero. Service must occur at least once per half uint32 microsecond range (about 35 minutes); longer gaps are unsupported because retained boundaries may predate the previous call. Long idle settles bounded acceleration then skips empty time. There is no blocking delay, heap allocation per event or USB-arrival timing dependency.

Reversal advances due boundaries, flushes the old partial frame at its old gain, drains its representable movement, then resets acceleration before collecting the new direction. Signed sub-minimum residual persists as numerical error and cannot independently emit an opposite correction. Controller bank/configuration route changes drain to the old route then reset; SmartGrid page changes and incoming ring feedback do not reset acceleration. The reconstructed movement gate is omitted because its units are unknown and modules already apply hysteresis.

## Wire codec

Ordinary three-byte CC messages use CC 0–15 on human MIDI channel **9**, configuration/parser channel **8**. Rotation is USB-only (`midi_port = 1`) to avoid saturating DIN. Buttons retain their existing messages/channel and ring/value feedback remains on human channel 1 (zero-based 0). Mode 6 accepts ordinary CC only on E41H/E41V endless-pot hardware; unsupported combinations remain silent. Startup dumps, switch-generated rotary sends and feedback do not emit exponential movement.

The codec constants are fixed protocol values, independent of the five response controls:

```text
minimumDelta = 1/65536
codesPerDoubling = 8
if value == 64: ignore the entire message
z = int(value) - 64
magnitude = minimumDelta * expm1(log(2)*abs(z)/codesPerDoubling)
                            / expm1(log(2)/codesPerDoubling)
delta = sign(z)*magnitude
```

Delta is a fraction of the full normalized `[0,1]` parameter range, not Hz, absolute MIDI value or selector index.

| Byte | Normalized delta |
| ---: | ---: |
| 0 | -0.042990704731587 |
| 1 | -0.039408657687819 |
| 32 | -0.002528864984211 |
| 56 | -0.0001685909989474 |
| 63 | -0.0000152587890625 |
| 64 | ignored, no event |
| 65 | +0.0000152587890625 |
| 72 | +0.0001685909989474 |
| 96 | +0.002528864984211 |
| 127 | +0.039408657687819 |

There are 64 negative codes and 63 positive codes. Byte 0 is slightly larger than 127 in magnitude; splitting preserves symmetric total movement. There is no alias for 64 and no runtime lookup table. Shared independent test vectors are in `tools/encoder_response/protocol-v1-vectors.json`.

Encoding chooses the largest decoded magnitude no greater than remaining movement:

```text
candidate = floor(codesPerDoubling*log2(
    1 + abs(remaining)*expm1(log(2)/codesPerDoubling)/minimumDelta))
candidate = clamp(candidate, 0, remaining > 0 ? 63 : 64)
```

Evaluate the candidate and adjacent code to correct floating-point rounding. Candidate zero retains residual and emits nothing. Otherwise emit `64 + sign(remaining)*candidate`, subtract its decoded movement and repeat. Double-precision residual accounting conserves repeated fine movement; each movement drains before accepting representable opposing motion. Firmware never emits 64.

## Bounds and build evidence

The scan guard permits one scan per distinct millis bucket; variable SPI processing latency can move receipt times across buckets. The corrected frame bound is `ceil(frameUs/1000)+2 = 10` pulses, giving `10*16/1024 = 0.15625` maximum normalized frame movement plus sub-minimum residual. Re-derive this bound if grouping or gain changes. A conservative analytic ceiling is 12 packets per drain; exhaustive q=-10..10, gains 1..16, residual extremes and a 200,001-point dense sweep observed at most 8. Movement is never truncated to fit a packet budget. Host tests reject nonfinite and out-of-contract inputs.

ARM state is 24 response bytes plus 8 codec bytes = **32 bytes per physical encoder**, 512 for 16 and 2048 for 64, allocated once through the existing checked RAM allocator; no per-bank replication. Static data is 448 bytes and BSS 15364. Bundled-core MAIN sketch is 123120 bytes, up 12340 from the 110780-byte baseline. Binary file size is 123576 bytes; SHA-256 `cde0424351d846fc0ff9ea69a30292e67c7c16178b2521b5069494f4f2c531cc`.

## Profile and compatibility

`configs/WRLD.BLDR.exponential-v1.ytx` is a deep copy of the read-only source `/Users/joyo/.codex/worktrees/f8f7/theallelectricsmartgrid/patches/WRLD.BLDR.ytx`, SHA-256 `642a6f0a1f73e077b088a607024033c11bf1a04d504d6b7acbef3bfc471c83bf`.

```sh
python3 tools/make_exponential_encoder_profile.py /Users/joyo/.codex/worktrees/f8f7/theallelectricsmartgrid/patches/WRLD.BLDR.ytx configs/WRLD.BLDR.exponential-v1.ytx
```

Only `encoder_mode.hw_mode = 6`, `rotary_config.channel = 8`, and `rotary_config.midi_port = 1` change for each of 16 encoders: exactly 48 leaves. Speed, min/max, switch settings, feedback, hardware identifiers and non-encoder controls are preserved. The original source is unchanged. Its `parameter` metadata swaps groups 4–7 and 8–11, while `parameter_lsb` follows physical indices. The generator validates each representation independently as the complete CC 0–15 set, with zero `parameter_msb`, and preserves both permutations; no editor serialization behavior is assumed.

| Firmware/profile/receiver | Behavior |
| --- | --- |
| New MAIN + original profile + existing receiver | Existing protocol and response continue to work |
| New MAIN + exponential profile + old SmartGrid | Channel-9 rotation ignored; buttons/feedback retain their routes |
| New MAIN + exponential profile + updated receiver | Intended v1 path, pending receiver implementation and hardware evaluation |
| Old MAIN + exponential profile | Unsupported; do not enable |

For a later authorized deployment, retain the original profile, install tested MAIN and the companion receiver, then load the separate exponential profile. Do not open/save mode 6 through an older editor that may coerce an unknown mode. Roll back by restoring the original WRLD.BLDR profile; the new MAIN supports it. These artifacts do not flash firmware, upload configuration, send diagnostics or modify SmartGrid.

## Required SmartGrid handoff — still pending

Current `EncoderMidi::FromMidi` subtracts 64 on channel 0; `Encoder::HandleIncDec` adds timing-dependent acceleration. Already accelerated exponential movement must bypass both operations.

1. Add `MessageIn::Mode::EncoderExponentialDelta` in `private/src/MessageIn.hpp`, carrying the raw byte in existing integer `m_amount`. Implement a standalone v1 analytic decoder with identical constants and shared test vectors.
2. In `private/src/WrldBLDRMidi.hpp`, recognize CC 0–15 on zero-based channel 8, route to existing encoder grid positions, and ignore 64 before producing an event. Do not globally reinterpret channel 8 for other devices.
3. In `EncoderBank::Apply` (`private/src/EncoderBank.hpp`), look up the visible **BankedEncoderCell**, decode the byte and call **`cell->Increment(delta)` directly**. Do not call `HandleIncDec`, `SetToValue` or `IncrementInternal`. `Increment` preserves gestures/scenes while bypassing the old acceleration layer.
4. Preserve legacy channel-0 `EncoderIncDec`, buttons, feedback and parameter mappings. Continuous parameters receive normalized deltas; discrete selectors retain normalized accumulation and rounding.
5. Verify all 128 bytes, invalid CC range, no-event 64, controller compatibility, identical results under different packet arrival timings, continuous/discrete parameters, gesture/scene handling and feedback. No MIDI callback changes or SysEx negotiation are needed.

## Provenance and later hardware evaluation

Static research examined Digitone II MAIN OS 1.10D, SHA-256 `8caf761317ec25848e47bd6c6907188e8d4de180e744a350fd8d4df8e5715a3f`. It supports signed acceleration, reversal reset, previous-gain output and truncating 2/3 decay. A candidate continuous profile has `(k, divisor, step) = (18, 1, 1)`. Cutoff assignment, input scaling, event grouping and physical timer frequency remain unverified. Timer registration argument 10 means every third parent dispatch, not a proven 10 ms period. The attempted isolated instruction-emulation verification did not run. The official manual does not specify these constants. Only coefficient 18 comes from that candidate; other defaults and the gain cap are engineering starting points.

For later authorized hardware evaluation:

- Compare slow full-range travel and micro-adjustments first, then medium/fast sweeps.
- Check stop/restart and immediate reversal.
- Turn three knobs on different modules simultaneously; check rapid SmartGrid page changes.
- Check boundary clamping and a discrete selector.
- Check ring feedback, startup dump and USB disconnect/reconnect.

Tune base sensitivity first, then grouping and acceleration coefficient, then decay interval, then maximum gain. Keep both codec constants fixed, and initially keep 2/3 decay and integer division by 8 fixed. Revisit the lossy module protocol only if measured lost motion or a speed plateau cannot be resolved by response tuning. Hardware feel and receiver integration remain separate deliverables; no hardware success is claimed.

## Final verification

Verified in branch `codex/ytx-encoder-response`, worktree `/Users/joyo/.codex/worktrees/ytx-encoder-response/ytx-controller`:

| Command | Result |
| --- | --- |
| `python3 tools/test_exponential_encoder_profile.py` | 5 portable unittest cases passed |
| `python3 tools/test_encoder_response.py` | Recurrence/timing/rollover/idle/independence/pipeline passed; response 24 bytes, pulse bound 10; codec conservation/dense sweep passed, observed maximum 8 packets, ceiling 12; all 128 independent decoder vectors passed under ASan/UBSan |
| `python3 tools/test_encoder_integration.py` | Production init/allocation, dump/buttons, new/legacy motion, routes, disabled state, same-millis service, three-module fairness and ten delayed receipts passed; combined state 32 bytes |
| `python3 tools/test_quarter_step_table.py` | Quarter-step table tests passed |
| `python3 tools/test_usb_reset.py` | All 17 cases passed under ASan/UBSan |
| `ARDUINO_DIRECTORIES_USER="$PWD" make main` | Exit 0; sketch 123120 bytes (53% of 229376) |
| `git diff --check` | Passed |

Build output: `ytx-main-controller/build/ytx-main-controller.ino.app.bin`; build log `/private/tmp/ytx-task4-main.log`. Generated profile SHA-256: `078eaf5b7f5c6faf9ca39d1c577e394f38f69fd28cf717d6cceb0f523701b2da`. Independent recursive comparison confirmed precisely the 48 permitted changed leaves and identical remaining fields; the read-only source SHA-256 was recomputed unchanged. Tests/build verify software behavior; companion SmartGrid integration and physical calibration remain pending.
