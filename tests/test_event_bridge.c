#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../src/engine/doom_bridge.h"

/* Mock SDL2 Renderer and Texture state */
typedef struct {
    int clear_calls;
    int copy_calls;
    SDL_Rect last_src_rect;
    SDL_Rect last_dst_rect;
    int has_src_rect;
    int present_calls;

    // Overlay draw tracking
    int blend_mode_calls;
    SDL_BlendMode last_blend_mode;
    int set_color_calls;
    int fill_rect_calls;
    int draw_rect_calls;
    int draw_line_calls;
} mock_sdl_state_t;

static mock_sdl_state_t g_mock_sdl;
static SDL_Renderer *g_dummy_renderer = (SDL_Renderer *)0x12345678;
static SDL_Texture  *g_dummy_texture  = (SDL_Texture *)0x87654321;

/* Mock SDL2 functions for display scaler testing */
int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode) {
    (void)renderer;
    g_mock_sdl.blend_mode_calls++;
    g_mock_sdl.last_blend_mode = blendMode;
    return 0;
}

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    (void)renderer;
    (void)r; (void)g; (void)b; (void)a;
    g_mock_sdl.set_color_calls++;
    return 0;
}

int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    (void)renderer;
    (void)rect;
    g_mock_sdl.fill_rect_calls++;
    return 0;
}

int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    (void)renderer;
    (void)rect;
    g_mock_sdl.draw_rect_calls++;
    return 0;
}

int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2) {
    (void)renderer;
    (void)x1; (void)y1; (void)x2; (void)y2;
    g_mock_sdl.draw_line_calls++;
    return 0;
}

int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y) {
    (void)renderer;
    (void)x; (void)y;
    return 0;
}

int SDL_RenderClear(SDL_Renderer *renderer) {
    (void)renderer;
    g_mock_sdl.clear_calls++;
    return 0;
}

int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect) {
    (void)renderer;
    (void)texture;
    g_mock_sdl.copy_calls++;
    if (srcrect) {
        g_mock_sdl.has_src_rect = 1;
        g_mock_sdl.last_src_rect = *srcrect;
    } else {
        g_mock_sdl.has_src_rect = 0;
    }
    if (dstrect) {
        g_mock_sdl.last_dst_rect = *dstrect;
    }
    return 0;
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    (void)renderer;
    g_mock_sdl.present_calls++;
}

/* Mock SDL_PollEvent queue */
#define MOCK_SDL_QUEUE_MAX 32
static SDL_Event g_mock_sdl_queue[MOCK_SDL_QUEUE_MAX];
static int g_mock_sdl_queue_head = 0;
static int g_mock_sdl_queue_tail = 0;
static int g_mock_sdl_queue_count = 0;

static void mock_sdl_push_event(const SDL_Event *ev) {
    if (g_mock_sdl_queue_count < MOCK_SDL_QUEUE_MAX) {
        g_mock_sdl_queue[g_mock_sdl_queue_head] = *ev;
        g_mock_sdl_queue_head = (g_mock_sdl_queue_head + 1) % MOCK_SDL_QUEUE_MAX;
        g_mock_sdl_queue_count++;
    }
}

int SDL_PollEvent(SDL_Event *event) {
    if (g_mock_sdl_queue_count > 0 && event != NULL) {
        *event = g_mock_sdl_queue[g_mock_sdl_queue_tail];
        g_mock_sdl_queue_tail = (g_mock_sdl_queue_tail + 1) % MOCK_SDL_QUEUE_MAX;
        g_mock_sdl_queue_count--;
        return 1;
    }
    return 0;
}

static void mock_reset_all(void) {
    memset(&g_mock_sdl, 0, sizeof(g_mock_sdl));
    g_mock_sdl_queue_head = 0;
    g_mock_sdl_queue_tail = 0;
    g_mock_sdl_queue_count = 0;
}

/* ========================================================================= */
/* Test Cases                                                                */
/* ========================================================================= */

static void test_step1_brief_scenario(void) {
    printf("Running test_step1_brief_scenario...\n");
    doom_bridge_init_headless();

    // Simulate touch fire action
    doom_bridge_inject_fire(1);
    assert(doom_bridge_is_fire_active() == 1);
    doom_bridge_inject_fire(0);
    assert(doom_bridge_is_fire_active() == 0);

    // Verify keydown / keyup events were generated
    assert(doom_bridge_get_queued_events_count() == 2);
    event_t ev1, ev2;
    assert(doom_bridge_get_event(&ev1) == 1);
    assert(ev1.type == ev_keydown);
    assert(ev1.data1 == DOOM_KEY_FIRE);

    assert(doom_bridge_get_event(&ev2) == 1);
    assert(ev2.type == ev_keyup);
    assert(ev2.data1 == DOOM_KEY_FIRE);

    assert(doom_bridge_get_queued_events_count() == 0);
    printf("  [PASS] test_step1_brief_scenario\n");
}

static void test_keypad_event_translation_all_16(void) {
    printf("Running test_keypad_event_translation_all_16...\n");
    doom_bridge_init_headless();
    doom_bridge_clear_events();

    /* Expected keycodes for actions 0..14 (key 15 is HUD toggle) */
    const int expected_keys[16] = {
        DOOM_KEY_STRAFE_L,   // Key 0  [1] Strafe L
        DOOM_KEY_UP,         // Key 1  [2] Move Fwd
        DOOM_KEY_STRAFE_R,   // Key 2  [3] Strafe R
        DOOM_KEY_FIRE,       // Key 3  [A] FIRE
        DOOM_KEY_LEFT,       // Key 4  [4] Turn Left
        DOOM_KEY_DOWN,       // Key 5  [5] Move Back
        DOOM_KEY_RIGHT,      // Key 6  [6] Turn Right
        DOOM_KEY_USE,        // Key 7  [B] USE/OPEN
        DOOM_KEY_PREV_WEAPON,// Key 8  [7] Prev Wep
        DOOM_KEY_SPEED,      // Key 9  [8] SPEED
        DOOM_KEY_NEXT_WEAPON,// Key 10 [9] Next Wep
        DOOM_KEY_TAB,        // Key 11 [C] AUTOMAP
        DOOM_KEY_ESCAPE,     // Key 12 [*] ESC / Menu
        DOOM_KEY_ENTER,      // Key 13 [0] ENTER / OK
        DOOM_KEY_GAMMA,      // Key 14 [#] GAMMA / HUD
        0                    // Key 15 [D] HUD TOGGLE (special internal action)
    };

    for (int k = 0; k < 15; k++) {
        // Press
        doom_bridge_inject_keypad_event(k, 1);
        if (k == 3) {
            // Key 3 is FIRE
            assert(doom_bridge_is_fire_active() == 1);
        }
        assert(doom_bridge_get_queued_events_count() == 1);
        event_t ev_down;
        assert(doom_bridge_get_event(&ev_down) == 1);
        assert(ev_down.type == ev_keydown);
        assert(ev_down.data1 == expected_keys[k]);

        // Release
        doom_bridge_inject_keypad_event(k, 0);
        if (k == 3) {
            assert(doom_bridge_is_fire_active() == 0);
        }
        assert(doom_bridge_get_queued_events_count() == 1);
        event_t ev_up;
        assert(doom_bridge_get_event(&ev_up) == 1);
        assert(ev_up.type == ev_keyup);
        assert(ev_up.data1 == expected_keys[k]);
    }

    // Key 15 is HUD Toggle: should cycle opacity without pushing a raw key event
    float op_initial = touch_overlay_get_opacity();
    doom_bridge_inject_keypad_event(15, 1);
    float op_after = touch_overlay_get_opacity();
    assert(op_after != op_initial);
    doom_bridge_inject_keypad_event(15, 0);
    assert(doom_bridge_get_queued_events_count() == 0);

    printf("  [PASS] test_keypad_event_translation_all_16\n");
}

static void test_keypad_custom_rebind(void) {
    printf("Running test_keypad_custom_rebind...\n");
    doom_bridge_init_headless();
    doom_bridge_clear_events();

    keypad_config_t kcfg;
    config_set_defaults(NULL, &kcfg);
    // Rebind Key 0 to FIRE, Key 1 to USE
    kcfg.key_bindings[0] = DOOM_ACTION_FIRE;
    kcfg.key_bindings[1] = DOOM_ACTION_USE;
    doom_bridge_set_keypad_config(&kcfg);

    doom_bridge_inject_keypad_event(0, 1);
    assert(doom_bridge_is_fire_active() == 1);
    event_t ev;
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_FIRE);

    doom_bridge_inject_keypad_event(0, 0);
    assert(doom_bridge_is_fire_active() == 0);
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_FIRE);

    printf("  [PASS] test_keypad_custom_rebind\n");
}

static void test_touch_actions_to_doom_events(void) {
    printf("Running test_touch_actions_to_doom_events...\n");
    doom_bridge_init_headless();
    doom_bridge_clear_events();

    // 1. Virtual D-pad: Move Forward
    // Press stick at (100, 200), drag to (100, 140) -> move_forward > 0.5
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    touch_overlay_handle_motion(1, 100.0f / 480.0f, 140.0f / 320.0f);

    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 1);
    event_t ev;
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_UP);

    // Subsequent tick with stick held should NOT re-trigger keydown
    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 0);

    // Release stick
    touch_overlay_handle_up(1);
    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 1);
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_UP);

    // 2. Strafe Left / Right
    // Drag right to (160, 200) -> strafe_right > 0.5
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    touch_overlay_handle_motion(1, 160.0f / 480.0f, 200.0f / 320.0f);
    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 1);
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_STRAFE_R);

    // Drag left across center to (40, 200) -> strafe_right < -0.5
    touch_overlay_handle_motion(1, 40.0f / 480.0f, 200.0f / 320.0f);
    doom_bridge_process_input();
    // Should see keyup for strafe right, keydown for strafe left
    assert(doom_bridge_get_queued_events_count() == 2);
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_STRAFE_R);
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_STRAFE_L);

    touch_overlay_handle_up(1);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_STRAFE_L);

    // 3. Touch Fire Button (Top Right: 450, 120)
    touch_overlay_handle_down(2, 450.0f / 480.0f, 120.0f / 320.0f);
    assert(doom_bridge_is_fire_active() == 1);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_FIRE);

    touch_overlay_handle_up(2);
    assert(doom_bridge_is_fire_active() == 0);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_FIRE);

    // 4. Touch Use Button (Mid Right: 450, 200)
    touch_overlay_handle_down(3, 450.0f / 480.0f, 200.0f / 320.0f);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_USE);

    touch_overlay_handle_up(3);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_USE);

    // 5. Touch Menu / ESC (Top Left: 20, 20)
    touch_overlay_handle_down(4, 20.0f / 480.0f, 20.0f / 320.0f);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_ESCAPE);

    touch_overlay_handle_up(4);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_ESCAPE);

    // 6. Touch Automap (Top Right: 460, 20)
    touch_overlay_handle_down(5, 460.0f / 480.0f, 20.0f / 320.0f);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_TAB);

    touch_overlay_handle_up(5);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_TAB);

    // 7. Weapon Switch Prev / Next
    touch_overlay_handle_down(6, 370.0f / 480.0f, 280.0f / 320.0f); // Prev
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_PREV_WEAPON);
    touch_overlay_handle_up(6);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_PREV_WEAPON);

    touch_overlay_handle_down(7, 450.0f / 480.0f, 280.0f / 320.0f); // Next
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keydown);
    assert(ev.data1 == DOOM_KEY_NEXT_WEAPON);
    touch_overlay_handle_up(7);
    doom_bridge_process_input();
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_keyup);
    assert(ev.data1 == DOOM_KEY_NEXT_WEAPON);

    printf("  [PASS] test_touch_actions_to_doom_events\n");
}

static void test_turn_impulse_cleared_per_frame(void) {
    printf("Running test_turn_impulse_cleared_per_frame...\n");
    doom_bridge_init_headless();
    doom_bridge_clear_events();

    // Swipe right in look zone: X: 300 -> 350
    touch_overlay_handle_down(8, 300.0f / 480.0f, 160.0f / 320.0f);
    touch_overlay_handle_motion(8, 350.0f / 480.0f, 160.0f / 320.0f);

    touch_game_actions_t acts_before = touch_overlay_get_actions();
    assert(acts_before.turn_speed > 0.0f);

    // Frame 1: doom_bridge_process_input consumes impulse
    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 1);
    event_t ev;
    assert(doom_bridge_get_event(&ev) == 1);
    assert(ev.type == ev_mouse);
    assert(ev.data2 > 0); // Turning right impulse

    // CRITICAL VERIFICATION:
    // After tick 1, the turn impulse MUST be cleared in touch_overlay
    touch_game_actions_t acts_after = touch_overlay_get_actions();
    assert(acts_after.turn_speed == 0.0f);

    // Frame 2: with finger stationary on screen, no new turn impulse should be emitted
    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 0); // No spinning!

    touch_overlay_handle_up(8);
    doom_bridge_process_input();
    assert(doom_bridge_get_queued_events_count() == 0);

    printf("  [PASS] test_turn_impulse_cleared_per_frame\n");
}

static void test_sdl_event_pumping_and_forwarding(void) {
    printf("Running test_sdl_event_pumping_and_forwarding...\n");
    mock_reset_all();
    doom_bridge_init(NULL, g_dummy_renderer);
    doom_bridge_clear_events();

    // 1. Inject SDL_FINGERDOWN at Fire button zone
    SDL_Event ev_down;
    memset(&ev_down, 0, sizeof(ev_down));
    ev_down.type = SDL_FINGERDOWN;
    ev_down.tfinger.fingerId = 42;
    ev_down.tfinger.x = 450.0f / 480.0f;
    ev_down.tfinger.y = 120.0f / 320.0f;
    mock_sdl_push_event(&ev_down);

    doom_bridge_process_input();
    assert(doom_bridge_is_fire_active() == 1);
    event_t doom_ev;
    assert(doom_bridge_get_event(&doom_ev) == 1);
    assert(doom_ev.type == ev_keydown);
    assert(doom_ev.data1 == DOOM_KEY_FIRE);

    // 2. Inject SDL_FINGERUP
    SDL_Event ev_up;
    memset(&ev_up, 0, sizeof(ev_up));
    ev_up.type = SDL_FINGERUP;
    ev_up.tfinger.fingerId = 42;
    mock_sdl_push_event(&ev_up);

    doom_bridge_process_input();
    assert(doom_bridge_is_fire_active() == 0);
    assert(doom_bridge_get_event(&doom_ev) == 1);
    assert(doom_ev.type == ev_keyup);
    assert(doom_ev.data1 == DOOM_KEY_FIRE);

    // 3. Inject SDL_QUIT -> produces ESC event
    SDL_Event ev_quit;
    memset(&ev_quit, 0, sizeof(ev_quit));
    ev_quit.type = SDL_QUIT;
    mock_sdl_push_event(&ev_quit);

    doom_bridge_process_input();
    assert(doom_bridge_get_event(&doom_ev) == 1);
    assert(doom_ev.type == ev_keydown);
    assert(doom_ev.data1 == DOOM_KEY_ESCAPE);

    printf("  [PASS] test_sdl_event_pumping_and_forwarding\n");
}

static void test_display_scaler_and_present(void) {
    printf("Running test_display_scaler_and_present...\n");
    mock_reset_all();
    doom_bridge_init(NULL, g_dummy_renderer);

    // Render present with game texture
    doom_bridge_render_present(g_dummy_texture);

    // Verify clear was called
    assert(g_mock_sdl.clear_calls == 1);

    // Verify copy was called and destination is exactly 480x320
    assert(g_mock_sdl.copy_calls == 1);
    assert(g_mock_sdl.last_dst_rect.x == 0);
    assert(g_mock_sdl.last_dst_rect.y == 0);
    assert(g_mock_sdl.last_dst_rect.w == 480);
    assert(g_mock_sdl.last_dst_rect.h == 320);

    // Verify overlay render calls occurred (blend mode and fill/rects/lines)
    assert(g_mock_sdl.blend_mode_calls >= 1);
    assert(g_mock_sdl.fill_rect_calls >= 4);

    // Verify present was called
    assert(g_mock_sdl.present_calls == 1);

    // Null safety: render present with NULL renderer or NULL texture
    doom_bridge_init_headless();
    doom_bridge_render_present(g_dummy_texture); // Should not crash
    doom_bridge_render_present(NULL);            // Should not crash

    printf("  [PASS] test_display_scaler_and_present\n");
}

int main(void) {
    printf("Starting test_event_bridge suite...\n");
    test_step1_brief_scenario();
    test_keypad_event_translation_all_16();
    test_keypad_custom_rebind();
    test_touch_actions_to_doom_events();
    test_turn_impulse_cleared_per_frame();
    test_sdl_event_pumping_and_forwarding();
    test_display_scaler_and_present();
    printf("test_event_bridge PASSED\n");
    return 0;
}
