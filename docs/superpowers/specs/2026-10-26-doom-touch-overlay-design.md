# Raspberry Pi Doom Touch Overlay Design

**Date:** 2026-10-26
**Status:** Approved by user request
**Target:** Raspberry Pi 4B running Raspberry Pi OS on a 480×320 XPT2046 SPI touchscreen
**Engine:** System Chocolate Doom package; game data supplied by Freedoom or the player

## Goal

Make the existing Custom-Doom repository launch a real Doom source port on the Pi, provide a compact transparent touch HUD that follows touch input, create a desktop shortcut on the Pi, and publish the verified changes to the existing GitHub repository. Preserve Chocolate Doom's gameplay and existing Git history.

## Engine choice

Use the existing system Chocolate Doom package rather than the repository's raycasting test chamber or an unrelated bare-metal image. Chocolate Doom is the established SDL2 source port already installed and confirmed running on this Pi. The repository adds a launcher and touch controls around it; it does not copy or patch the upstream engine.

The application starts the first map directly so the touch HUD appears only during play. The launcher uses the system Freedoom IWAD when available, or an IWAD explicitly provided by the player. No copyrighted WAD files are stored in the repository or downloaded by the launcher.

## Touch HUD behavior

- Draw a translucent, borderless HUD above the game only while the launched Chocolate Doom process is playing. Hide the HUD when the game exits.
- Use a floating movement stick anchored to the initial touch point in the lower-left play area. The stick and thumb knob follow the touch; release/cancel clears all held actions.
- Use right-side swipe for horizontal look and touch buttons for fire, use, map, and pause. Controls show visible pressed feedback and have forgiving hit areas for the 480×320 panel.
- The panel is resistive and reports one touch at a time. Provide a toggle for continuous fire so one touch can move and fire without pretending the screen supports multitouch.
- Keep the engine's own status bar visible. Avoid a duplicate always-on status panel.
- The pause control hides the gameplay controls and offers a compact resume control. The game remains usable with a keyboard.

## Desktop and installation

- `scripts/launch.sh` locates Chocolate Doom and a configured IWAD, starts directly in gameplay in the current X11 session, and runs the touch HUD alongside the engine.
- `scripts/install_desktop_shortcut.sh` installs a desktop launcher under the invoking user's Desktop and application menu. It must use the current checkout path, mark the shortcut executable/trusted where supported, and not use sudo.
- `scripts/install_dependencies.sh` installs Chocolate Doom, Freedoom, Python Tk, Python evdev, and a lightweight X11 compositor needed for alpha blending. It adds only the minimum input permissions needed for touchscreen reads and `/dev/uinput`; it must not rewrite boot display settings or apply a generic event-device rule.
- The existing display setup remains system-managed. The project must not replace the user's Xorg configuration or assume every 3.5-inch HAT shares the same pin mapping.

## Code boundaries

- A pure Python touch-control module owns coordinate transforms, dynamic stick math, hit testing, key state, look deltas, and pause/visibility state. It has no Tk or evdev dependency so it can be unit tested on a desktop.
- A Tk/evdev HUD module owns transparent rendering and input-device polling. It discovers an ADS7846/XPT2046 touchscreen by device capabilities/name, with an environment variable override; it does not hardcode `/dev/input/event0`.
- The launcher owns IWAD selection, Chocolate Doom process lifetime, and orderly HUD shutdown. It does not scan or drive GPIO.

## Security and release requirements

- Preserve the existing `origin` and `main` history; push the finished commit to the existing repository only after review and verification.
- Use the Git author already configured for the user's authenticated account. Do not add collaborators or expose credentials.
- Never include IWAD files, local config, generated databases, build output, screenshots from the user's device, or runtime data.
- Scan the working tree and reachable history for secrets before publication. README examples use placeholders only.
- Document the source port, dependencies, touch limitations, setup, test, build, and desktop-launch steps accurately.

## Acceptance criteria

1. The desktop shortcut starts Chocolate Doom fullscreen on the current 480×320 desktop and begins a playable level.
2. The HUD is translucent, is absent before gameplay and after game exit, and does not cover the original Doom status bar unnecessarily.
3. The floating stick follows touch dynamically; swipe look, fire, use, map, pause/resume, and auto-fire toggle produce the intended inputs and release cleanly.
4. Touchscreen discovery finds the ADS7846 device without assuming event number; a documented environment override remains available.
5. A standard keyboard still controls the game, and the existing keypad remains disabled and untouched.
6. Pure tests cover calibration, stick/deadzone behavior, target hit-testing, key release, look/button separation, auto-fire, and pause visibility.
7. A new clone can install dependencies and create its desktop shortcut without receiving a WAD or changing display boot configuration.
8. The local repo is pushed to the existing public GitHub `main` branch and remote HEAD is verified equal to the final local commit.
