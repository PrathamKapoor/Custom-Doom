#!/usr/bin/env bash
# ==============================================================================
# scripts/setup_touch_screen.sh
# Configures XPT2046 480x320 touch calibration, coordinate transformation,
# rotation (270 degrees landscape), and user udev permissions.
# ==============================================================================

set -e

GREEN='\033[1;32m'
CYAN='\033[1;36m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
NC='\033[0m'

echo -e "${CYAN}========================================================================${NC}"
echo -e "${CYAN}     rpi-doom-touch: 3.5\" XPT2046 Touchscreen Calibration Setup         ${NC}"
echo -e "${CYAN}========================================================================${NC}"

if [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
else
    SUDO=""
fi

# 1. Configure user-space config in ~/.doom-touch/doom_touch.cfg
CONFIG_DIR="$HOME/.doom-touch"
mkdir -p "$CONFIG_DIR"
TOUCH_CFG="$CONFIG_DIR/doom_touch.cfg"

echo -e "\n${CYAN}[1/4] Writing touch settings to $TOUCH_CFG...${NC}"
cat << 'EOF' > "$TOUCH_CFG"
[touch]
enabled = 1
opacity = 0.55
dpad_deadzone = 15
turn_sensitivity = 1.2
screen_rotation = 270
EOF
echo -e "${GREEN}[OK] Configured screen_rotation = 270 (landscape), deadzone = 15px.${NC}"

# 2. Configure udev rule for touchscreen permissions
echo -e "\n${CYAN}[2/4] Setting up udev permissions for /dev/input/event*...${NC}"
UDEV_RULE_FILE="/etc/udev/rules.d/99-touchscreen-doom.rules"
$SUDO tee "$UDEV_RULE_FILE" > /dev/null << 'EOF'
# Grant video and input group access to ADS7846 / XPT2046 resistive touchscreen
SUBSYSTEM=="input", ATTRS{name}=="*ADS7846*", MODE="0660", GROUP="input"
SUBSYSTEM=="input", ATTRS{name}=="*XPT2046*", MODE="0660", GROUP="input"
SUBSYSTEM=="input", KERNEL=="event*", MODE="0660", GROUP="input"
EOF
$SUDO udevadm control --reload-rules || true
$SUDO udevadm trigger || true
echo -e "${GREEN}[OK] udev rules installed at $UDEV_RULE_FILE.${NC}"

# 3. Configure X11 calibration and transformation matrix (if Xorg is present)
X11_CONF_DIR="/etc/X11/xorg.conf.d"
if [ -d "/etc/X11" ]; then
    echo -e "\n${CYAN}[3/4] Configuring X11 480x320 Touch Transformation Matrix...${NC}"
    $SUDO mkdir -p "$X11_CONF_DIR"
    CALIB_FILE="$X11_CONF_DIR/99-calibration.conf"
    $SUDO tee "$CALIB_FILE" > /dev/null << 'EOF'
Section "InputClass"
        Identifier      "calibration"
        MatchProduct    "ADS7846 Touchscreen|XPT2046"
        Option  "Calibration"   "3936 227 268 3957"
        Option  "SwapAxes"      "1"
        Option  "InvertX"       "1"
        Option  "InvertY"       "0"
        Option  "TransformationMatrix" "0 -1 1 1 0 0 0 0 1"
EndSection
EOF
    echo -e "${GREEN}[OK] Calibration written to $CALIB_FILE.${NC}"
fi

# 4. Check evtest input devices
echo -e "\n${CYAN}[4/4] Detecting connected input devices...${NC}"
if [ -d "/dev/input" ]; then
    ls -l /dev/input/event* 2>/dev/null || echo "No /dev/input/event* nodes found yet."
fi

echo -e "\n${GREEN}========================================================================${NC}"
echo -e "${GREEN}  Touchscreen configuration completed!                                  ${NC}"
echo -e "${GREEN}  Screen resolution: 480x320 | Orientation: 270° Landscape             ${NC}"
echo -e "${GREEN}========================================================================${NC}\n"
