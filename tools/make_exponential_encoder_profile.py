#!/usr/bin/env python3
"""Generate a separate WRLD.BLDR exponential-relative v1 profile; never contact a device."""
import argparse
import copy
import json
import os
from pathlib import Path


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def transform(source):
    """Validate the supported legacy profile, then change exactly three leaves per knob."""
    try:
        hardware = source['hwconfig']
        banks = source['banks']
        modules = hardware['encoders']
        if (hardware['device_name'] != 'WRLD.BLDR'
                or type(banks) is not dict or not banks
                or type(hardware['banksNumber']) is not int
                or hardware['banksNumber'] != len(banks)
                or set(banks) != {str(i) for i in range(len(banks))}
                or type(modules) is not list or len(modules) != 8
                or any(m not in ('ENC41H', 'ENC41V') for m in modules[:4])
                or modules[4:] != ['NONE'] * 4):
            raise ValueError('Expected WRLD.BLDR with exactly 16 supported endless pots')
        for bank in banks.values():
            entries = bank['encoders']
            if type(entries) is not dict or set(entries) != {str(i) for i in range(16)}:
                raise ValueError('Each bank must contain encoders 0–15')
            for field in ('parameter', 'parameter_lsb'):
                values = [entry['rotary_config'][field] for entry in entries.values()]
                if (any(not integer(value, 0, 15) for value in values)
                        or set(values) != set(range(16))):
                    raise ValueError('Each rotary CC representation must contain CC 0–15 exactly once')
            for index in range(16):
                entry = entries[str(index)]
                rotary = entry['rotary_config']
                switch = entry['switch_config']
                if (type(entry['encoder_mode']['hw_mode']) is not int
                        or entry['encoder_mode']['hw_mode'] != 1
                        or type(rotary['message']) is not int or rotary['message'] != 2
                        or not integer(rotary['channel'], 0, 15)
                        or not integer(rotary['midi_port'], 1, 3)):
                    raise ValueError('Expected legacy ordinary CC rotary configuration')
                if type(rotary['parameter_msb']) is not int or rotary['parameter_msb'] != 0:
                    raise ValueError('Ordinary CC requires parameter_msb zero')
                # Shift, fine, alternate CC, bank and velocity buttons can alter rotary behavior.
                if (not integer(switch['mode'], 0, 1)
                        or type(switch['double_click']) is not int
                        or switch['double_click'] != 0):
                    raise ValueError('Only disabled or ordinary message buttons without double click are supported')
    except (KeyError, TypeError, AttributeError) as error:
        raise ValueError('Malformed profile: missing or invalid configuration structure') from error
    result = copy.deepcopy(source)
    for bank in result['banks'].values():
        for entry in bank['encoders'].values():
            entry['encoder_mode']['hw_mode'] = 6
            entry['rotary_config']['channel'] = 8
            entry['rotary_config']['midi_port'] = 1
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if (args.input.resolve() == args.output.resolve()
            or (args.output.exists() and os.path.samefile(args.input, args.output))):
        parser.error('Input and output must be different files')
    try:
        result = transform(json.loads(args.input.read_text(encoding='utf-8')))
    except (ValueError, OSError) as error:
        parser.error(str(error))
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
