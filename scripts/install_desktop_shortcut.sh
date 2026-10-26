#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
LAUNCHER="$SCRIPT_DIR/launch.sh"
cd "$SCRIPT_DIR"
python3 - "$LAUNCHER" "$HOME" <<'PY'
from pathlib import Path
import sys
from launch_config import desktop_entry

launcher = Path(sys.argv[1]).resolve()
home = Path(sys.argv[2]).resolve()
entry = desktop_entry(launcher)
for folder in (home / "Desktop", home / ".local/share/applications"):
    folder.mkdir(parents=True, exist_ok=True)
    target = folder / "Custom-Doom.desktop"
    target.write_text(entry, encoding="utf-8")
    target.chmod(0o755)
    print(target)
PY
if command -v gio >/dev/null 2>&1; then
    gio set "$HOME/Desktop/Custom-Doom.desktop" metadata::trusted true 2>/dev/null || true
fi
