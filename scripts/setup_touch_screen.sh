#!/usr/bin/env bash
set -euo pipefail
cat <<'EOF'
Display boot overlays and X11 calibration are specific to the exact screen board.
This project does not overwrite Raspberry Pi OS boot or Xorg configuration.

The touch HUD uses the measured 480x320 ADS7846 calibration in:
  scripts/touch_hud.py

If the screen orientation or calibration differs, adjust those constants after
checking the touchscreen's raw axes with evtest. The input device can be
selected with DOOM_TOUCH_DEVICE=/dev/input/eventN.
EOF
