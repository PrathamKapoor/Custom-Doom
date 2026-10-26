# Custom Doom Touch

Play Doom on Raspberry Pi OS with the system Chocolate Doom port and a compact translucent touch overlay. The repository contains the launcher and controls; it does not contain the Doom engine or game data. The included C program is an experimental raycasting chamber, not a Doom port.

## Target setup

- Raspberry Pi 4B running Raspberry Pi OS with an active X11 desktop.
- 3.5-inch 480×320 SPI display with an XPT2046/ADS7846 resistive touchscreen.
- One touch is available at a time. The controls use a floating movement pad, swipe-to-turn, held FIRE/USE, map, pause/resume, and optional auto-fire.
- Keyboard input remains available. This overlay does not access GPIO or configure the display boot driver.

## Install on Raspberry Pi OS

Clone this repository and run:

```sh
cd Custom-Doom
./scripts/install_dependencies.sh
```

The installer uses Raspberry Pi OS packages for Chocolate Doom, Freedoom, Tk, evdev, and X11 compositing. It adds the logged-in user to the `input` group if needed and installs a udev rule limited to `/dev/uinput`. Log out and back in if the installer just added the group membership.

Install the desktop and application-menu shortcuts:

```sh
./scripts/install_desktop_shortcut.sh
```

Open **Custom Doom** from the desktop. The launcher starts map 1 fullscreen, then displays the HUD for the lifetime of the game. The touch overlay is not shown on the desktop or after the game closes.

## Game data

The dependency installer installs Freedoom, which supplies freely distributable Doom-compatible game data. The launcher checks common Raspberry Pi OS Freedoom locations. You can select another IWAD without copying it into the repository:

```sh
DOOM_IWAD="$HOME/.doom-touch/DOOM.WAD" ./scripts/launch.sh
```

The launcher never downloads or commits an IWAD. `scripts/download_wad.sh` is retained as a compatibility helper but only points to the Freedoom package; it does not fetch copyrighted game files.

## Touch controls

- Touch and drag in the lower-left play area: floating movement stick (forward/back and strafe).
- Touch and drag on the right half: turn the view.
- FIRE and USE: hold while touching the on-screen button.
- AUTO: toggle continuous fire so the single touch can stay on movement while firing.
- MAP: toggle the automap.
- Ⅱ: pause the game; touch anywhere to resume.

The overlay uses the ADS7846 calibration measured for this display. Set `DOOM_TOUCH_DEVICE=/dev/input/eventN` if automatic touchscreen discovery selects the wrong device. The default calibration constants are in `scripts/touch_hud.py` for adjustment when the panel orientation or calibration differs.

## Requirements and troubleshooting

- `chocolate-doom`, `freedoom`, `python3-tk`, `python3-evdev`, and `xcompmgr` (installed by the script).
- A logged-in X11 desktop; the overlay needs X11 alpha compositing.
- If keyboard play works but touch buttons do not, log out and in if the installer just added `input` group membership, then check that `/dev/uinput` belongs to group `input` with group read/write permission.
- If the game starts with a missing IWAD message, confirm `freedoom1.wad` is installed or set `DOOM_IWAD` to a WAD file you are allowed to use.
- The SPI display driver and its X11 rotation are managed by Raspberry Pi OS and the display manufacturer. The project intentionally does not replace system boot or Xorg settings.

## Development checks

Pure Python touch and launcher tests run on desktop systems:

```sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 -m py_compile scripts/touch_controls.py scripts/touch_hud.py scripts/launch_config.py scripts/game_session.py
```

The C prototype has its own build and tests:

```sh
make
make test
```

The C prototype and its GPIO matrix diagnostic are separate from the Chocolate Doom launcher. Do not run the GPIO diagnostic while the SPI display or keypad wiring may share GPIO lines.

## Repository contents

- `scripts/game_session.py`, `scripts/launch.sh`: launch Chocolate Doom and tie the HUD lifetime to the game.
- `scripts/touch_controls.py`: tested touch calibration and control state mapping.
- `scripts/touch_hud.py`: Tk/evdev translucent overlay.
- `scripts/install_dependencies.sh`, `scripts/install_desktop_shortcut.sh`: Pi setup and shortcut installation.
- `src/`: experimental C raycasting and hardware-input prototype.
- `tests/`: Python launcher/touch tests and C prototype tests.
