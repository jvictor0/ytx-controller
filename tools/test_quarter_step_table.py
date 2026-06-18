#!/usr/bin/env python3
import re
from pathlib import Path


HEADER = Path(__file__).resolve().parents[1] / "ytx-main-controller/headers/EncoderInputs.h"

DIR_CW = 0x10
DIR_CCW = 0x20

STATE_VALUES = {
    "R_START_0": 0,
    "R_START_1": 1,
    "R_START_2": 2,
    "R_START_3": 3,
    "R_CW_1": 4,
    "R_CW_2": 5,
    "R_CW_3": 6,
    "R_CCW_1": 7,
    "R_CCW_2": 8,
    "R_CCW_3": 9,
    "DIR_CW": DIR_CW,
    "DIR_CCW": DIR_CCW,
}


def parse_quarter_step_table():
    text = HEADER.read_text()
    match = re.search(r"quarterStepTable\[10\]\[4\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not match:
        raise AssertionError("quarterStepTable not found")

    rows = []
    for row in re.findall(r"\{([^{}]+)\}", match.group(1)):
        values = []
        for cell in row.split(","):
            value = 0
            for token in cell.strip().split("|"):
                token = token.strip()
                if not token:
                    continue
                value |= STATE_VALUES[token]
            values.append(value)
        rows.append(values)
    return rows


def emitted_directions(table, pin_sequence):
    state = 0
    directions = []
    for pin_state in pin_sequence:
        state = table[state & 0x0F][pin_state]
        direction = state & 0x30
        if direction:
            directions.append(direction)
    return directions


def test_ccw_sequence_from_start_3_emits_only_ccw():
    table = parse_quarter_step_table()
    pin_sequence = [3, 2, 0, 1, 3] * 2
    directions = emitted_directions(table, pin_sequence)

    assert directions == [DIR_CCW] * 8, directions


def test_cw_sequences_still_emit_only_cw():
    table = parse_quarter_step_table()
    for pin_sequence in ([0, 2, 3, 1, 0] * 2, [3, 1, 0, 2, 3] * 2):
        directions = emitted_directions(table, pin_sequence)
        assert directions == [DIR_CW] * 8, directions


def test_ccw_sequence_from_start_0_emits_only_ccw():
    table = parse_quarter_step_table()
    pin_sequence = [0, 1, 3, 2, 0] * 2
    directions = emitted_directions(table, pin_sequence)

    assert directions == [DIR_CCW] * 8, directions


if __name__ == "__main__":
    for test in (
        test_ccw_sequence_from_start_3_emits_only_ccw,
        test_cw_sequences_still_emit_only_cw,
        test_ccw_sequence_from_start_0_emits_only_ccw,
    ):
        test()
    print("quarter-step table tests passed")
