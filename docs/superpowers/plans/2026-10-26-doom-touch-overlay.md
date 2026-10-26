# Doom Touch Overlay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Add a transparent, dynamic touch HUD and desktop launcher around Chocolate Doom on Raspberry Pi 4B, then safely publish the verified release to the existing GitHub repository.

**Architecture:** Keep Chocolate Doom as an external system engine and add a small Python launcher/HUD. Separate pure touch state from Tk rendering and evdev input so control behavior can be tested without Pi hardware. Do not modify Doom gameplay code or touch GPIO.

**Tech Stack:** Python 3 standard library, Tkinter, python-evdev, Chocolate Doom, Freedoom, Bash, Git.

**Spec:** [docs/superpowers/specs/2026-10-26-doom-touch-overlay-design.md](../specs/2026-10-26-doom-touch-overlay-design.md)

## Global Constraints

- Target the Pi 4B X11 desktop at 480×320.
- Do not distribute IWADs or rewrite the Chocolate Doom engine.
- Respect the single-touch XPT2046 panel; provide auto-fire for one-finger play.
- Do not access keypad GPIO or change display boot/Xorg configuration.
- Push only to existing `origin/main`, after secret scan, tests, review, and remote synchronization checks.

---

### Task 1: Pure touch-control state and tests

**Files:**
- Create: `scripts/touch_controls.py`
- Create: `tests/test_touch_controls.py`

**Interfaces:**
- Produces a testable state mapper that accepts normalized touch events and returns key/button/look state plus drawing geometry.
- Keep imports to Python standard library.

- [ ] Write failing unit tests for calibration, floating stick anchor/deadzone, right-side look deltas, fixed action hit areas, auto-fire while moving, pause/resume visibility, and release/cancel clearing held keys.
- [ ] Run those tests and confirm they fail because the module does not exist yet.
- [ ] Implement the pure mapper with clamped geometry scaled from the 480×320 logical screen.
- [ ] Run tests and verify each behavior passes.

### Task 2: Transparent Tk/evdev overlay

**Files:**
- Create: `scripts/touch_hud.py`
- Modify: `scripts/touch_controls.py`
- Test: `tests/test_touch_controls.py`

**Interfaces:**
- Consumes the pure mapper, evdev ABS/BTN events, and `/dev/uinput` keyboard/mouse events.
- Renders translucent floating stick, action buttons with pressed states, and a pause/resume affordance.

- [ ] Add a test that rejects non-touch input devices and checks explicit device-path override parsing.
- [ ] Verify the new device-selection tests fail before implementing discovery.
- [ ] Implement device discovery by touchscreen capabilities/name, plus `DOOM_TOUCH_DEVICE` override.
- [ ] Build borderless topmost translucent control windows with responsive positions for the actual Tk screen size; keep them withdrawn until the game is active.
- [ ] Translate touch state to key down/up and relative mouse look; release every held key on touch cancel, device loss, shutdown, or game exit.
- [ ] Run all pure tests and static Python compilation.

### Task 3: Game launcher and desktop shortcut

**Files:**
- Modify: `scripts/launch.sh`
- Modify: `scripts/install_dependencies.sh`
- Create: `scripts/install_desktop_shortcut.sh`
- Test: `tests/test_launcher.py`

**Interfaces:**
- The launcher selects a user-provided IWAD or installed Freedoom, starts `chocolate-doom` fullscreen with `-warp 1`, starts the HUD after the game process, and cleans up the HUD on exit.
- The shortcut installer creates an executable desktop entry for the current checkout and application menu.

- [ ] Add failing tests for IWAD precedence, missing dependency errors, argument quoting, and desktop entry paths.
- [ ] Verify launcher tests fail before implementation.
- [ ] Implement non-interactive launch with clear error dialogs/logging, no WAD download, and X11 environment preservation.
- [ ] Restrict dependency setup to the required packages and a narrow uinput rule; remove boot, broad event-rule, and unused GPIO modifications.
- [ ] Make the desktop installer install the shortcut without root and mark it trusted where supported.
- [ ] Run the launcher tests and shell syntax checks.

### Task 4: Reproducible documentation and repository hygiene

**Files:**
- Modify: `README.md`
- Modify: `.gitignore`
- Review: `scripts/download_wad.sh`, `scripts/setup_touch_screen.sh`, `Makefile`, `CMakeLists.txt`

- [ ] Rewrite README claims around the real Chocolate Doom runtime, installation, IWAD choice, single-touch controls, launcher, shortcut, and tests.
- [ ] Remove false “Doom engine”/FPS claims and document the old C raycaster as an optional prototype only if it remains buildable.
- [ ] Stop recommending unverified WAD downloads and destructive generic display setup for this hardware.
- [ ] Add ignore rules for runtime settings, logs, desktop shortcuts, build outputs, caches, databases, and downloaded IWADs; retain `.example` config templates.
- [ ] Review tracked files and history for secrets, local databases, binaries, and private runtime data; do not print any secret values.

### Task 5: Verify, deploy, and publish

**Files:**
- All changed files

- [ ] Run pure Python tests, all existing C tests, shell syntax checks, and a clean-clone/reproducibility review.
- [ ] Deploy the exact committed source to the Pi, install the shortcut, launch the game and HUD, and capture a local-only screenshot for visual verification (never add it to Git).
- [ ] Verify touch input using the connected panel without touching keypad GPIO.
- [ ] Review `git diff`, `git diff --cached`, `git ls-files`, and secret scan results before committing.
- [ ] Commit under the configured user identity, merge without rewriting history, push `main`, fetch, and verify local HEAD equals `origin/main`.
