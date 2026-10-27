import unittest

from scripts.keypad_controls import (
    KEYMAP,
    KEYPAD_COLS,
    KEYPAD_KEYS,
    KEYPAD_LAYOUT,
    KEYPAD_ROWS,
    MatrixDebouncer,
    key_names_for,
    validate_gpio_pin_map,
)
from scripts.keypad_bridge import scan_matrix


class FakeGPIO:
    BCM, OUT, IN, LOW, HIGH, PUD_OFF = 11, 1, 0, 0, 1, 0

    def __init__(self, keys):
        self.keys = set(keys)
        self.modes = {}
        self.active_rows = []

    def setup(self, pin, mode, initial=None, pull_up_down=None):
        if mode == self.OUT and initial != self.LOW:
            raise AssertionError("keypad scanner may only drive rows low")
        self.modes[pin] = mode
        self.active_rows = [p for p, m in self.modes.items() if m == self.OUT]

    def input(self, pin):
        if len(self.active_rows) > 1:
            raise AssertionError("scanner drove multiple rows at once")
        row = self.active_rows[0] if self.active_rows else None
        if row is None:
            return self.HIGH
        row_index = KEYPAD_ROWS.index(row)
        col_index = KEYPAD_COLS.index(pin)
        return self.LOW if KEYPAD_LAYOUT[row_index][col_index] in self.keys else self.HIGH


class KeypadControlsTests(unittest.TestCase):
    def test_gpio_mapping_matches_verified_physical_wiring(self):
        self.assertEqual(KEYPAD_ROWS, (21, 20, 16, 19))
        self.assertEqual(KEYPAD_COLS, (13, 12, 6, 5))
        self.assertEqual(validate_gpio_pin_map(KEYPAD_ROWS, KEYPAD_COLS), True)

    def test_full_key_layout_and_playable_bindings(self):
        self.assertEqual(KEYPAD_KEYS, ("1", "2", "3", "A", "4", "5", "6", "B",
                                      "7", "8", "9", "C", "*", "0", "#", "D"))
        self.assertEqual(set(KEYMAP), set(KEYPAD_KEYS))
        self.assertEqual(KEYMAP["8"], ("KEY_UP",))
        self.assertEqual(KEYMAP["2"], ("KEY_DOWN",))
        self.assertEqual(KEYMAP["4"], ("KEY_LEFT",))
        self.assertEqual(KEYMAP["6"], ("KEY_RIGHT",))
        self.assertEqual(KEYMAP["5"], ("KEY_LEFTCTRL",))
        self.assertEqual(KEYMAP["0"], ("KEY_SPACE",))
        self.assertEqual(KEYMAP["#"], ("KEY_TAB",))
        self.assertEqual(KEYMAP["C"], ("KEY_PAUSE",))
        self.assertEqual(KEYMAP["D"], ("KEY_ESC",))

    def test_simultaneous_move_and_fire_and_strafe_chords(self):
        self.assertEqual(key_names_for({"8", "5"}), {"KEY_UP", "KEY_LEFTCTRL"})
        self.assertEqual(key_names_for({"1", "3"}),
                         {"KEY_LEFTALT", "KEY_LEFT", "KEY_RIGHT"})

    def test_invalid_or_duplicate_gpio_maps_are_rejected(self):
        with self.assertRaises(ValueError):
            validate_gpio_pin_map((21, 20, 25, 19), KEYPAD_COLS)
        with self.assertRaises(ValueError):
            validate_gpio_pin_map((21, 20, 16, 19), (13, 12, 6, 19))

    def test_debounce_suppresses_short_contact_and_emits_press_release(self):
        debouncer = MatrixDebouncer(debounce_ms=20)
        self.assertEqual(debouncer.update({"5"}, 0), ())
        self.assertEqual(debouncer.update(set(), 8), ())
        self.assertEqual(debouncer.update(set(), 30), ())
        self.assertEqual(debouncer.update({"5"}, 40), ())
        self.assertEqual(debouncer.update({"5"}, 59), ())
        self.assertEqual(debouncer.update({"5"}, 60), (("5", True),))
        self.assertEqual(debouncer.update(set(), 61), ())
        self.assertEqual(debouncer.update(set(), 81), (("5", False),))

    def test_hardware_scan_drives_one_low_row_then_releases_it(self):
        gpio = FakeGPIO({"8", "5"})
        self.assertEqual(scan_matrix(gpio), frozenset({"8", "5"}))
        self.assertFalse(gpio.active_rows)
        self.assertTrue(all(gpio.modes[row] == gpio.IN for row in KEYPAD_ROWS))


if __name__ == "__main__":
    unittest.main()
