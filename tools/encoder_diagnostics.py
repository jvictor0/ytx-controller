#!/usr/bin/env python3
import argparse
import json
import time
from typing import Any

import mido


SYSEX_ID = [0x79, 0x74, 0x78]  # ytx
WISH_SET = 1
MESSAGE_SPECIAL = 0
REQUEST_ENCODER_DIAGNOSTICS = 0x22


def u14(data: list[int], index: int) -> tuple[int, int]:
    return data[index] | (data[index + 1] << 7), index + 2


def parse_response(data: list[int]) -> dict[str, Any] | None:
    if len(data) < 60:
        return None
    if data[:3] != SYSEX_ID:
        return None
    if data[5] != MESSAGE_SPECIAL or data[6] != REQUEST_ENCODER_DIAGNOSTICS:
        return None

    index = 7
    result: dict[str, Any] = {
        "status": data[3],
        "version": data[index],
        "encoder": data[index + 1],
        "module": data[index + 2],
        "module_type": data[index + 3],
        "pin_a": data[index + 4],
        "pin_b": data[index + 5],
        "last_state": data[index + 6],
    }
    index += 7

    for key in (
        "samples",
        "invalid_transitions",
        "cw_transitions",
        "ccw_transitions",
        "read_mismatches",
        "min_sample_us",
        "max_sample_us",
    ):
        result[key], index = u14(data, index)

    state_counts = []
    for _ in range(4):
        value, index = u14(data, index)
        state_counts.append(value)
    result["state_counts"] = state_counts

    transitions = []
    for _ in range(16):
        value, index = u14(data, index)
        transitions.append(value)
    result["transition_counts"] = transitions
    return result


def send_request(port: mido.ports.BaseOutput, encoder: int, duration_ms: int) -> None:
    duration_ms = max(10, min(5000, duration_ms))
    data = SYSEX_ID + [
        0x00,
        WISH_SET,
        MESSAGE_SPECIAL,
        REQUEST_ENCODER_DIAGNOSTICS,
        encoder & 0x7F,
        duration_ms & 0x7F,
        (duration_ms >> 7) & 0x7F,
    ]
    port.send(mido.Message("sysex", data=data))


def print_report(result: dict[str, Any]) -> None:
    print(f"encoder      : {result['encoder']}")
    print(f"module/type  : {result['module']} / {result['module_type']}")
    print(f"pins A/B     : {result['pin_a']} / {result['pin_b']}")
    print(f"status       : {result['status']}")
    print(f"samples      : {result['samples']}")
    print(f"sample us    : min {result['min_sample_us']}, max {result['max_sample_us']}")
    print(f"read mismatch: {result['read_mismatches']}")
    print(f"invalid jumps: {result['invalid_transitions']}")
    print(f"CW / CCW     : {result['cw_transitions']} / {result['ccw_transitions']}")
    print("states       : 00={0} 01={1} 10={2} 11={3}".format(*result["state_counts"]))
    print("transitions  :")
    counts = result["transition_counts"]
    for prev_state in range(4):
        row = counts[prev_state * 4 : prev_state * 4 + 4]
        print(f"  {prev_state:02b}: 00={row[0]:5d} 01={row[1]:5d} 10={row[2]:5d} 11={row[3]:5d}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run YTX encoder raw A/B diagnostics over SysEx.")
    parser.add_argument("--port", default="WRLD.BLDR", help="MIDI port name substring")
    parser.add_argument("--encoder", type=int, required=True, help="internal encoder index")
    parser.add_argument("--duration-ms", type=int, default=1000, help="capture duration, 10..5000 ms")
    parser.add_argument("--timeout", type=float, default=8.0, help="seconds to wait for response")
    parser.add_argument("--json", action="store_true", help="print JSON instead of a text report")
    args = parser.parse_args()

    inputs = mido.get_input_names()
    outputs = mido.get_output_names()
    in_name = next((name for name in inputs if args.port in name), None)
    out_name = next((name for name in outputs if args.port in name), None)
    if not in_name or not out_name:
        raise SystemExit(f"Could not find MIDI input/output containing {args.port!r}")

    with mido.open_input(in_name) as in_port, mido.open_output(out_name) as out_port:
        while in_port.poll():
            pass
        send_request(out_port, args.encoder, args.duration_ms)

        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            msg = in_port.poll()
            if msg is None:
                time.sleep(0.005)
                continue
            if msg.type != "sysex":
                continue
            result = parse_response(list(msg.data))
            if result is None:
                continue
            if args.json:
                print(json.dumps(result, indent=2, sort_keys=True))
            else:
                print_report(result)
            return

    raise SystemExit("Timed out waiting for encoder diagnostics response")


if __name__ == "__main__":
    main()
