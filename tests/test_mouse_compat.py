import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import launch_config


class MouseCompatTests(unittest.TestCase):
    def test_legacy_crispy_moves_side_buttons_out_of_wheel_slots(self):
        self.assertTrue(hasattr(launch_config, 'legacy_mouse_plan'))
        listing = '  ↳ Gaming Mouse id=12 [slave pointer (2)]\n  ↳ Gaming Mouse Keyboard id=10 [slave pointer (2)]'
        maps = {'12': '1 2 3 4 5 6 7 8 9', '10': '1 2 3 4 5 6 7 8 9'}
        plan = launch_config.legacy_mouse_plan('5.12.0-1', listing, maps)
        self.assertEqual(plan, [('12', '1 2 3 4 5 6 7 8 9', '1 2 3 4 5 6 7 10 11')])

    def test_new_versions_and_custom_maps_are_not_modified(self):
        self.assertTrue(hasattr(launch_config, 'legacy_mouse_plan'))
        listing = '  ↳ Gaming Mouse id=12 [slave pointer (2)]'
        self.assertEqual(launch_config.legacy_mouse_plan('7.2-1', listing, {'12': '1 2 3 4 5 6 7 8 9'}), [])
        self.assertEqual(launch_config.legacy_mouse_plan('5.12.0-1', listing, {'12': '1 2 3 4 5 6 7 9 8'}), [])


if __name__ == '__main__':
    unittest.main()
