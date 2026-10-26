# DOOM - Touch & 4x4 Matrix Keypad Port for Raspberry Pi 4B

[![Platform: Raspberry Pi 4B](https://img.shields.io/badge/Platform-Raspberry%20Pi%204B-red.svg)](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/)
[![OS: Raspberry Pi OS Legacy](https://img.shields.io/badge/OS-Raspberry%20Pi%20OS%20Legacy%20(32--bit)-blue.svg)](https://www.raspberrypi.com/software/)
[![Display: 3.5" 480x320 SPI TFT](https://img.shields.io/badge/Display-480x320%20SPI%20TFT%20(XPT2046)-orange.svg)]()
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-green.svg)]()

A custom, high-performance, touch-native port of classic Doom tailored specifically for the **Raspberry Pi 4 Model B**. Designed from the ground up for handheld play on a **3.5" 480x320 color SPI TFT touchscreen** (XPT2046 controller), while delivering modular, conflict-free support for a physical **16-key 4x4 membrane matrix keypad**.

---

## Key Highlights

- **Handheld Touch-Native Ergonomics:** Dual-thumb layout featuring a dynamic floating virtual thumbstick on the left, responsive on-screen FIRE/USE buttons on the right, analog horizontal swipe camera aiming, and weapon switch selectors.
- **Authentic Full-Color Doom:** Crisp 320x200 software rasterizer scaled directly to 480x320 native resolution with alpha-blended SDL2 HUD overlays running at rock-solid 35–60 FPS.
- **Hardware Pin Safety & Conflict Resolution:** Complete isolation between the 3.5" SPI display bus and the 4x4 keypad. **Physical Pin 22 (BCM GPIO 25)** is strictly guarded for LCD Data/Command (D/C); Column 2 safely operates on **BCM GPIO 5 (Physical Pin 29)**.
- **Modular Input Architecture:** Seamlessly switch between touch-only controls, physical 4x4 keypad controls, or simultaneous hybrid input.
- **Turnkey Automated Deployment:** Single-command setup scripts for Raspberry Pi OS Legacy (32-bit), automatic shareware `DOOM1.WAD` fetching, and standalone visual hardware diagnostics.

---

## Hardware Architecture & Wiring Guide

### Pin Conflict Warning
> [!CAUTION]
> **CRITICAL PIN PROTECTION WARNING**  
> Physical Pin 22 (**BCM GPIO 25**) is strictly reserved for the 3.5" SPI LCD Data/Command (D/C) line.  
> Connecting any keypad wire to Physical Pin 22 will corrupt the display bus and cause severe screen tearing or hardware lockups.  
> **Always connect Keypad Ribbon Wire 6 (Column 2) to Physical Pin 29 (BCM GPIO 5).**

---

### Raspberry Pi 4B 40-Pin GPIO Header Reference

```
                             3.3V Power (Pin 1)  [ 1] [ 2]  5V Power
              I2C1 SDA / BCM  2 (Pin 3)  [ 3] [ 4]  5V Power
              I2C1 SCL / BCM  3 (Pin 5)  [ 5] [ 6]  Ground
  [KEYPAD ROW 1] -------> BCM  4 (Pin 7)  [ 7] [ 8]  BCM 14 (UART TX)
                                  Ground  [ 9] [10]  BCM 15 (UART RX)
  [TOUCH TP_IRQ] -------> BCM 17 (Pin 11) [11] [12]  BCM 18
  [KEYPAD ROW 2] -------> BCM 27 (Pin 13) [13] [14]  Ground
  [KEYPAD ROW 3] -------> BCM 22 (Pin 15) [15] [16]  BCM 23 <------- [KEYPAD ROW 4]
                              3.3V Power [17] [18]  BCM 24 <------- [KEYPAD COL 1]
    [SPI MOSI] ---------> BCM 10 (Pin 19) [19] [20]  Ground
    [SPI MISO] ---------> BCM  9 (Pin 21) [21] [22]  BCM 25 <=== LCD D/C [DO NOT USE FOR KEYPAD!]
    [SPI SCLK] ---------> BCM 11 (Pin 23) [23] [24]  BCM  8 <------- [SCREEN CE0]
                                  Ground [25] [26]  BCM  7 <------- [TOUCH CE1]
                                  ID_SD  [27] [28]  ID_SC
  [KEYPAD COL 2] -------> BCM  5 (Pin 29) [29] [30]  Ground
                          BCM  6 (Pin 31) [31] [32]  BCM 12
                          BCM 13 (Pin 33) [33] [34]  Ground
  [KEYPAD COL 3] -------> BCM 16 (Pin 35) [35] [36]  BCM 16 <--- Pin 36
  [KEYPAD COL 4] -------> BCM 26 (Pin 37) [37] [38]  BCM 20
                                  Ground [39] [40]  BCM 21
```

### 4x4 Membrane Matrix Keypad Pinout Table

| Ribbon Wire | Matrix Line | Physical Pin | BCM GPIO | Electrical Configuration | Safety Status |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **Pin 1** | Row 1 (1, 2, 3, A) | Pin 7 | **GPIO 4** | Row Drive Output (Active LOW / Open-Drain) | Verified Safe |
| **Pin 2** | Row 2 (4, 5, 6, B) | Pin 13 | **GPIO 27** | Row Drive Output (Active LOW / Open-Drain) | Verified Safe |
| **Pin 3** | Row 3 (7, 8, 9, C) | Pin 15 | **GPIO 22** | Row Drive Output (Active LOW / Open-Drain) | Verified Safe |
| **Pin 4** | Row 4 (*, 0, #, D) | Pin 16 | **GPIO 23** | Row Drive Output (Active LOW / Open-Drain) | Verified Safe |
| **Pin 5** | Col 1 (1, 4, 7, *) | Pin 18 | **GPIO 24** | Col Sense Input (Internal Pull-Up) | Verified Safe |
| **Pin 6** | Col 2 (2, 5, 8, 0) | Pin 29 | **GPIO 5** | Col Sense Input (Internal Pull-Up) | **SECURED (Avoids GPIO 25)** |
| **Pin 7** | Col 3 (3, 6, 9, #) | Pin 36 | **GPIO 16** | Col Sense Input (Internal Pull-Up) | Verified Safe |
| **Pin 8** | Col 4 (A, B, C, D) | Pin 37 | **GPIO 26** | Col Sense Input (Internal Pull-Up) | Verified Safe |

---

## On-Screen Touch Controls (480x320 Native)

The touch interface is ergonomically engineered for dual-thumb handheld grip:

```
┌───────────────────────────────────────────────────────────────┐
│ [MENU/ESC]                                          [AUTOMAP] │
│ (X: 0..40)                                      (X: 440..480) │
│                                                               │
│         ▲                                         ┌─────────┐ │
│      [MOVE]                                       │  FIRE   │ │
│   ◄          ►                                    │ (Shoot) │ │
│         ▼                                         └─────────┘ │
│                                                   ┌─────────┐ │
│  (Left Thumb Zone:                                │   USE   │ │
│   Floating Virtual Stick                          │ (Door)  │ │
│   X: 0..200, Y: 100..320)                         └─────────┘ │
│                                                   [PREV] [NEXT│
│ [AMMO: 50]  [HEALTH: 100%]  [ARMS: 2]  [ARMOR: 0%] ◄─WEAPONS─►│
└───────────────────────────────────────────────────────────────┘
```

1. **Left Thumb - Movement:**
   - **Floating Virtual Thumbstick:** Touching anywhere in the bottom-left quadrant sets the anchor center; dragging moves forward/back and strafes left/right.
   - **15px Deadzone:** Eliminates drift on resistive screens.
2. **Right Thumb - Aiming & Weapons:**
   - **FIRE Button (Top Right):** Tap or hold for continuous firing with muzzle flash.
   - **USE Button (Mid Right):** Opens doors, flips switches, activates elevators.
   - **Analog Look Swipe:** Dragging horizontally in unreserved right zones turns the camera smoothly.
   - **Weapon Switcher:** Horizontal `< PREV` and `NEXT >` tap zones cycle weapons 1 through 7.
3. **System Controls:**
   - **Top-Left (X: 0–40, Y: 0–40):** Opens in-game Menu / ESC.
   - **Top-Right (X: 440–480, Y: 0–40):** Toggles Automap.
   - **Center-Top Tap / Key 'D':** Cycles HUD visibility (Full 0.55 -> Dim 0.35 -> Hidden/Keypad Mode 0.0).

---

## 4x4 Matrix Keypad Mapping

The 16 physical buttons translate directly to classic Doom actions:

```
┌───────────┬───────────┬───────────┬───────────┐
│   [ 1 ]   │   [ 2 ]   │   [ 3 ]   │   [ A ]   │
│ Strafe L  │  Move Fwd │ Strafe R  │   FIRE    │
├───────────┼───────────┼───────────┼───────────┤
│   [ 4 ]   │   [ 5 ]   │   [ 6 ]   │   [ B ]   │
│ Turn Left │ Move Back │Turn Right │ USE/OPEN  │
├───────────┼───────────┼───────────┼───────────┤
│   [ 7 ]   │   [ 8 ]   │   [ 9 ]   │   [ C ]   │
│ Prev Wep  │   SPEED   │ Next Wep  │  AUTOMAP  │
├───────────┼───────────┼───────────┼───────────┤
│   [ * ]   │   [ 0 ]   │   [ # ]   │   [ D ]   │
│ ESC / Menu│ ENTER/OK  │ GAMMA/HUD │ HUD TOGGLE│
└───────────┴───────────┴───────────┴───────────┘
```

- Keys can be pressed simultaneously with full **multi-key rollover**.
- All scanning runs in a dedicated 100 Hz POSIX background thread with a 15ms debounce filter.

---

## Turnkey Installation (Single-Command Setup)

### Step 1: Clone Repository
```bash
git clone https://github.com/example/rpi-doom-touch.git
cd rpi-doom-touch
```

### Step 2: Install System Dependencies
Installs build tools, `libsdl2-dev`, `libgpiod-dev`, and enables SPI:
```bash
chmod +x scripts/*.sh
./scripts/install_dependencies.sh
```

### Step 3: Download Shareware DOOM1.WAD
Fetches the verified official v1.9 shareware IWAD:
```bash
./scripts/download_wad.sh
```
*(To use commercial Doom, Doom II, TNT, or Plutonia, simply copy `DOOM.WAD` or `DOOM2.WAD` to `~/.doom-touch/` or `./wads/`).*

### Step 4: Calibrate Touchscreen
Configures XPT2046 480x320 landscape calibration and coordinate transformation matrix:
```bash
./scripts/setup_touch_screen.sh
```

### Step 5: Build & Launch
Compile with ARM Cortex-A72 hardware optimizations:
```bash
make
./scripts/launch.sh
```

---

## Standalone Hardware Diagnostics Tool

Before launching Doom, verify your 4x4 matrix keypad wiring using the visual terminal utility:

### C Diagnostic Tool:
```bash
make tools
./tools/test_gpio_matrix
```

### Python Diagnostic Tool:
```bash
python3 tools/test_gpio_matrix.py
```

Features:
- Enforces strict GPIO 25 lockout (aborts immediately if GPIO 25 is assigned).
- Displays a real-time illuminated 4x4 ASCII grid.
- Shows timestamped press and release logs.
- Run with `--sim` flag to test simulation mode without physical hardware.

---

## Configuration Files

Settings are stored in standard INI files under `~/.doom-touch/`:

### `~/.doom-touch/doom_touch.cfg`
```ini
[touch]
enabled = 1
opacity = 0.55
dpad_deadzone = 15
turn_sensitivity = 1.2
screen_rotation = 270
```

### `~/.doom-touch/doom_keypad.cfg`
```ini
[keypad]
enabled = 1
rows = 4,27,22,23
cols = 24,5,16,26
debounce_ms = 15
```

---

## Verification & Unit Tests

The codebase includes 5 automated unit test suites covering configuration parsing, touch math & gestures, alpha-blended rendering, keypad debouncing & concurrency, and the Doom event bridge:

```bash
make test
```

Expected output:
```
=================================================
 Running full test suite for rpi-doom-touch...   
=================================================
test_config_manager PASSED
test_touch_math PASSED
test_touch_render PASSED
test_matrix_keypad PASSED
test_event_bridge PASSED
=================================================
 ALL 5 TEST SUITES PASSED SUCCESSFULLY (100%)    
=================================================
```

---

## Architecture & Code Structure

```
rpi-doom-touch/
├── CMakeLists.txt              # Modern CMake build system
├── Makefile                    # Turnkey Make build system with ARM optimizations
├── README.md                   # Complete documentation and hardware wiring guide
├── scripts/
│   ├── install_dependencies.sh # Automated APT package and system configuration
│   ├── download_wad.sh         # Verified shareware DOOM1.WAD downloader
│   ├── setup_touch_screen.sh   # XPT2046 480x320 calibration & udev rules
│   └── launch.sh               # KMSDRM / X11 environment launcher
├── src/
│   ├── main.c                  # Executable entry point with 60 FPS interactive loop
│   ├── engine/
│   │   ├── doom_bridge.h       # Thread-safe event queue & SDL display scaler
│   │   └── doom_bridge.c
│   └── hardware/
│       ├── config_manager.h    # INI configuration serialization & pin safety
│       ├── config_manager.c
│       ├── matrix_keypad.h     # 100 Hz POSIX thread 4x4 keypad scanner
│       ├── matrix_keypad.c
│       ├── touch_overlay.h     # Touch gesture processing & SDL2 HUD renderer
│       └── touch_overlay.c
├── tests/                      # Automated unit test suite
│   ├── test_config_manager.c
│   ├── test_touch_math.c
│   ├── test_touch_render.c
│   ├── test_matrix_keypad.c
│   └── test_event_bridge.c
└── tools/                      # Standalone hardware testing utilities
    ├── test_gpio_matrix.c
    └── test_gpio_matrix.py
```

---

## License

This project is licensed under the GNU General Public License v2.0 (GPL-2.0).
Doom is a registered trademark of id Software / ZeniMax Media Inc.
