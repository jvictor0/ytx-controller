#!/usr/bin/env python3
"""Portable standard-library tests for the opt-in profile generator."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from make_exponential_encoder_profile import transform


def fixture():
    return {'hwconfig': {'device_name': 'WRLD.BLDR', 'banksNumber': 1,
                         'encoders': ['ENC41H'] * 4 + ['NONE'] * 4},
            'metadata': {'keep': [1, 'hello']},
            'banks': {'0': {'other': {'value': 42}, 'encoders': {
                str(i): {'encoder_mode': {'hw_mode': 1, 'speed_value': 2},
                         'rotary_config': {'message': 2, 'channel': 0, 'midi_port': 3,
                                           'parameter': i, 'parameter_lsb': i,
                                           'parameter_msb': 0, 'parameter_min': 0,
                                           'parameter_max': 127},
                         'switch_config': {'mode': 1, 'message': 1, 'channel': 1,
                                           'double_click': 0},
                         'rotation_feedback': {'channel': 0}, 'value': 0}
                for i in range(16)}}}}


class ProfileTests(unittest.TestCase):
    def test_exact_changes_and_deep_copy(self):
        source = fixture()
        original = copy.deepcopy(source)
        result = transform(source)
        expected = copy.deepcopy(source)
        for entry in expected['banks']['0']['encoders'].values():
            entry['encoder_mode']['hw_mode'] = 6
            entry['rotary_config']['channel'] = 8
            entry['rotary_config']['midi_port'] = 1
        self.assertEqual(result, expected)  # Every other field must be identical.
        self.assertEqual(source, original)
        result['metadata']['keep'].append(9)
        self.assertEqual(source, original)

    def test_preserves_independent_cc_permutations(self):
        source = fixture()
        entries = source['banks']['0']['encoders']
        for i in range(4, 12):
            entries[str(i)]['rotary_config']['parameter'] = i + 4 if i < 8 else i - 4
        result = transform(source)
        for i in range(16):
            self.assertEqual(result['banks']['0']['encoders'][str(i)]['rotary_config']['parameter'],
                             entries[str(i)]['rotary_config']['parameter'])
            self.assertEqual(result['banks']['0']['encoders'][str(i)]['rotary_config']['parameter_lsb'], i)

    def test_vertical_hardware_multiple_banks_and_disabled_buttons(self):
        source = fixture()
        source['hwconfig']['encoders'][0] = 'ENC41V'
        source['hwconfig']['banksNumber'] = 2
        source['banks']['1'] = copy.deepcopy(source['banks']['0'])
        source['banks']['1']['encoders']['0']['switch_config']['mode'] = 0
        result = transform(source)
        for bank in result['banks'].values():
            self.assertEqual(bank['encoders']['0']['encoder_mode']['hw_mode'], 6)
        self.assertEqual(result['banks']['1']['encoders']['0']['switch_config']['mode'], 0)

    def test_rejections(self):
        mutations = [
            lambda d: d.pop('banks'),
            lambda d: d['hwconfig'].update(device_name='other'),
            lambda d: d['hwconfig'].update(banksNumber=2),
            lambda d: d['hwconfig']['encoders'].__setitem__(0, 'ENC42'),
            lambda d: d['banks']['0']['encoders'].pop('15'),
        ]
        for field, value in [('message', 3), ('parameter', 128), ('parameter', -1),
                             ('parameter', True), ('parameter_lsb', 9),
                             ('parameter_msb', 1), ('channel', 16), ('midi_port', 0)]:
            mutations.append(lambda d, f=field, v=value:
                             d['banks']['0']['encoders']['0']['rotary_config'].__setitem__(f, v))
        for mode in [2, 3, 4, 5, 6, 7, 99]:
            mutations.append(lambda d, v=mode:
                             d['banks']['0']['encoders']['0']['switch_config'].__setitem__('mode', v))
        mutations.extend([
            lambda d: d['banks']['0']['encoders']['0']['encoder_mode'].update(hw_mode=6),
            lambda d: d['banks']['0']['encoders']['0']['switch_config'].update(double_click=1),
        ])
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                source = fixture()
                mutate(source)
                original = copy.deepcopy(source)
                with self.assertRaises(ValueError):
                    transform(source)
                self.assertEqual(source, original)

    def test_cli_source_preservation_and_same_path(self):
        script = Path(__file__).with_name('make_exponential_encoder_profile.py')
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source.ytx'
            output = Path(directory) / 'output.ytx'
            source.write_text(json.dumps(fixture()))
            before = source.read_bytes()
            run = subprocess.run([sys.executable, str(script), str(source), str(output)], capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(json.loads(output.read_text()), transform(fixture()))
            self.assertEqual(source.read_bytes(), before)
            alias = Path(directory) / 'alias.ytx'
            alias.symlink_to(source)
            hardlink = Path(directory) / 'hardlink.ytx'
            os.link(source, hardlink)
            for destination in [source, alias, hardlink]:
                run = subprocess.run([sys.executable, str(script), str(source), str(destination)], capture_output=True)
                self.assertNotEqual(run.returncode, 0)
                self.assertEqual(source.read_bytes(), before)
            source.write_text('{invalid json')
            invalid_output = Path(directory) / 'invalid-output.ytx'
            run = subprocess.run([sys.executable, str(script), str(source), str(invalid_output)], capture_output=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertFalse(invalid_output.exists())


if __name__ == '__main__':
    unittest.main()
