"""Dependency and IWAD selection helpers shared by launcher and tests."""
from pathlib import Path
import os


def select_iwad(repo: Path, environ: dict | None = None) -> Path | None:
    env = os.environ if environ is None else environ
    candidates = []
    if env.get("DOOM_IWAD"):
        candidates.append(Path(env["DOOM_IWAD"]).expanduser())
    candidates += [
        Path.home() / ".doom-touch" / "DOOM1.WAD",
        Path("/usr/share/games/freedoom/freedoom1.wad"),
        Path("/usr/share/games/doom/freedoom1.wad"),
        Path("/usr/share/games/doom/DOOM1.WAD"),
        repo / "wads" / "DOOM1.WAD",
    ]
    return next((p.resolve() for p in candidates if p.is_file()), None)


def missing_requirements(chocolate_doom: str | None, iwad: Path | None, display: str | None) -> list[str]:
    missing = []
    if not chocolate_doom:
        missing.append("chocolate-doom")
    if iwad is None:
        missing.append("Freedoom or a user-provided IWAD")
    if not display:
        missing.append("an active X11 display (DISPLAY)")
    return missing


def desktop_entry(exec_path: Path, name: str = "Custom Doom") -> str:
    def quote(value: str) -> str:
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return "\n".join((
        "[Desktop Entry]", "Version=1.0", "Type=Application", f"Name={name}",
        f"Exec={quote(exec_path.as_posix())}", "Terminal=false", "Categories=Game;",
        "Icon=applications-games", "StartupNotify=true", "",))
