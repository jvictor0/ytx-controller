#!/usr/bin/env python3
"""Test actual encoder headers with independent Python protocol values."""
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix='ytx-encoder-response-') as tmp:
        binary = Path(tmp) / 'cases'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++11',
                        '-Wall', '-Wextra', '-Werror', '-g',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I', str(ROOT / 'ytx-main-controller/headers'), str(ROOT / 'tools/encoder_response/cases.cpp'),
                        '-o', str(binary)], check=True)
        for invalid in ['nan', 'infinity', 'oversize', 'bad_remainder', 'negative_magnitude']:
            result = subprocess.run([str(binary), invalid], capture_output=True, text=True)
            assert result.returncode != 0, invalid
        values = subprocess.check_output([str(binary)], text=True)
        lines = values.splitlines()
        assert len(lines) == 129
        vectors = json.loads((ROOT / 'tools/encoder_response/protocol-v1-vectors.json').read_text())
        for vector in vectors['vectors']:
            assert math.isclose(float(lines[vector['value']]), vector['delta'],
                                rel_tol=2e-14, abs_tol=1e-18)
            assert vector['ignored'] == (vector['value'] == 64)
        for code, line in enumerate(lines[:128]):
            actual = float(line)
            z = code - 64
            expected = math.copysign(math.expm1(math.log(2) * abs(z) / 8)
                                     / math.expm1(math.log(2) / 8) / 65536, z)
            assert math.isclose(actual, expected, rel_tol=2e-14, abs_tol=1e-18), code
        print('\n'.join(lines[128:]))
        print('128 independent Python decoder vectors passed (ASan/UBSan).')


if __name__ == '__main__':
    main()
