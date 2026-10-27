import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import sys

from scripts.launch_config import desktop_entry, missing_requirements, select_iwad
from scripts import launch_config

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import game_session


class LauncherTests(unittest.TestCase):
    def test_prefers_crispy_and_falls_back_to_chocolate(self):
        self.assertTrue(hasattr(launch_config, 'select_engine'))
        both = {'crispy-doom': '/games/crispy', 'chocolate-doom': '/games/chocolate'}
        self.assertEqual(launch_config.select_engine(both.get), ('/games/crispy', True))
        self.assertEqual(launch_config.select_engine({'chocolate-doom': '/games/chocolate'}.get), ('/games/chocolate', False))

    def test_crispy_configuration_copies_mouse_settings_only_on_first_launch(self):
        self.assertTrue(hasattr(launch_config, 'prepare_crispy_config'))
        with tempfile.TemporaryDirectory() as td:
            home = Path(td)
            old = home / '.local/share/chocolate-doom'
            old.mkdir(parents=True)
            (old / 'default.cfg').write_text('mouseb_forward 6\n')
            (old / 'chocolate-doom.cfg').write_text('mouseb_use 2\n')
            main, extra = launch_config.prepare_crispy_config(home)
            self.assertIn('mouseb_forward 6', main.read_text())
            self.assertIn('mouseb_use 2', extra.read_text())
            self.assertIn('crispy_mouselook 1', extra.read_text())
            self.assertIn('crispy_freeaim 1', extra.read_text())
            extra.write_text('user customized\n')
            launch_config.prepare_crispy_config(home)
            self.assertEqual(extra.read_text(), 'user customized\n')

    def test_fresh_crispy_config_has_playable_mouse_defaults(self):
        with tempfile.TemporaryDirectory() as td:
            main, extra = launch_config.prepare_crispy_config(Path(td))
            self.assertIn('mouseb_forward 6', main.read_text())
            self.assertIn('mouseb_use 2', extra.read_text())
            self.assertIn('mouseb_backward 5', extra.read_text())
            self.assertIn('mouseb_prevweapon 3', extra.read_text())
            self.assertIn('mouseb_nextweapon 4', extra.read_text())

    def test_iwad_override_precedes_system_and_repo(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            custom, repo_wad = root / "custom.wad", root / "wads" / "DOOM1.WAD"
            custom.touch()
            repo_wad.parent.mkdir()
            repo_wad.touch()
            self.assertEqual(select_iwad(root, {"DOOM_IWAD": str(custom)}), custom)
            self.assertEqual(select_iwad(root, {}), repo_wad)

    def test_missing_requirements_are_clear(self):
        self.assertEqual(missing_requirements(None, None, None), [
            "crispy-doom or chocolate-doom", "Freedoom or a user-provided IWAD", "an active X11 display (DISPLAY)"])

    def test_desktop_entry_quotes_paths(self):
        entry = desktop_entry(Path('/home/admin/My Doom/scripts/launch.sh'))
        self.assertIn('Exec="/home/admin/My Doom/scripts/launch.sh"', entry)
        self.assertTrue(entry.endswith("\n"))

    def test_game_disables_sdl_touch_to_mouse_synthesis(self):
        with patch.object(game_session.shutil, "which", return_value="/usr/games/chocolate-doom"), \
             patch.object(game_session, "select_engine", return_value=("/usr/games/chocolate-doom", False)), \
             patch.object(game_session, "select_iwad", return_value=Path("/tmp/game.wad")), \
             patch.object(game_session, "missing_requirements", return_value=[]), \
             patch.object(game_session.subprocess, "Popen") as popen, \
             patch.object(game_session.time, "sleep"):
            popen.return_value.poll.return_value = 1
            popen.return_value.returncode = 1
            game_session.main()
            self.assertEqual(popen.call_args.kwargs.get("env", {}).get("SDL_TOUCH_MOUSE_EVENTS"), "0")


if __name__ == "__main__":
    unittest.main()
