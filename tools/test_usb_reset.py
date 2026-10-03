#!/usr/bin/env python3
"""Compile the actual USB receive code against a host register-admission model.

No Arduino SDK or hardware is used. CXX defaults to clang++; build artifacts live
in a temporary directory. This tests selected IRQ boundaries, not USB wire timing.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tools/usb_reset'
USB = ROOT / 'hardware/yaeltexv2/samd/cores/arduino/USB'
MIDI = ROOT / 'hardware/yaeltexv2/samd/libraries/MIDIUSB/src/MIDIUSB.cpp'


def between(text, start, end):
    return text[text.index(start):text.index(end)]


def main():
    header = (USB / 'SAMD21_USBDevice.h').read_text()
    core = (USB / 'USBCore.cpp').read_text()
    midi = MIDI.read_text()
    source = '\n'.join([
        (FIXTURES / 'model.h').read_text(),
        between(header, 'class __Guard {', '/*\n * USB EP generic handlers.'),
        '#define malloc tracked_malloc',
        header[header.index('class EPHandler {'):],
        '#undef malloc',
        'static EPHandler* epHandlers[7] = {};',
        between(core, 'void USBDeviceClass::initEndpoints()', 'void USBDeviceClass::flush('),
        between(core, 'uint32_t USBDeviceClass::available(', '// Recv 1 byte if ready'),
        (FIXTURES / 'midi.h').read_text(),
        between(midi, 'void MIDI_::accept(', 'void MIDI_::flush('),
        (FIXTURES / 'cases.cpp').read_text(),
    ])
    cases = [
        'cold', 'reset', 'partial', 'full', 'pending', 'repeat', 'zlp',
        'full_transfer', 'midi_ring_wrap', 'data0_reconfigure',
        'irq_recv0', 'irq_recv1', 'irq_available', 'masked', 'midi_empty',
        'midi_reset_race', 'midi_unconfigured',
    ]
    with tempfile.TemporaryDirectory(prefix='ytx-usb-reset-') as tmp:
        cpp = Path(tmp) / 'test.cpp'
        binary = Path(tmp) / 'test'
        cpp.write_text(source)
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++11',
                        '-Wall', '-Wextra', '-Werror', '-g',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        str(cpp), '-o', str(binary)], check=True)
        failures = []
        # Allocated endpoint buffers have firmware lifetime; test processes exit
        # without inventing a destructor absent from the production interface.
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        for case in cases:
            result = subprocess.run([str(binary), case], env=env,
                                    capture_output=True, text=True)
            print(f'{case}: {"PASS" if result.returncode == 0 else "FAIL"}')
            if result.returncode:
                print(result.stdout + result.stderr, end='')
                failures.append(case)
        if failures:
            raise SystemExit(f'{len(failures)} failed: {", ".join(failures)}')
        print(f'{len(cases)} USB reset cases passed (ASan/UBSan).')


if __name__ == '__main__':
    main()
