# Exponential encoder response v1

This opt-in implementation changes MAIN firmware only. Mode 6 uses the Digitone II cutoff recurrence and nominal timer periods recovered on 2026-10-12, with an empirical WRLD.BLDR sensitivity trial: base step 1/1500 and acceleration coefficient 8. This replaces the earlier 1/32768 and coefficient-18 calibration. The user approved its feel after the hardware trial on 2026-10-12; this does not establish an exact Elektron feel match. This revision updates MAIN only and keeps the existing mode-6 profile and exponential-relative v1 wire protocol. Module firmware, AUX, SPI protocol and hardware remain unchanged. Existing modules report activity and direction, without a magnitude counter or timestamped queue. Motion can be lost before MAIN receives it; acceleration approximates the density of received reports.

## Response and timing

MAIN owns acceleration, timing, reversal and wire quantization. SmartGrid owns parameter values, limits, selector quantization, gestures and feedback. One response applies to all parameters, with no pressed-turn behavior.

| Response control | Current value | Basis |
| --- | ---: | --- |
| `x_baseStep` | 1/1500 normalized range per accepted report at minimum gain | Rounded empirical quarter-turn fit |
| `x_frameUs` | 33334 µs | Native nominal batch period 33.334303 ms |
| `x_accelerationCoefficient` | 8 | Empirical YTX adaptation; native cutoff profile A uses 18 |
| `x_decayUs` | 100000 µs | Native nominal 10 Hz decay callback |
| `x_batchLimit` | 30 reports per frame | Native signed input-batch clamp |

The old 16× gain cap is removed. Each physical knob holds signed acceleration `a`, raw same-direction frame report sum `p`, direction, frame/decay clocks and signed codec residual. At a nonempty frame boundary:

```text
q = clamp(p, -30, 30)
movement = baseStep * q * max(1, abs(a))
if abs(q) > 1 or abs(a) > 1:
    a += trunc(8*q/8)
p = 0
```

The ±30 limit is input saturation, not an acceleration cap. Just as in the native input stage, the entire raw batch is cleared: reports beyond 30 in one frame are deliberately discarded. With MAIN's conservative 36-report frame bound, this can discard up to six reports in a saturated frame. All movement *after this explicit input clamp* and its codec residual are conserved; packet limits do not truncate movement. At reversal YTX retains its old-partial-frame drain before resetting acceleration; it does not copy the native panel stage's possible opposite-direction input loss.

The output uses the previous gain. At each decay boundary `a = trunc(2*a/3)`. Signed division truncates toward zero. Empty frames and decay emit nothing. Isolated single pulses retain minimum gain. Without elapsed decay, frames `1, 2, 2, 2, -2` produce base-step movements `1, 2, 4, 8, -2` and acceleration `0, 2, 4, 6, -2`; decay from 12 produces `8, 5, 3, 2, 1, 0`.

Frame boundaries are relative to each knob's initialized clock. Elapsed frame/decay boundaries run chronologically, with decay first at ties. A boundary pulse belongs to the next frame. Unsigned elapsed-time arithmetic handles micros rollover, including initialization at zero. Service must occur at least once per half uint32 microsecond range (about 35 minutes); longer gaps are unsupported because retained boundaries may predate the previous call. Long idle settles bounded acceleration then skips empty time. There is no blocking delay, heap allocation per event or USB-arrival timing dependency.

Reversal advances due boundaries, flushes the old partial frame at its old gain, drains its representable movement, then resets acceleration before collecting the new direction. Signed sub-minimum residual persists as numerical error and cannot independently emit an opposite correction. Controller bank/configuration route changes drain to the old route then reset; SmartGrid page changes and incoming ring feedback do not reset acceleration. The native movement gate (threshold 24, reset 48, recovery +1 per 100 ms) remains omitted: conversion from its raw counts to YTX activity reports and its reset call sites are unestablished, and YTX modules already apply hysteresis.

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

## Sensitivity adaptation

One module activity report is not proven equivalent to one native Digitone count. The cutoff internal range `0..0x7f00` (32512) therefore does not justify directly normalizing each YTX report by 32512. The first calibration selected 1/32768 using a synthetic 1,000-report/s trace. Actual quarter-turn captures exposed much sparser reports and substantially finer movement than the Digitone.

The current trial uses **1/1500** and **coefficient 8**, rounded from an endpoint fit of approximately 1/1500.63 and coefficient 8. The base is 21.85 times larger than the prior calibration; reducing the coefficient limits the accompanying increase in accelerated motion. Each isolated report is immediately representable, with fractional wire error retained by the unchanged codec.

The 2026-10-12 WB quarter-turn capture yielded 31–40 accepted reports per individual turn across slow, medium and fast gestures, inferred from exact production-code replay of 317 positive MIDI packets. The first slow turn had 38 reports and no stored acceleration. Measured movement was 0.12–0.17% of full range for slow turns, 1.22–1.31% for medium, and 1.75–2.31% for fast. Corresponding Digitone quarter-turn observations were 2.25–3.82%, 14.3–18.8%, and 10.1–19.1%. Its measured NRPN wire range was 0–16256, distinct from the internal range.

The fit minimizes log error of total movement for six individual turns while retaining the recovered timing and recurrence. It is not a physical trajectory fit: angles and speeds were approximate, and the two paired turns excluded from fitting overshot their target by about 1.6–3.2 times. The user authorized a subjective trial after these limitations were reported. This change intentionally makes the earlier fine response coarser; timing, reversal and codec semantics remain unchanged.

Raw captures and analysis are under `/private/tmp/wrld-bldr-quarter-turn-experiment-20261012/` and `/private/tmp/digitone-cutoff-experiment-20261012/`; `offline-fit.json` records the objective, candidates and limitations. The earlier half-turn dataset is `/private/tmp/wrld-bldr-speed-experiment-20261012/`. The new deployment evidence belongs under `/private/tmp/wrld-bldr-fitted-response-20261012/`.

## Bounds and work

The scan guard permits one read per distinct millis bucket; variable SPI latency can deliver one earlier scan inside the interval. For any service interval `T`, the adapter supplies at most `ceil(T/1000)+2` reports per physical encoder. Thus `x_maxFramePulses = 36` and `x_maxDecayPulses = 102`. The input batch passed to the profile is at most 30.

A decay interval's emitted batches can include its 102 incoming reports plus one pending 36-report frame. Even with early partial flushes, the sum of acceleration increments is bounded by `B = floor(8*(102+36)/8) = 138`. Starting from reset, the invariant peak magnitude is `3*B = 414`: decay takes it to at most `2*B`, then another interval adds at most `B`. Reversal only lowers this bound. **414 is a derived assertion and never a clamp or a tuning control.** All signed integer arithmetic fits in 32 bits. Direct recurrence/clock users must honor the adapter's report-rate contract, not just a per-call pulse limit.

The largest possible drain input is `30*414/1500 = 8.28`, plus a sub-minimum codec residual. This changes only the encoder's accepted-input/work bounds, not the v1 decoding formula or bytes. Let `M = Decode(127)`, `d = minimumDelta` and `alpha = 1 - 2^(-1/8)`. There are at most 210 full positive-size packets because `maximumInput+d < 211*M`; the negative full packet is larger. Below `M`, each floor-encoded packet leaves `R_next < alpha*R + d*(1-alpha)`. After four such packets, `R < d + (M-d)*alpha^4 < 2*d`; a fifth, if needed, leaves less than `d`. Therefore **215 packets per drain** is a conservative ceiling. Exhaustive batches -30..30, every integer gain 1..414, three residual extremes and a 200,001-point dense sweep observed **214**. Nonfinite and out-of-contract inputs fail assertions in host tests.

Idle service performs only the decays required to settle the bounded acceleration, then jumps over empty frames in constant time. Per-event state size is unchanged: 24 response bytes plus 8 codec bytes = **32 bytes per physical encoder**, 512 for 16 and 2048 for 64. Allocation remains checked and occurs once, outside the motion path. USB transmission and actual Cortex-M0+ execution latency require hardware observation; a finite packet bound does not claim a measured service deadline.

## Profile and compatibility

The device already has mode 6, ordinary USB CC0–15 on human channel 9 (zero-based 8). This firmware revision requires **no new profile, profile generation or configuration upload**. The 2026-10-12 exported live profile is `/private/tmp/wrld-bldr-exponential-20261012/WRLD.BLDR.exponential-v1.ytx`; it also exists in SmartGrid's `patches/WRLD.BLDR.ytx`. USB→DIN forwarding was intentionally preserved when that profile was installed.

The historical generated `configs/WRLD.BLDR.exponential-v1.ytx` and profile generator remain unchanged. The original source used canonical `parameter` values with a groups-4–7/8–11 permutation and stale redundant `parameter_lsb` fields. The live device readback established that canonical `parameter` matched its actual CC routing. This revision preserves all actual device bytes, including that routing, instead of exporting or rewriting metadata.

| Firmware/profile/receiver | Behavior |
| --- | --- |
| Revised MAIN + original legacy profile | Existing legacy response/protocol |
| Revised MAIN + existing mode-6 profile + exponential v1 receiver | Corrected response over unchanged v1 protocol |
| Revised MAIN + existing mode-6 profile + old receiver | Channel-9 rotation ignored |

Do not open/save mode 6 through an editor that might coerce an unknown mode. To roll back this behavior revision while retaining mode 6, flash the previously deployed MAIN binary; its configuration is compatible. Restoring the pre-mode-6 legacy profile instead selects the existing legacy response.

## Unchanged SmartGrid receiver contract

This task changes neither SmartGrid nor its deployment. The v1 path recognizes WRLD.BLDR CC0–15 on zero-based channel 8, ignores 64 before creating any event, and analytically decodes the remaining byte. `EncoderBank::Apply` passes that normalized delta directly to the visible `BankedEncoderCell::Increment`, bypassing `HandleIncDec`'s host acceleration. Legacy channel-0 processing, buttons, feedback, gestures/scenes and normalized discrete-selector accumulation retain their existing behavior. No receiver/protocol change is needed for the corrected MAIN timing and gain.

## Provenance and later hardware evaluation

The 2026-10-12 static trace examined saved Digitone II MAIN OS 1.10D, SHA-256 `8caf761317ec25848e47bd6c6907188e8d4de180e744a350fd8d4df8e5715a3f`. FilterPageView encoder E maps to cutoff for all six filter machines, through parameter IDs 0x24, 0x27, 0x29, 0x36, 0x39 and 0x13e. All use ordinary profile A: coefficient 18, divisor 1, step 1, continuous, ordinary input multiplier 1, no ×256 output scale and no explicit gain cap. Pressed behavior is outside this implementation.

The input callback batches four nominal PIT2 ticks: `4*64*(17187+1)/132000000` seconds = approximately 33.334303 ms, then clamps the signed batch to ±30 and clears it. Decay is every third nominal 30 Hz UI event: approximately 100 ms. Registration argument 10 denotes frequency, not milliseconds. Nominal timing comes from peripheral register interpretation and the firmware's intended 132 MHz scale; task scheduling and oscillator behavior were not measured.

Evidence, byte assertions and bounded disassemblies are in `/Users/joyo/.codex/visualizations/2026/10/11/01a12c12-4359-74c3-8c28-622bf7502328/digitone-cutoff-firmware-20261012/README.txt` and its sibling `findings.json`/`extract.py`. This is verified static evidence, not instruction emulation or hardware measurement. The earlier attempted isolated emulation did not run. Native physical counts per turn, YTX/native input scaling and gate reset call sites remain unestablished.

For later authorized hardware evaluation:

- Compare slow full-range travel and micro-adjustments first, then medium/fast sweeps.
- Check stop/restart and immediate reversal.
- Turn three knobs on different modules simultaneously; check rapid SmartGrid page changes.
- Check boundary clamping and a discrete selector.
- Check ring feedback, startup dump and USB disconnect/reconnect.

The current trial changes base sensitivity and acceleration together using the measured comparison. Keep batching, decay ratio/interval, batch saturation and both codec protocol constants fixed while evaluating it. Any future timing/coefficient change requires re-deriving the assertions and wire-drain ceiling. Revisit the lossy module protocol only if measured motion loss cannot be resolved by MAIN adaptation. The user tried the fitted response and approved its feel on 2026-10-12. The broader hardware checklist above remains available for further evaluation.

## Current trial verification

Response and production-integration tests passed under ASan/UBSan, along with quarter-step, all 17 USB reset cases and all five profile-generator cases (no profile generated). The recurrence expectations now use coefficient 8 and base 1/1500; the synthetic sustained peak is 268, below the derived assertion bound of 414. Exhaustive/dense codec tests observed 214 packets below the 215-packet ceiling, with all 128 independent decoder values unchanged. Conservation checks use an operation-count-scaled floating-point error allowance for the larger drains; sub-minimum residual and packet ceilings remain independently enforced.

Current MAIN build and deployment details are recorded in `/private/tmp/wrld-bldr-fitted-response-20261012/deployment.json`. Firmware source commit `e572a4225c395f00fac22854560692b743fdfc98` built successfully (123296 sketch bytes; application image 123752 bytes, SHA-256 `09062b2379e4951226a35d4af031c4922bffb890a97acb586d157c31e8f44c9f`). All 967 MAIN blocks were acknowledged, the application reconnected, and all 215 configuration sections / 6667 bytes matched the pre-flash backup, preserving the profile and USB→DIN forwarding. The user approved the hardware trial and requested landing without redeployment on 2026-10-12. The previous deployment below is retained as the rollback reference.

## Previous timing-correction deployment (rollback)

The previous revision tests covered both signs of explicit input saturation, full batch clearing, previous-gain output above the old cap, exact 100 ms decay, 33,334 µs boundary ownership, decay-first ties, rollover, delayed versus frequent service, long idle, reversal, independent knobs, periodic partial flushes, sustained 601 acceleration, and codec conservation. Integration compiles actual production methods and checks 36 delayed receipts, the 30-report clamp, old-route drains, three-module fairness and same-millis service. Existing startup/button/feedback silence and legacy compatibility tests remain in place.

Software verification passed:

| Check | Result |
| --- | --- |
| `python3 tools/test_encoder_response.py` | Recurrence, timing, both-sign batch clamp, sustained gain, partial flushes, idle, reversal, rollover and pipeline passed under ASan/UBSan; exhaustive/dense codec maximum 25 packets, bound 26; all 128 independent decoder vectors passed |
| `python3 tools/test_encoder_integration.py` | Actual MAIN methods passed, including 36 delayed receipts, clamp, silence, routes, fairness and legacy behavior |
| `python3 tools/test_quarter_step_table.py` | Passed |
| `python3 tools/test_usb_reset.py` | All 17 ASan/UBSan cases passed |
| `python3 tools/test_exponential_encoder_profile.py` | All 5 cases passed; no profile generated |
| `ARDUINO_DIRECTORIES_USER="$PWD" make main` | Passed; sketch 123304 bytes (53% of 229376), +184 bytes from the prior response build |
| `git diff --check` | Passed |

ARM ELF: text 123312, data 448, BSS 15364. The exported application binary is 123760 bytes, SHA-256 `b842fe68114611c155680b5209781e4eb5cb96a194bc9f23fd40404bef1f8dea`. Build output remains `ytx-main-controller/build/ytx-main-controller.ino.app.bin`. Deployment evidence is saved under `/private/tmp/wrld-bldr-cutoff-response-20261012/`. AUX, modules and .ytx files were not written by that revision.

Deployment completed on 2026-10-12: all **967 MAIN blocks** were acknowledged, the reset command completed, and a fresh MIDI client connected to WRLD.BLDR and successfully read configuration. All **215 sections / 6667 bytes** matched the pre-flash backup exactly (combined section-byte SHA-256 `de3882216848c202c495bbbc459b69a8b1798334de3d3cd301ddb02ec3e1dc68`). The existing mode-6 profile, CC routing and USB→DIN forwarding were preserved; no profile generation/upload or AUX/module flash occurred. `flash-transfer.json` and `configuration-verification.json` in the evidence directory record the checks. The firmware hash above identifies the acknowledged host binary; the bootloader has no firmware-hash readback API. Physical feel is still for the user to evaluate.
