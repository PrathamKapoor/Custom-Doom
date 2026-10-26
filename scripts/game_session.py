#!/usr/bin/env python3
"""Start installed Chocolate Doom and keep the touch HUD tied to its lifetime."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from launch_config import missing_requirements, select_iwad


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    game = shutil.which("chocolate-doom")
    iwad = select_iwad(repo)
    missing = missing_requirements(game, iwad, os.environ.get("DISPLAY"))
    if missing:
        message = "Custom Doom needs: " + ", ".join(missing) + ". Run scripts/install_dependencies.sh first."
        print(message, file=sys.stderr)
        if os.environ.get("DISPLAY") and shutil.which("zenity"):
            subprocess.run(["zenity", "--error", "--title=Custom Doom", f"--text={message}"], check=False)
        return 2

    game_process = subprocess.Popen([game, "-iwad", str(iwad), "-fullscreen", "-warp", "1"])
    hud_log = None
    try:
        time.sleep(1.0)
        if game_process.poll() is not None:
            return int(game_process.returncode or 1)
        state_dir = Path.home() / ".doom-touch"
        state_dir.mkdir(parents=True, exist_ok=True)
        hud_log = (state_dir / "touch-hud.log").open("a", encoding="utf-8")
        hud = subprocess.Popen([sys.executable, str(repo / "scripts" / "touch_hud.py")],
                               cwd=repo, stdout=hud_log, stderr=subprocess.STDOUT)
        time.sleep(0.5)
        if hud.poll() is not None and shutil.which("zenity"):
            subprocess.run(["zenity", "--error", "--title=Custom Doom Touch",
                            f"--text=The touch HUD could not start. See {state_dir / 'touch-hud.log'}"], check=False)
        result = game_process.wait()
        return int(result)
    except KeyboardInterrupt:
        game_process.terminate()
        return 130
    finally:
        if game_process.poll() is None:
            game_process.terminate()
            try: game_process.wait(timeout=4)
            except subprocess.TimeoutExpired: game_process.kill()
        if "hud" in locals() and hud.poll() is None:
            hud.terminate()
            try: hud.wait(timeout=2)
            except subprocess.TimeoutExpired: hud.kill()
        if hud_log is not None:
            hud_log.close()


if __name__ == "__main__":
    raise SystemExit(main())
