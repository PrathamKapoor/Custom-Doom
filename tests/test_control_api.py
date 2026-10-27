import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import control_api


class ControlApiTests(unittest.TestCase):
    def test_validates_bindings_and_rejects_conflicts(self):
        self.assertTrue(hasattr(control_api, 'validate_settings'))
        data = {'forward': 6, 'backward': 5, 'use': 2, 'mouselook': True, 'freeaim': True, 'hires': False, 'sensitivity': 5}
        self.assertEqual(control_api.validate_settings(data), data)
        data['use'] = 6
        with self.assertRaises(ValueError):
            control_api.validate_settings(data)

    def test_update_preserves_other_settings_and_adds_missing(self):
        self.assertTrue(hasattr(control_api, 'update_config'))
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / 'config.cfg'
            path.write_text('mouseb_forward 2\nsfx_volume 8\n')
            control_api.update_config(path, {'mouseb_forward': 6, 'crispy_mouselook': 1})
            contents = path.read_text()
            self.assertIn('mouseb_forward 6\n', contents)
            self.assertIn('sfx_volume 8\n', contents)
            self.assertIn('crispy_mouselook 1\n', contents)


if __name__ == '__main__':
    unittest.main()
