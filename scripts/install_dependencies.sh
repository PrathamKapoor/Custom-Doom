#!/usr/bin/env bash
# ==============================================================================
# scripts/install_dependencies.sh
# Automated dependency installation for Raspberry Pi OS Legacy (32-bit armhf)
# Targets: Raspberry Pi 4 Model B with 3.5" SPI TFT & 4x4 Matrix Keypad
# ==============================================================================

set -e

# ANSI Color Codes
GREEN='\033[1;32m'
CYAN='\033[1;36m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}========================================================================${NC}"
echo -e "${CYAN}    rpi-doom-touch: Raspberry Pi OS Dependency Installer                ${NC}"
echo -e "${CYAN}    Target: Raspberry Pi 4B (3.5\" 480x320 SPI TFT + 4x4 Keypad)        ${NC}"
echo -e "${CYAN}========================================================================${NC}"

# Check for root / sudo privileges
if [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
    echo -e "${YELLOW}[INFO] Script will request sudo privileges for apt installations.${NC}"
else
    SUDO=""
fi

echo -e "\n${CYAN}[1/5] Updating APT package repositories...${NC}"
$SUDO apt-get update -y

echo -e "\n${CYAN}[2/5] Installing core build system and compiler toolchains...${NC}"
$SUDO apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    g++ \
    make \
    cmake \
    pkg-config \
    git \
    wget \
    curl \
    unzip

echo -e "\n${CYAN}[3/5] Installing SDL2, Audio, and Graphics Libraries...${NC}"
$SUDO apt-get install -y --no-install-recommends \
    libsdl2-dev \
    libsdl2-mixer-dev \
    libsdl2-image-dev \
    libasound2-dev

echo -e "\n${CYAN}[4/5] Installing GPIO, evdev, and touchscreen calibration tools...${NC}"
$SUDO apt-get install -y --no-install-recommends \
    libgpiod-dev \
    gpiod \
    evtest \
    xinput-calibrator \
    python3-pip \
    python3-setuptools

# Optional Python GPIO packages
$SUDO apt-get install -y python3-rpi.gpio python3-gpiod || true

echo -e "\n${CYAN}[5/5] Configuring system permissions and hardware interfaces...${NC}"

# 1. Ensure SPI interface is enabled in raspi-config
if command -v raspi-config >/dev/null 2>&1; then
    echo -e "${YELLOW}[CONFIG] Enabling SPI bus via raspi-config...${NC}"
    $SUDO raspi-config nonint do_spi 0 || true
fi

# 2. Add current user to hardware groups for conflict-free direct access
CURRENT_USER="${SUDO_USER:-$USER}"
if [ -n "$CURRENT_USER" ]; then
    echo -e "${YELLOW}[CONFIG] Adding user '$CURRENT_USER' to gpio, input, and video groups...${NC}"
    $SUDO usermod -a -G gpio,input,video "$CURRENT_USER" || true
fi

# 3. Create default configuration folder
CONFIG_DIR="$HOME/.doom-touch"
mkdir -p "$CONFIG_DIR"
mkdir -p "$CONFIG_DIR/wads"

# 4. Check /boot/config.txt for SPI display overlay
BOOT_CONFIG="/boot/config.txt"
[ ! -f "$BOOT_CONFIG" ] && BOOT_CONFIG="/boot/firmware/config.txt"

if [ -f "$BOOT_CONFIG" ]; then
    if grep -q "piscreen\|tft35a\|dtoverlay=piscreen" "$BOOT_CONFIG"; then
        echo -e "${GREEN}[OK] 3.5\" SPI Display overlay already detected in $BOOT_CONFIG.${NC}"
    else
        echo -e "${YELLOW}[NOTICE] 3.5\" SPI Display overlay was not found in $BOOT_CONFIG.${NC}"
        echo -e "If your 3.5\" TFT screen requires an overlay, add the following to $BOOT_CONFIG:"
        echo -e "  ${CYAN}dtoverlay=piscreen,speed=16000000,rotate=270${NC}"
    fi
fi

# 5. PIN SAFETY NOTICE
echo -e "\n${RED}========================================================================${NC}"
echo -e "${RED}[CRITICAL PIN SAFETY NOTICE]${NC}"
echo -e "Physical Pin 22 (BCM GPIO 25) is strictly reserved for LCD Data/Command (D/C)."
echo -e "The 4x4 matrix keypad MUST use GPIO 5 (Physical Pin 29) for Column 2."
echo -e "Never connect Keypad Wire 6 to Physical Pin 22!"
echo -e "${RED}========================================================================${NC}"

echo -e "\n${GREEN}========================================================================${NC}"
echo -e "${GREEN}  All dependencies installed successfully!                              ${NC}"
echo -e "${GREEN}  Next step: run ./scripts/download_wad.sh to fetch shareware DOOM1.WAD  ${NC}"
echo -e "${GREEN}========================================================================${NC}\n"
