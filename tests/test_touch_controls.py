import unittest

from scripts.touch_controls import TouchControls, calibrate, find_touch_device


class TouchControlsTests(unittest.TestCase):
    def test_calibration_clamps_and_maps(self):
        self.assertEqual(calibrate(3932, 3932, 300, 294, 294, 3801), (0, 0))
        self.assertEqual(calibrate(300, 3932, 300, 3801, 294, 3801), (479, 319))

    def test_floating_stick_anchor_deadzone_and_follow(self):
        c = TouchControls(480, 320)
        c.set_game_active(True)
        c.down(80, 210)
        self.assertEqual(c.stick_anchor, (80, 210))
        c.move(84, 211)
        self.assertEqual(c.movement, (0.0, 0.0))
        c.move(80, 160)
        self.assertGreater(c.movement[1], 0.7)
        self.assertEqual(c.stick_knob, (80, 160))

    def test_right_swipe_aim_and_action_regions(self):
        c = TouchControls(480, 320)
        c.set_game_active(True)
        c.down(250, 150)
        c.move(270, 150)
        self.assertEqual(c.look_delta, 20)
        c.up()
        c.down(410, 205)
        self.assertIn("fire", c.keys)
        c.up()
        self.assertNotIn("fire", c.held_actions)

    def test_auto_fire_works_while_moving_single_touch(self):
        c = TouchControls(480, 320)
        c.set_game_active(True)
        c.auto_fire = True
        c.down(70, 230)
        c.move(70, 170)
        self.assertIn("fire", c.keys)
        self.assertGreater(c.movement[1], 0)

    def test_pause_hides_controls_and_resume_restores(self):
        c = TouchControls(480, 320)
        self.assertFalse(c.visible)
        c.set_game_active(True)
        self.assertTrue(c.visible)
        c.pause()
        self.assertFalse(c.visible)
        c.resume()
        self.assertTrue(c.visible)
        c.set_game_active(False)
        self.assertFalse(c.visible)

    def test_cancel_releases_everything(self):
        c = TouchControls(480, 320)
        c.set_game_active(True)
        c.down(410, 205)
        c.cancel()
        self.assertFalse(c.held_actions)
        self.assertIsNone(c.stick_anchor)

    def test_touch_device_discovery_and_override(self):
        devices = [
            {"path": "/dev/input/event0", "name": "gpio-keys", "absolute": True},
            {"path": "/dev/input/event2", "name": "ADS7846 Touchscreen", "absolute": True},
        ]
        self.assertEqual(find_touch_device(devices), "/dev/input/event2")
        self.assertEqual(find_touch_device(devices, "/dev/input/event9"), "/dev/input/event9")
        self.assertIsNone(find_touch_device([devices[0]]))


if __name__ == "__main__":
    unittest.main()
