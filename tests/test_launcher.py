import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import sys

from scripts.launch_config import desktop_entry, missing_requirements, select_iwad

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import game_session


class LauncherTests(unittest.TestCase):
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
            "chocolate-doom", "Freedoom or a user-provided IWAD", "an active X11 display (DISPLAY)"])

    def test_desktop_entry_quotes_paths(self):
        entry = desktop_entry(Path('/home/admin/My Doom/scripts/launch.sh'))
        self.assertIn('Exec="/home/admin/My Doom/scripts/launch.sh"', entry)
        self.assertTrue(entry.endswith("\n"))

    def test_game_disables_sdl_touch_to_mouse_synthesis(self):
        with patch.object(game_session.shutil, "which", return_value="/usr/games/chocolate-doom"), \
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
