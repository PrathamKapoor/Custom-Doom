#!/usr/bin/env bash
# ==============================================================================
# scripts/launch.sh
# Optimized launcher for rpi-doom-touch on Raspberry Pi 4B
# Auto-detects X11 desktop vs KMSDRM / Framebuffer console, configures SDL2,
# and starts Doom at 480x320 with touch HUD and keypad driver.
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$REPO_ROOT/doom-rpi-touch"

GREEN='\033[1;32m'
CYAN='\033[1;36m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
NC='\033[0m'

echo -e "${CYAN}========================================================================${NC}"
echo -e "${CYAN}             Launching DOOM - Touch Edition (RPi 4B)                    ${NC}"
echo -e "${CYAN}========================================================================${NC}"

# 1. Build executable if missing
if [ ! -f "$BIN" ]; then
    echo -e "${YELLOW}[BUILD] Binary '$BIN' not found. Compiling now...${NC}"
    cd "$REPO_ROOT"
    make
fi

# 2. Check for DOOM IWAD
IWAD_USER="$HOME/.doom-touch/DOOM1.WAD"
IWAD_REPO="$REPO_ROOT/wads/DOOM1.WAD"
if [ ! -f "$IWAD_USER" ] && [ ! -f "$IWAD_REPO" ]; then
    echo -e "${YELLOW}[WARNING] No DOOM1.WAD detected.${NC}"
    echo -e "Would you like to download shareware DOOM1.WAD now? (y/n)"
    read -r -t 5 response || response="y"
    if [[ "$response" =~ ^[Yy]$ ]]; then
        "$REPO_ROOT/scripts/download_wad.sh" || true
    fi
fi

# 3. Environment detection: X11 vs Pure Linux Console / KMSDRM
if [ -n "$DISPLAY" ]; then
    echo -e "${GREEN}[DISPLAY] Running under X11 Desktop ($DISPLAY).${NC}"
    export SDL_VIDEODRIVER="x11"
else
    echo -e "${GREEN}[DISPLAY] Running in Framebuffer / KMSDRM Console mode.${NC}"
    # Check for dedicated SPI display framebuffer (typically /dev/fb1)
    if [ -e "/dev/fb1" ]; then
        echo -e "${GREEN}[DISPLAY] Detected SPI TFT framebuffer at /dev/fb1.${NC}"
        export SDL_FBDEV="/dev/fb1"
        export SDL_VIDEODRIVER="fbcon"
    else
        export SDL_VIDEODRIVER="KMSDRM"
    fi
    export DOOM_FULLSCREEN="1"
fi

# 4. Audio Driver Configuration
if [ -z "$SDL_AUDIODRIVER" ]; then
    export SDL_AUDIODRIVER="alsa"
fi

# 5. Touch input device detection (if TSLIB or evdev is used)
if [ -e "/dev/input/touchscreen" ]; then
    export TSLIB_TSDEVICE="/dev/input/touchscreen"
    export SDL_MOUSEDEV="/dev/input/touchscreen"
fi

# 6. Launch Game Engine
cd "$REPO_ROOT"
echo -e "\n${CYAN}[START] Executing: $BIN $@${NC}\n"
exec "$BIN" "$@"
