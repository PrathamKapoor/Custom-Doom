#!/usr/bin/env bash
set -euo pipefail
cat <<'EOF'
This project does not download or redistribute Doom IWAD files.

Install the free game data package with:
  sudo apt-get install freedoom

The launcher detects Freedoom automatically. To use a game file you are
entitled to use, set DOOM_IWAD=/path/to/your/game.wad when launching.
EOF
