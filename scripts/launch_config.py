"""Dependency and IWAD selection helpers shared by launcher and tests."""
from pathlib import Path
import os
import re
import shutil
import subprocess


def select_engine(which):
    crispy = which("crispy-doom")
    if crispy:
        return crispy, True
    return which("chocolate-doom"), False


def legacy_mouse_plan(version: str, listing: str, maps: dict[str, str]):
    """Avoid side-button/wheel collisions in the Bookworm Crispy 5.12 package."""
    if version.split("-", 1)[0] != "5.12.0":
        return []
    original = "1 2 3 4 5 6 7 8 9"
    remapped = "1 2 3 4 5 6 7 10 11"
    plan = []
    for line in listing.splitlines():
        match = re.search(r"id=(\d+)\s+\[slave\s+pointer", line)
        name = line.split("id=", 1)[0].lower()
        if not match or "mouse" not in name or any(x in name for x in ("keyboard", "virtual", "custom doom")):
            continue
        device_id = match.group(1)
        current = " ".join(maps.get(device_id, "").split())
        if current in (original, remapped):
            plan.append((device_id, original, remapped))
    return plan


def apply_legacy_mouse_workaround():
    try:
        version = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "crispy-doom"], capture_output=True, text=True, check=True).stdout
        listing = subprocess.run(["xinput", "list", "--short"], capture_output=True, text=True, check=True).stdout
        ids = re.findall(r"id=(\d+)\s+\[slave\s+pointer", listing)
        maps = {device_id: subprocess.run(["xinput", "get-button-map", device_id], capture_output=True, text=True, check=True).stdout for device_id in ids}
        applied = []
        for device_id, original, target in legacy_mouse_plan(version, listing, maps):
            if subprocess.run(["xinput", "set-button-map", device_id, *target.split()], check=False, capture_output=True).returncode == 0:
                applied.append((device_id, original, target))
        return applied
    except (OSError, subprocess.SubprocessError):
        return []


def restore_mouse_buttons(applied):
    for device_id, original, target in applied:
        try:
            current = subprocess.run(["xinput", "get-button-map", device_id], capture_output=True, text=True, check=True).stdout
            if " ".join(current.split()) == target:
                subprocess.run(["xinput", "set-button-map", device_id, *original.split()], check=False, capture_output=True)
        except (OSError, subprocess.SubprocessError):
            pass


def prepare_crispy_config(home: Path) -> tuple[Path, Path]:
    """Seed a separate Crispy config once; later edits belong to the player."""
    state = home / ".doom-touch"
    state.mkdir(parents=True, exist_ok=True)
    chocolate = home / ".local" / "share" / "chocolate-doom"
    main, extra = state / "crispy-default.cfg", state / "crispy-extra.cfg"
    for target, source in ((main, chocolate / "default.cfg"),
                           (extra, chocolate / "chocolate-doom.cfg")):
        if not target.exists():
            if source.is_file():
                shutil.copyfile(source, target)
            else:
                defaults = ("mouseb_forward 6\nmouseb_speed -1\n" if target == main else
                            "mouseb_use 2\nmouseb_backward 5\nmouseb_prevweapon 3\nmouseb_nextweapon 4\n")
                target.write_text(defaults, encoding="utf-8")
            if target == extra:
                with target.open("a", encoding="utf-8") as config:
                    config.write("\ncrispy_freelook 1\ncrispy_mouselook 1\ncrispy_freeaim 1\ncrispy_hires 0\n")
    return main, extra


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
        missing.append("crispy-doom or chocolate-doom")
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
