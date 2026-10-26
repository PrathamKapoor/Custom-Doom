# Custom Doom Port for Raspberry Pi 4B (Touch-Native & 4x4 Matrix) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a custom, high-performance, touch-native Doom port for the Raspberry Pi 4B featuring full-color 480x320 rendering, low-latency on-screen touch controls optimized for 3.5" XPT2046 displays, and a modular conflict-free 4x4 matrix keypad driver.

**Architecture:** A portable C99 SDL2 Doom engine with an integrated Touch Input & HUD rendering layer, a background POSIX thread scanning the 4x4 GPIO matrix, an INI configuration manager, and automated deployment scripts tailored for Raspberry Pi OS Legacy 32-bit.

**Tech Stack:** C99, SDL2, SDL2_mixer, libgpiod / Linux GPIO character device, GCC, CMake / Make.

**Spec:** [docs/superpowers/specs/2026-10-26-rpi4b-doom-touch-matrix-design.md](file:///C:/Users/LENOVO/docs/superpowers/specs/2026-10-26-rpi4b-doom-touch-matrix-design.md)

## Global Constraints
- Target platform: Raspberry Pi 4 Model B running Raspberry Pi OS Legacy (32-bit armhf).
- Display: 3.5" SPI TFT LCD (480x320 native resolution) with XPT2046 resistive touch.
- CRITICAL PIN SAFETY: Physical Pin 22 (GPIO 25) is strictly reserved for LCD Data/Command; never drive or reassign GPIO 25 for keypad scanning.
- 4x4 Keypad pins: Row 1-4 on GPIO 4, 27, 22, 23; Col 1-4 on GPIO 24, 5, 16, 26.
- All configuration stored in `~/.doom-touch/doom_touch.cfg` and `~/.doom-touch/doom_keypad.cfg`.
- Free shareware `DOOM1.WAD` supported out of the box; full commercial IWADs seamlessly accepted.

---

### Task 1: Hardware Configuration Manager

**Files:**
- Create: `rpi-doom-touch/src/hardware/config_manager.h`
- Create: `rpi-doom-touch/src/hardware/config_manager.c`
- Test: `rpi-doom-touch/tests/test_config_manager.c`

**Interfaces:**
- Consumes: Standard C library (`stdio.h`, `stdlib.h`, `string.h`, `sys/stat.h`).
- Produces:
  ```c
  typedef struct {
      int touch_enabled;
      float touch_opacity;
      int dpad_deadzone;
      float turn_sensitivity;
      int screen_rotation; // 0, 90, 180, 270
  } touch_config_t;

  typedef struct {
      int keypad_enabled;
      int row_pins[4];
      int col_pins[4];
      int debounce_ms;
      int key_bindings[16]; // Mapped Doom action codes
  } keypad_config_t;

  void config_set_defaults(touch_config_t *tcfg, keypad_config_t *kcfg);
  int config_load(const char *dir_path, touch_config_t *tcfg, keypad_config_t *kcfg);
  int config_save(const char *dir_path, const touch_config_t *tcfg, const keypad_config_t *kcfg);
  ```

- [ ] **Step 1: Write the failing test for configuration serialization and deserialization**

```c
// rpi-doom-touch/tests/test_config_manager.c
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/hardware/config_manager.h"

int main(void) {
    touch_config_t tcfg;
    keypad_config_t kcfg;
    config_set_defaults(&tcfg, &kcfg);

    assert(tcfg.touch_enabled == 1);
    assert(tcfg.screen_rotation == 270);
    assert(kcfg.row_pins[0] == 4);
    assert(kcfg.col_pins[1] == 5); // Must be GPIO 5, NOT GPIO 25!

    const char *test_dir = "./tmp_config_test";
    assert(config_save(test_dir, &tcfg, &kcfg) == 0);

    touch_config_t loaded_t;
    keypad_config_t loaded_k;
    assert(config_load(test_dir, &loaded_t, &loaded_k) == 0);

    assert(loaded_t.touch_enabled == 1);
    assert(loaded_t.screen_rotation == 270);
    assert(loaded_k.row_pins[0] == 4);
    assert(loaded_k.col_pins[1] == 5);
    printf("test_config_manager PASSED\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Compile & run: `gcc -I src rpi-doom-touch/tests/test_config_manager.c rpi-doom-touch/src/hardware/config_manager.c -o test_config_manager && ./test_config_manager`  
Expected: FAIL (files missing or undefined symbols).

- [ ] **Step 3: Write minimal implementation of `config_manager.h` and `config_manager.c`**

Implement INI parser and writer with safe defaults (GPIO 4, 27, 22, 23 for rows; GPIO 24, 5, 16, 26 for columns; screen_rotation = 270).

- [ ] **Step 4: Run test to verify it passes**

Compile & run: Verify output `test_config_manager PASSED`.

- [ ] **Step 5: Commit**

```bash
git add rpi-doom-touch/src/hardware/config_manager.* rpi-doom-touch/tests/test_config_manager.c
git commit -m "feat(config): add touch and keypad INI configuration manager"
```

---

### Task 2: Touch Coordinate Normalization, Filtering & Floating Thumbstick Subsystem

**Files:**
- Create: `rpi-doom-touch/src/hardware/touch_overlay.h`
- Create: `rpi-doom-touch/src/hardware/touch_overlay.c`
- Test: `rpi-doom-touch/tests/test_touch_math.c`

**Interfaces:**
- Consumes: `config_manager.h`, SDL2 headers (`SDL.h`).
- Produces:
  ```c
  typedef struct {
      float move_forward;  // -1.0 to 1.0
      float strafe_right;  // -1.0 to 1.0
      float turn_speed;    // -1.0 to 1.0
      int fire_pressed;
      int use_pressed;
      int next_weapon;
      int prev_weapon;
      int menu_pressed;
      int automap_pressed;
      int toggle_hud;
  } touch_game_actions_t;

  void touch_overlay_init(const touch_config_t *config, int screen_w, int screen_h);
  void touch_overlay_handle_down(int finger_id, float norm_x, float norm_y);
  void touch_overlay_handle_motion(int finger_id, float norm_x, float norm_y);
  void touch_overlay_handle_up(int finger_id);
  touch_game_actions_t touch_overlay_get_actions(void);
  ```

- [ ] **Step 1: Write the failing test for touch coordinate transforms and deadzone**

```c
// rpi-doom-touch/tests/test_touch_math.c
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "../src/hardware/touch_overlay.h"
#include "../src/hardware/config_manager.h"

int main(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);

    // Test Left Thumb Dynamic Anchor
    // Press down at X: 100, Y: 200
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    // Drag forward 50 pixels to Y: 150
    touch_overlay_handle_motion(1, 100.0f / 480.0f, 150.0f / 320.0f);
    touch_game_actions_t actions = touch_overlay_get_actions();
    assert(actions.move_forward > 0.5f);
    assert(fabsf(actions.strafe_right) < 0.1f);

    // Release thumb
    touch_overlay_handle_up(1);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward == 0.0f);

    // Test Fire Button Zone (Top-right tap at X: 450, Y: 120)
    touch_overlay_handle_down(2, 450.0f / 480.0f, 120.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.fire_pressed == 1);
    touch_overlay_handle_up(2);

    printf("test_touch_math PASSED\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Compile & run: Verify compiler error (missing `touch_overlay.c/.h`).

- [ ] **Step 3: Write minimal implementation of `touch_overlay.c` and `touch_overlay.h`**

Implement touch zone recognition, dynamic floating thumbstick calculation, deadzone clamping, and touch-look aim accumulation.

- [ ] **Step 4: Run test to verify it passes**

Compile & run: Verify output `test_touch_math PASSED`.

- [ ] **Step 5: Commit**

```bash
git add rpi-doom-touch/src/hardware/touch_overlay.* rpi-doom-touch/tests/test_touch_math.c
git commit -m "feat(touch): add touch input math, gestures, and floating thumbstick logic"
```

---

### Task 3: Touch HUD SDL2 Rendering & Visual Feedback

**Files:**
- Modify: `rpi-doom-touch/src/hardware/touch_overlay.h`
- Modify: `rpi-doom-touch/src/hardware/touch_overlay.c`
- Test: `rpi-doom-touch/tests/test_touch_render.c`

**Interfaces:**
- Consumes: SDL2 Renderer API (`SDL_RenderDrawLine`, `SDL_SetRenderDrawBlendMode`, `SDL_RenderFillRect`).
- Produces:
  ```c
  void touch_overlay_render(SDL_Renderer *renderer);
  void touch_overlay_set_opacity(float opacity);
  ```

- [ ] **Step 1: Write test verifying touch HUD opacity state cycle and render calls**

```c
// rpi-doom-touch/tests/test_touch_render.c
#include <stdio.h>
#include <assert.h>
#include "../src/hardware/touch_overlay.h"
#include "../src/hardware/config_manager.h"

int main(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);

    touch_overlay_set_opacity(0.75f);
    // Cycle opacity modes
    touch_overlay_cycle_hud_visibility(); // 0.75 -> 0.35
    touch_overlay_cycle_hud_visibility(); // 0.35 -> 0.0 (Hidden)
    touch_overlay_cycle_hud_visibility(); // 0.0 -> 0.75
    printf("test_touch_render PASSED\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Compile & run: Verify failure.

- [ ] **Step 3: Implement `touch_overlay_render` and `touch_overlay_cycle_hud_visibility`**

Draw crisp alpha-blended geometry for the D-Pad/stick circle, Fire badge, Use badge, Weapon switch arrows, Menu, and Automap toggles.

- [ ] **Step 4: Run test to verify it passes**

Compile & run: Verify output `test_touch_render PASSED`.

- [ ] **Step 5: Commit**

```bash
git add rpi-doom-touch/src/hardware/touch_overlay.* rpi-doom-touch/tests/test_touch_render.c
git commit -m "feat(render): add SDL2 alpha-blended touch HUD renderer"
```

---

### Task 4: Modular 4x4 GPIO Matrix Keypad Driver

**Files:**
- Create: `rpi-doom-touch/src/hardware/matrix_keypad.h`
- Create: `rpi-doom-touch/src/hardware/matrix_keypad.c`
- Test: `rpi-doom-touch/tests/test_matrix_keypad.c`

**Interfaces:**
- Consumes: `config_manager.h`, `pthread.h`, Linux GPIO character device (`/dev/gpiochip*` / `libgpiod`).
- Produces:
  ```c
  typedef void (*keypad_event_callback_t)(int key_index, int is_down);

  int matrix_keypad_init(const keypad_config_t *config, keypad_event_callback_t callback);
  void matrix_keypad_poll_once(void); // For testing & manual scanning
  void matrix_keypad_start_thread(void);
  void matrix_keypad_stop(void);
  ```

- [ ] **Step 1: Write test for matrix key decoding and debounce state machine**

```c
// rpi-doom-touch/tests/test_matrix_keypad.c
#include <stdio.h>
#include <assert.h>
#include "../src/hardware/matrix_keypad.h"
#include "../src/hardware/config_manager.h"

static int last_key = -1;
static int last_state = -1;
static void test_callback(int key, int down) {
    last_key = key;
    last_state = down;
}

int main(void) {
    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;

    assert(matrix_keypad_init(&cfg, test_callback) == 0);
    // Simulate Row 1, Col 1 press (Key '1', Index 0)
    matrix_keypad_simulate_raw_state(0, 0, 1);
    matrix_keypad_poll_once();
    assert(last_key == 0);
    assert(last_state == 1);

    // Release
    matrix_keypad_simulate_raw_state(0, 0, 0);
    matrix_keypad_poll_once();
    assert(last_key == 0);
    assert(last_state == 0);

    printf("test_matrix_keypad PASSED\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Compile & run: Verify compiler failure.

- [ ] **Step 3: Implement `matrix_keypad.c` and `matrix_keypad.h`**

Implement GPIO character device row driving and column reading, internal pull-up configuration, debouncing filter, simulation hooks for testing, and POSIX background thread.

- [ ] **Step 4: Run test to verify it passes**

Compile & run: Verify output `test_matrix_keypad PASSED`.

- [ ] **Step 5: Commit**

```bash
git add rpi-doom-touch/src/hardware/matrix_keypad.* rpi-doom-touch/tests/test_matrix_keypad.c
git commit -m "feat(keypad): add 4x4 matrix GPIO scanning thread with debouncing"
```

---

### Task 5: Engine Event Injection & 480x320 Display Scaler

**Files:**
- Create: `rpi-doom-touch/src/engine/doom_bridge.h`
- Create: `rpi-doom-touch/src/engine/doom_bridge.c`
- Test: `rpi-doom-touch/tests/test_event_bridge.c`

**Interfaces:**
- Consumes: `touch_overlay.h`, `matrix_keypad.h`, SDL2, Doom core event loop.
- Produces:
  ```c
  void doom_bridge_init(SDL_Window *window, SDL_Renderer *renderer);
  void doom_bridge_process_input(void);
  void doom_bridge_render_present(SDL_Texture *game_framebuffer);
  ```

- [ ] **Step 1: Write test verifying event bridge maps touch and keypad actions into Doom game events**

```c
// rpi-doom-touch/tests/test_event_bridge.c
#include <stdio.h>
#include <assert.h>
#include "../src/engine/doom_bridge.h"

int main(void) {
    doom_bridge_init_headless();
    // Simulate touch fire action
    doom_bridge_inject_fire(1);
    assert(doom_bridge_is_fire_active() == 1);
    doom_bridge_inject_fire(0);
    assert(doom_bridge_is_fire_active() == 0);
    printf("test_event_bridge PASSED\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Compile & run: Verify failure.

- [ ] **Step 3: Implement `doom_bridge.c` and `doom_bridge.h`**

Bridge touch vectors and matrix keypad presses into Doom's `event_t` structure (`ev_keydown`, `ev_keyup`, `ev_mouse`), and present the 480x320 SDL2 streaming texture with overlaid HUD.

- [ ] **Step 4: Run test to verify it passes**

Compile & run: Verify output `test_event_bridge PASSED`.

- [ ] **Step 5: Commit**

```bash
git add rpi-doom-touch/src/engine/doom_bridge.* rpi-doom-touch/tests/test_event_bridge.c
git commit -m "feat(engine): integrate Doom event bridge and 480x320 display scaler"
```

---

### Task 6: Turnkey Build, Setup Scripts & Raspberry Pi Deployment

**Files:**
- Create: `rpi-doom-touch/CMakeLists.txt`
- Create: `rpi-doom-touch/Makefile`
- Create: `rpi-doom-touch/scripts/install_dependencies.sh`
- Create: `rpi-doom-touch/scripts/download_wad.sh`
- Create: `rpi-doom-touch/scripts/setup_touch_screen.sh`
- Create: `rpi-doom-touch/scripts/launch.sh`
- Create: `rpi-doom-touch/README.md`

**Deliverables:**
- Automated single-command installation for Raspberry Pi OS Legacy (32-bit).
- Build system with ARMv8/Cortex-A72 optimizations (`-O3 -march=armv8-a+crc -mtune=cortex-a72`).
- Safe GPIO matrix testing tool (`tools/test_gpio_matrix.py` and `tools/test_gpio_matrix.c`) to verify 4x4 keypad wiring without starting the game.

- [ ] **Step 1: Create `CMakeLists.txt` and `Makefile` supporting both native Pi compilation and cross-platform builds**
- [ ] **Step 2: Write `scripts/install_dependencies.sh` (installs `libsdl2-dev`, `libsdl2-mixer-dev`, `libgpiod-dev`, etc.)**
- [ ] **Step 3: Write `scripts/download_wad.sh` (fetches verified shareware `DOOM1.WAD`)**
- [ ] **Step 4: Write `scripts/setup_touch_screen.sh` (configures XPT2046 480x320 calibration matrix & rotation)**
- [ ] **Step 5: Write standalone GPIO matrix test utility `tools/test_keypad`**
- [ ] **Step 6: Write complete step-by-step `README.md` with wiring diagrams, pin protection warnings, and launch options**
- [ ] **Step 7: Verify full build and run test suite**

Run: `make test`  
Expected: All unit tests pass.

- [ ] **Step 8: Commit**

```bash
git add rpi-doom-touch/
git commit -m "feat(deploy): complete turnkey build system, deployment scripts, and documentation"
```
