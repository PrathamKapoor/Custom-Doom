import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import touch_hud
from touch_controls import TouchControls


class FakeDevice:
    def __init__(self, **kwargs):
        self.options = kwargs
        self.events = []
        self.closed = False

    def write(self, *event):
        self.events.append(event)

    def syn(self):
        self.events.append(("sync",))

    def close(self):
        self.closed = True


class HudDeviceTests(unittest.TestCase):
    def setUp(self):
        names = {name for values in touch_hud.KEYS.values() for name in values}
        names.update("KEY_" + chr(c) for c in range(ord("A"), ord("Z") + 1))
        names.update(("KEY_PAUSE", "BTN_LEFT", "REL_X", "REL_Y", "EV_KEY", "EV_REL"))
        self.codes = SimpleNamespace(**{name: i for i, name in enumerate(sorted(names), 1)})
        self.evdev = SimpleNamespace(ecodes=self.codes, UInput=FakeDevice)

    def test_virtual_keyboard_and_mouse_have_desktop_capabilities(self):
        self.assertTrue(hasattr(touch_hud, "create_virtual_inputs"))
        keyboard, mouse = touch_hud.create_virtual_inputs(self.evdev)
        self.assertIn(self.codes.KEY_A, keyboard.options["events"][self.codes.EV_KEY])
        self.assertIn(self.codes.KEY_Z, keyboard.options["events"][self.codes.EV_KEY])
        self.assertIn(self.codes.BTN_LEFT, mouse.options["events"][self.codes.EV_KEY])
        self.assertEqual(set(mouse.options["events"][self.codes.EV_REL]), {self.codes.REL_X, self.codes.REL_Y})

    def test_movement_and_look_go_to_separate_devices_and_release(self):
        self.assertTrue(hasattr(touch_hud, "create_virtual_inputs"))
        keyboard, mouse = touch_hud.create_virtual_inputs(self.evdev)
        overlay = touch_hud.Overlay.__new__(touch_hud.Overlay)
        overlay.controls = TouchControls()
        overlay.controls.set_game_active(True)
        overlay.ui, overlay.mouse_ui, overlay.evdev = keyboard, mouse, self.evdev
        overlay.last_keys, overlay.last_pause_events = set(), 0
        overlay.controls.down(70, 230)
        overlay.controls.move(70, 170)
        overlay.emit()
        self.assertIn((self.codes.EV_KEY, self.codes.KEY_UP, 1), keyboard.events)
        self.assertNotIn((self.codes.EV_KEY, self.codes.KEY_LEFTCTRL, 1), keyboard.events)
        overlay.controls.up()
        overlay.emit()
        self.assertIn((self.codes.EV_KEY, self.codes.KEY_UP, 0), keyboard.events)
        overlay.controls.down(270, 160)
        overlay.controls.move(290, 160)
        overlay.emit()
        self.assertIn((self.codes.EV_REL, self.codes.REL_X, 30), mouse.events)


if __name__ == "__main__":
    unittest.main()
