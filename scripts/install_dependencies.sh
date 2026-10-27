#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -eq 0 ]]; then SUDO=; else SUDO=sudo; fi
$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends crispy-doom chocolate-doom freedoom python3-tk python3-evdev python3-rpi-lgpio xcompmgr xinput

CURRENT_USER="${SUDO_USER:-${USER:-}}"
if [[ -n "$CURRENT_USER" && "$CURRENT_USER" != root ]]; then
    $SUDO usermod -a -G input,gpio "$CURRENT_USER"
fi

RULE='KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"'
printf '%s\n' "$RULE" | $SUDO tee /etc/udev/rules.d/70-custom-doom-uinput.rules >/dev/null
$SUDO udevadm control --reload-rules
if [[ -e /dev/uinput ]]; then
    $SUDO chgrp input /dev/uinput
    $SUDO chmod 0660 /dev/uinput
fi

if [[ -n "${DISPLAY:-}" ]] && ! pgrep -x xcompmgr >/dev/null 2>&1; then
    xcompmgr -aR >/dev/null 2>&1 &
fi

printf '\nInstalled Crispy Doom, Chocolate Doom, Freedoom, Tk, evdev, RPi LGPIO, and X11 compositing support.\n'
printf 'Log out and back in if this user was just added to the input or gpio group.\n'
printf 'Then run scripts/install_desktop_shortcut.sh from this checkout.\n'
