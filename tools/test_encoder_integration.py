#!/usr/bin/env python3
"""Compile actual MAIN methods with fake SPI, clock, EEPROM and MIDI dependencies."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / 'ytx-main-controller'


def method(source, name):
    # A top-level closing brace delimits these existing sketch methods. No method
    # body is rewritten; this is the same sketch-to-C++ assembly Arduino performs.
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^\n]*\)\s*\{.*?^\}', source, re.M | re.S)
    if not match:
        raise AssertionError('Missing production method: ' + name)
    return match.group(0)


def main():
    with tempfile.TemporaryDirectory(prefix='ytx-integration-') as tmp:
        tmp = Path(tmp)
        header = (MAIN / 'headers/EncoderInputs.h').read_text()
        header = re.sub(r'^#include [<"](?:SPI.h|SPIGPIOExpander.h|SPIEndlessPot.h|FeedbackClass.h)[>"]\n', '', header, flags=re.M)
        header = header.replace('private:', 'public:')
        (tmp / 'EncoderInputsHost.h').write_text(header)
        names = ['Init', 'SwitchCheck', 'SwitchAction', 'GetModuleType', 'ReadModule', 'ModuleHasActivity', 'Read', 'RotaryHasConfiguration',
                 'DecodeRotaryDirection', 'EncoderAction', 'AddToPriority', 'SendRotaryMessage',
                 'SendRotaryAltMessage', 'SetEncoderValue', 'IsBankShifted',
                 'IsExponentialEncoder', 'ServiceExponentialEncoders', 'ResetExponentialEncoders',
                 'FlushExponentialEncoders', 'FlushExponentialEncoder', 'SendExponentialRotaryMessage']
        source = (MAIN / 'encoders.ino').read_text()
        # Existing sketch code has unused fields/parameters and legacy bitwise
        # precedence warnings. Keep those suppressions local to extracted legacy
        # methods; all new adapters and the fixture compile with -Werror.
        legacy_warnings = ['-Wunused-parameter', '-Wunused-variable',
                           '-Wunused-but-set-variable', '-Wlogical-op-parentheses',
                           '-Wsometimes-uninitialized', '-Wreturn-type']
        bodies = []
        for name in names:
            body = method(source, 'EncoderInputs::' + name)
            if 'Exponential' not in name:
                pragmas = '\n'.join('#pragma clang diagnostic ignored "' + warning + '"'
                                    for warning in legacy_warnings)
                body = '#pragma clang diagnostic push\n' + pragmas + '\n' + body + '\n#pragma clang diagnostic pop'
            bodies.append(body)
        bodies += [method((MAIN / 'utils.ino').read_text(), 'DumpControllerState')]
        memory = (MAIN / 'memoryHost.ino').read_text()
        bodies += [method(memory, 'memoryHost::' + name) for name in ['LoadBank', 'LoadBankSingleSection']]
        (tmp / 'production.inc').write_text('\n\n'.join(bodies))
        binary = tmp / 'integration'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++11', '-Wall', '-Wextra', '-Werror',
                        '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I', str(tmp), '-I', str(MAIN / 'headers'),
                        str(ROOT / 'tools/encoder_response/integration.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
