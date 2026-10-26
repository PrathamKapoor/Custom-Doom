import tempfile
import unittest
from pathlib import Path

from scripts.launch_config import desktop_entry, missing_requirements, select_iwad


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


if __name__ == "__main__":
    unittest.main()
