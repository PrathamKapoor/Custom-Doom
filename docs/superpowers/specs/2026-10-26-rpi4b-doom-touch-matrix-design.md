# Technical Design Specification: Custom Doom Port for Raspberry Pi 4B (Touch-Native & 4x4 Matrix Controller)

**Date:** 2026-10-26  
**Status:** Approved by User  
**Target Platform:** Raspberry Pi 4 Model B (Quad-core Cortex-A72 @ 1.5GHz)  
**Operating System:** Raspberry Pi OS Legacy (32-bit armhf, Buster/Bullseye)  
**Primary Display:** 3.5" GPIO SPI TFT LCD (480x320 pixels, full color) with XPT2046 Resistive Touch Controller  
**Secondary Controller:** 16-key 4x4 Membrane Matrix Keypad (Pins 1–8 on GPIO)  

---

## 1. Executive Summary & Goals

The objective is to deliver a custom, high-performance port of the classic Doom engine tailored specifically for the Raspberry Pi 4B, designed from the ground up for a 3.5" 480x320 color touchscreen with XPT2046 resistive touch, while providing modular, conflict-free support for a physical 4x4 matrix keypad.

### Core Goals:
1. **Touch-Native Gameplay:** Highly responsive, low-latency on-screen touch controls optimized for handheld dual-thumb play on a 3.5" (480x320) display.
2. **Authentic Full-Color Doom:** Full 256-color authentic Doom rendering scaled cleanly to 480x320 with rock-solid 35–60 FPS.
3. **Hardware Safety & Conflict Resolution:** Prevent pin collisions between the 3.5" SPI display (which strictly reserves GPIO 25 for LCD Data/Command) and the 8-pin 4x4 matrix keypad.
4. **Modular Input Architecture:** Seamless switching between touch-only controls, physical 4x4 keypad controls, or simultaneous hybrid input.
5. **Turnkey Deployment:** Automated build and dependency installation script with shareware `DOOM1.WAD` pre-configured.

---

## 2. Hardware Architecture & Pin Mapping

### 2.1 3.5" SPI TFT & Touchscreen Pin Allocation
The 3.5" display HAT plugs directly onto the Raspberry Pi 40-pin header:
* **SPI Bus (Screen):**
  * GPIO 10 (Physical Pin 19) - MOSI
  * GPIO 9 (Physical Pin 21) - MISO
  * GPIO 11 (Physical Pin 23) - SCLK
  * GPIO 8 (Physical Pin 24) - Screen Chip Select (CE0)
* **SPI Bus (Touch Controller - XPT2046):**
  * GPIO 7 (Physical Pin 26) - Touch Chip Select (CE1)
  * GPIO 17 (Physical Pin 11) - Touch Interrupt (TP_IRQ, if enabled)
* **Control:**
  * GPIO 25 (Physical Pin 22) - LCD Data/Command (D/C) **[CRITICAL: Do not use for keypad]**

### 2.2 4x4 Keypad Conflict-Free Pin Allocation
The 4x4 matrix keypad uses 8 wires (4 rows, 4 columns). To prevent display corruption on GPIO 25:

| Ribbon Wire | Matrix Line | Physical Pin | BCM GPIO | Electrical Role |
| :--- | :--- | :--- | :--- | :--- |
| **Pin 1** | Row 1 (Keys: 1, 2, 3, A) | Pin 7 | **GPIO 4** | Row Drive Output (Active LOW) |
| **Pin 2** | Row 2 (Keys: 4, 5, 6, B) | Pin 13 | **GPIO 27** | Row Drive Output (Active LOW) |
| **Pin 3** | Row 3 (Keys: 7, 8, 9, C) | Pin 15 | **GPIO 22** | Row Drive Output (Active LOW) |
| **Pin 4** | Row 4 (Keys: *, 0, #, D) | Pin 16 | **GPIO 23** | Row Drive Output (Active LOW) |
| **Pin 5** | Col 1 (Keys: 1, 4, 7, *) | Pin 18 | **GPIO 24** | Col Sense Input (Internal Pull-Up) |
| **Pin 6** | Col 2 (Keys: 2, 5, 8, 0) | Pin 29 | **GPIO 5** | Col Sense Input (Internal Pull-Up) |
| **Pin 7** | Col 3 (Keys: 3, 6, 9, #) | Pin 36 | **GPIO 16** | Col Sense Input (Internal Pull-Up) |
| **Pin 8** | Col 4 (Keys: A, B, C, D) | Pin 37 | **GPIO 26** | Col Sense Input (Internal Pull-Up) |

*(All pins are loaded dynamically from `~/.doom-touch/doom_keypad.cfg`).*

---

## 3. Touchscreen Subsystem Design (480x320 Native)

### 3.1 Ergonomic HUD Layout
The screen layout is tailored for handheld dual-thumb ergonomics:

```
┌───────────────────────────────────────────────────────────────┐
│ [MENU/ESC]                                          [AUTOMAP] │
│                                                               │
│         ▲                                         ┌─────────┐ │
│      [MOVE]                                       │  FIRE   │ │
│   ◄          ►                                    │ (Shoot) │ │
│         ▼                                         └─────────┘ │
│                                                   ┌─────────┐ │
│  (Left Thumb:                                     │   USE   │ │
│   Virtual D-Pad                                   │ (Door)  │ │
│   or Analog Swipe)                                └─────────┘ │
│                                                   [PREV] [NEXT│
│ [HEALTH: 100%] [AMMO: 50] [ARMOR: 0%] [WEAPON: 2] ◄──WEAPONS─►│
└───────────────────────────────────────────────────────────────┘
```

### 3.2 Touch Zones & Gestures
1. **Left Thumb - Movement:**
   * Dynamic virtual thumbstick: on finger press in the bottom-left zone (X: 0–200, Y: 100–320), the center anchors to touch position.
   * Drag vector determines forward/backward speed and strafe left/right speed.
   * 15-pixel deadzone prevents drift.
2. **Right Thumb - Aiming & Action:**
   * **Fire Button (Top Right, 60x60 px):** Tap or hold for continuous firing.
   * **Use Button (Mid Right, 60x45 px):** Opens doors, flips switches.
   * **Look / Turn Swipe (Unoccupied Right Half):** Dragging right thumb horizontally pans the camera (analog turning sensitivity).
   * **Weapon Selector:** Horizontal tap zones for `< PREV` and `NEXT >` weapon switching.
3. **System Controls:**
   * Top-Left (`X: 0–40, Y: 0–40`): In-game Menu / ESC.
   * Top-Right (`X: 440–480, Y: 0–40`): Automap toggle.
   * Center-Top Long Press: Cycles Touch HUD visibility (Full Opacity `0.75` -> Dim `0.35` -> Invisible/Keypad Mode `0.0`).

### 3.3 Resistive Touch (XPT2046) Calibration & Filtering
* **Coordinate Normalization:** Reads Linux evdev events (`/dev/input/eventX`), normalizes raw 12-bit ADC coordinates (`0–4095`) to `0–480` and `0–320`.
* **Jitter Filter:** Single-pole low-pass IIR filter (`pos = 0.7 * raw + 0.3 * prev`) eliminates resistive noise and mechanical micro-oscillations.
* **Rotation Matrix:** Configurable in config (`rotation = 0, 90, 180, 270`) to handle any physical orientation of the display.

---

## 4. 4x4 Matrix Keypad Subsystem ("Later This" Modular Layer)

### 4.1 Keypad Mapping Matrix
The 16 physical buttons translate to game actions:

```
┌───────────┬───────────┬───────────┬───────────┐
│   [ 1 ]   │   [ 2 ]   │   [ 3 ]   │   [ A ]   │
│ Strafe L  │  Move Fwd │ Strafe R  │   FIRE    │
├───────────┼───────────┼───────────┼───────────┤
│   [ 4 ]   │   [ 5 ]   │   [ 6 ]   │   [ B ]   │
│ Turn Left │  Move Back│ Turn Right│ USE/OPEN  │
├───────────┼───────────┼───────────┼───────────┤
│   [ 7 ]   │   [ 8 ]   │   [ 9 ]   │   [ C ]   │
│ Prev Wep  │  SPEED    │ Next Wep  │ AUTOMAP   │
├───────────┼───────────┼───────────┼───────────┤
│   [ * ]   │   [ 0 ]   │   [ # ]   │   [ D ]   │
│ ESC / Menu│ ENTER/OK  │ GAMMA/HUD │ HUD TOGGLE│
└───────────┴───────────┴───────────┴───────────┘
```

### 4.2 Matrix Scanning Thread
* Implemented as an independent POSIX background thread using `libgpiod` or memory-mapped GPIO registers.
* Scanning rate: 100 Hz (10ms scan period).
* 15ms debounce filter with multi-key rollover support.
* Key events are dispatched directly into Doom's `D_PostEvent()` queue as `ev_keydown` and `ev_keyup`, ensuring instant response and full vanilla game compatibility.

---

## 5. Software Architecture & Game Engine

### 5.1 Engine Core
* **Base:** Custom lightweight C99 Doom engine based on Chocolate Doom / Crispy Doom.
* **Renderer:** Native 320x200 or 320x240 software rasterizer rendered to an SDL2 streaming texture, upscaled via nearest-neighbor/bilinear filtering to 480x320.
* **Overlay Layer:** Touch controls rendered directly by SDL2 Render API with alpha blending over the game frame buffer before `SDL_RenderPresent()`.
* **Audio:** SDL_mixer at 44.1 kHz, 16-bit stereo. Emulated OPL3 / MIDI synth for classic authentic sound without high CPU overhead.

### 5.2 Configuration Files
1. **`~/.doom-touch/doom_touch.cfg`**:
   ```ini
   [touch]
   enabled = 1
   opacity = 0.55
   dpad_deadzone = 15
   turn_sensitivity = 1.2
   screen_rotation = 270
   ```
2. **`~/.doom-touch/doom_keypad.cfg`**:
   ```ini
   [keypad]
   enabled = 0
   rows = 4,27,22,23
   cols = 24,5,16,26
   debounce_ms = 15
   ```

---

## 6. Verification & Testing Strategy

1. **Touch Responsiveness Test:** Verify multi-touch / single-touch drag tracking and coordinate mapping accuracy across all 4 corners of the 480x320 screen.
2. **Frame Rate Verification:** Measure rendering FPS on RPi 4B to ensure minimum 35 FPS, target 60 FPS.
3. **GPIO Safety Verification:** Confirm GPIO 25 is untouched and SPI display continues updating without bus glitches.
4. **Keypad Matrix Test:** Run standalone GPIO matrix test utility to verify each of the 16 buttons register cleanly.
5. **Full Gameplay Test:** Play Episode 1 Map 1 (E1M1) using touch controls exclusively, then enable keypad and verify hybrid play.
