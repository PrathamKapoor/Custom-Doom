# Last Light: Raspberry Pi Touch Game Design

**Date:** 2026-10-26  
**Status:** Proposed; awaiting user review  
**Target:** Raspberry Pi 4B, Raspberry Pi OS, 3.5-inch 480×320 SPI TFT with XPT2046 resistive touch  
**Input:** Touch and keyboard; GPIO keypad remains disabled

## Goal

Replace the current non-interactive raycasting test chamber with a short, replayable original action game designed around the Pi's small touch display. The game should feel complete in a single session and remain responsive on the Pi 4B without requiring a commercial game data file.

## Game concept and play loop

**Last Light** is a compact first-person sci-fi survival mission. The player enters a failing relay station, activates three power relays, gathers ammunition and medical supplies, avoids or fights station stalkers, then reaches the extraction lift. Each relay changes the station lighting and enemy pressure. The mission ends in a clear win screen; zero health leads to a loss screen with a restart option.

The playable loop is: explore a room, spot threats and supplies, move and aim, spend ammunition or evade, activate a relay at close range, and use the new route toward extraction. Enemies move toward the player and attack at close range. The starter mission is hand-authored and finite; it is not an endless arena or a WAD loader.

## Approaches considered

1. **Integrate a full Doom-compatible engine.** This provides proven content and gameplay, but depends on external game data and brings a larger renderer/input integration effort. It also conflicts with the request for an original game.
2. **Expand the existing raycaster into a focused original mission.** This reuses the lightweight C/SDL2 rendering and input foundation, keeps dependencies small, and allows a complete authored loop tailored to this screen. It is the recommended approach.
3. **Build a top-down action game.** This simplifies aiming and rendering, but discards the existing first-person camera and touch-look work.

The design selects approach 2. Keep the renderer low-resolution internally and scale to the 480×320 display. Avoid assuming hardware acceleration; use the SDL software renderer when accelerated rendering is unavailable.

## Game systems

- A bounded tile map with collision-safe movement and defined spawn/relay/extraction locations.
- One primary weapon with fire cooldown, finite ammunition, reload, and visible feedback. Ammo pickups support exploration and combat.
- A small number of enemies with readable silhouettes, simple pursuit/attack states, health, and hit feedback. The first mission favors reliable behavior over complex pathfinding.
- Three relay objectives with proximity checks and a short activation action. The extraction point unlocks only after all relays are active.
- Player health, damage feedback, death, mission completion, restart, and pause.
- Deterministic update timing with capped delta time. Game rules are separated from drawing so movement, combat, objectives, and win/loss can be checked without display hardware.

## Screen and interaction design

The title screen is a composed start menu with the game title, a clear **START MISSION** action, and a compact controls hint. Gameplay controls and HUD are absent from the title screen. Pause and result screens use the same visual style and offer resume/retry/return actions as appropriate.

During gameplay, use a restrained industrial interface: near-black navy panels, cool mint for objective progress, amber for supplies and interaction, and red for damage/low health. Use a built-in bitmap font so labels remain crisp and do not add SDL_ttf as a dependency. Reserve most pixels for the scene.

The 480×320 HUD has a slim top objective strip (relay count and extraction state), a compact health bar and number at lower left, and weapon/ammo at lower right. Damage gives a brief edge flash. Relay activation has a short on-world indicator and a clear HUD update. Avoid permanent large boxes over the play area.

Touch controls only appear during gameplay:

- A floating virtual stick in the lower-left region controls forward/back and strafe. It follows a finger ID and releases cleanly when that finger lifts.
- Dragging in the unoccupied right-side region turns the view. Touching buttons must not also rotate the camera.
- Large lower-right **FIRE** and **USE** controls support held fire and relay activation. A smaller **RELOAD** control sits above or beside them, outside the look gesture region.
- A top-corner pause button opens the pause screen. It does not overlap the objective text.
- Use XPT2046 single-touch behavior gracefully: controls must remain usable one finger at a time. If SDL reports multiple fingers, movement and fire can be held together.

On desktop, WASD/arrows move, mouse or left/right arrows aim, mouse click/space fires, E activates, R reloads, and Escape pauses. Keyboard support also enables development without the Pi attached.

## Code structure and data flow

- `game_state` owns player values, mission progress, pickups, enemies, and state transitions. Its update function receives normalized actions and elapsed time.
- `game_map` owns collision and point-of-interest queries for the authored mission.
- `game_renderer` draws the 3D scene, enemies, supplies, relay feedback, title/pause/result screens, and gameplay HUD into the existing software frame path.
- `touch_overlay` remains responsible for finger tracking and emits game actions; layout dimensions derive from the actual logical display size.
- `doom_bridge` may retain SDL event translation during migration, but game-specific Doom naming and unused keypad paths should not be expanded. The implementation plan will decide the smallest safe interface cleanup after inspecting call sites.

The main loop polls SDL input, converts it into a frame action snapshot, updates game state, renders the world, draws the current screen's UI, presents the frame, and paces the next frame. Rendering must not mutate mission state except for presentation-only animation timers.

## Reliability and hardware constraints

- Keep GPIO keypad scanning disabled by default and outside the first mission's required controls. Do not change display or touch pin configuration as part of this game work.
- Do not run tests that request real GPIO lines on the Pi. Simulator tests and off-device builds cover logic; final touch/render validation requires the user to launch it on the hardware.
- The current Makefile's ARM flags fail with the Pi's current compiler configuration. Fix architecture/ABI detection separately within this project before claiming the Pi build works; use compiler target information and avoid incompatible hard-float flags.
- Do not download or require a WAD. Do not claim this project is Doom.
- On renderer or window creation failure, print the SDL error and exit cleanly. On missing configuration, create defaults without requiring network access. If audio is unavailable, gameplay still runs silently.

## Acceptance criteria

1. Launching without a WAD reaches the Last Light title screen and can start the mission.
2. The mission has walls with collision, visible enemies and supplies, three distinct relay activations, and an extraction goal that unlocks after the relays.
3. Firing consumes ammunition at a controlled rate, can damage an enemy, and cannot reduce ammo below zero. Reloading restores only available reserve ammo.
4. Enemies can damage the player; health reaching zero transitions to a loss screen. Reaching extraction after all relays transitions to a win screen. Both can restart the mission.
5. The title screen has no gameplay HUD/controls. The in-game overlay appears only after starting and remains readable at 480×320.
6. Touch control regions do not conflict: stick movement, right-side look, fire, use, reload, and pause behave independently, including finger release/cancel.
7. Keyboard control works in an ordinary SDL window for development and test use.
8. Logic and input tests cover collision bounds, mission state transitions, ammo/reload, enemy damage, touch button/look separation, and UI visibility by game state.
9. The project builds with available SDL2 development libraries. Pi-specific build success and frame rate are reported only after a build/run on the Pi; target is a stable 30 FPS at native output, with measured results recorded rather than assumed.

## Out of scope for the first playable release

- Full Doom/WAD compatibility, commercial assets, or a Doom branding claim.
- Keypad integration, network play, multiple missions, procedural maps, save files, settings menus, and elaborate audio.
- Complex enemy pathfinding or a large weapon inventory.

