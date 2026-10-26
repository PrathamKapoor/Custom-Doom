#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "../src/hardware/touch_overlay.h"
#include "../src/hardware/config_manager.h"

/* Mock SDL2 Renderer state */
typedef struct {
    int blend_mode_calls;
    SDL_BlendMode last_blend_mode;
    int set_color_calls;
    unsigned char last_r, last_g, last_b, last_a;
    int fill_rect_calls;
    SDL_Rect last_fill_rect;
    int draw_rect_calls;
    SDL_Rect last_draw_rect;
    int draw_line_calls;
    int draw_point_calls;

    int total_rect_count;
    SDL_Rect recorded_rects[64];
    unsigned char recorded_colors[64][4];

    int total_line_count;
    int recorded_lines[128][4];
} mock_renderer_state_t;

static mock_renderer_state_t g_mock;
static SDL_Renderer *g_dummy_renderer = (SDL_Renderer *)0x12345678;

static void mock_reset(void) {
    memset(&g_mock, 0, sizeof(g_mock));
}

/* Mock SDL2 implementations */
int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode) {
    (void)renderer;
    g_mock.blend_mode_calls++;
    g_mock.last_blend_mode = blendMode;
    return 0;
}

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    (void)renderer;
    g_mock.set_color_calls++;
    g_mock.last_r = r;
    g_mock.last_g = g;
    g_mock.last_b = b;
    g_mock.last_a = a;
    return 0;
}

int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    (void)renderer;
    g_mock.fill_rect_calls++;
    if (rect) {
        g_mock.last_fill_rect = *rect;
        if (g_mock.total_rect_count < 64) {
            g_mock.recorded_rects[g_mock.total_rect_count] = *rect;
            g_mock.recorded_colors[g_mock.total_rect_count][0] = g_mock.last_r;
            g_mock.recorded_colors[g_mock.total_rect_count][1] = g_mock.last_g;
            g_mock.recorded_colors[g_mock.total_rect_count][2] = g_mock.last_b;
            g_mock.recorded_colors[g_mock.total_rect_count][3] = g_mock.last_a;
            g_mock.total_rect_count++;
        }
    }
    return 0;
}

int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    (void)renderer;
    g_mock.draw_rect_calls++;
    if (rect) {
        g_mock.last_draw_rect = *rect;
    }
    return 0;
}

int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2) {
    (void)renderer;
    g_mock.draw_line_calls++;
    if (g_mock.total_line_count < 128) {
        g_mock.recorded_lines[g_mock.total_line_count][0] = x1;
        g_mock.recorded_lines[g_mock.total_line_count][1] = y1;
        g_mock.recorded_lines[g_mock.total_line_count][2] = x2;
        g_mock.recorded_lines[g_mock.total_line_count][3] = y2;
        g_mock.total_line_count++;
    }
    return 0;
}

int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y) {
    (void)renderer;
    (void)x;
    (void)y;
    g_mock.draw_point_calls++;
    return 0;
}

/* Unit Tests */
static void test_opacity_cycling(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);

    touch_overlay_set_opacity(0.75f);
    assert(fabsf(touch_overlay_get_opacity() - 0.75f) < 0.01f);

    touch_overlay_cycle_hud_visibility(); // 0.75 -> 0.35
    assert(fabsf(touch_overlay_get_opacity() - 0.35f) < 0.01f);

    touch_overlay_cycle_hud_visibility(); // 0.35 -> 0.0
    assert(fabsf(touch_overlay_get_opacity() - 0.0f) < 0.01f);

    touch_overlay_cycle_hud_visibility(); // 0.0 -> 0.75
    assert(fabsf(touch_overlay_get_opacity() - 0.75f) < 0.01f);

    printf("  [PASS] test_opacity_cycling\n");
}

static void test_hidden_hud_rendering(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);

    touch_overlay_set_opacity(0.0f);
    mock_reset();

    touch_overlay_render(g_dummy_renderer);

    // When opacity is 0.0 (hidden), rendering should be skipped entirely
    assert(g_mock.blend_mode_calls == 0);
    assert(g_mock.fill_rect_calls == 0);
    assert(g_mock.draw_rect_calls == 0);
    assert(g_mock.draw_line_calls == 0);
    assert(g_mock.draw_point_calls == 0);

    printf("  [PASS] test_hidden_hud_rendering\n");
}

static void test_null_renderer_safety(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_set_opacity(0.75f);
    mock_reset();

    // Calling with NULL renderer must not crash or make render calls
    touch_overlay_render(NULL);
    assert(g_mock.blend_mode_calls == 0);
    assert(g_mock.fill_rect_calls == 0);

    printf("  [PASS] test_null_renderer_safety\n");
}

static void test_idle_hud_rendering(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_set_opacity(0.75f);
    mock_reset();

    touch_overlay_render(g_dummy_renderer);

    // Verify blend mode was enabled
    assert(g_mock.blend_mode_calls >= 1);
    assert(g_mock.last_blend_mode == SDL_BLENDMODE_BLEND);

    // Verify geometric HUD elements were drawn:
    // Menu, Automap, Fire, Use, Prev Weapon, Next Weapon, etc.
    assert(g_mock.fill_rect_calls >= 4);
    assert(g_mock.draw_line_calls > 0);

    // Check that all recorded rectangles fall within screen bounds (480x320)
    for (int i = 0; i < g_mock.total_rect_count; i++) {
        SDL_Rect r = g_mock.recorded_rects[i];
        assert(r.x >= 0);
        assert(r.y >= 0);
        assert(r.x + r.w <= 480);
        assert(r.y + r.h <= 320);
    }

    printf("  [PASS] test_idle_hud_rendering\n");
}

static void test_button_pressed_highlights(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_set_opacity(0.75f);

    // 1. Idle render baseline
    mock_reset();
    touch_overlay_render(g_dummy_renderer);
    int idle_rect_count = g_mock.total_rect_count;
    assert(idle_rect_count > 0);

    // 2. Fire button pressed: Tap at (430, 110)
    touch_overlay_handle_down(2, 430.0f / 480.0f, 110.0f / 320.0f);
    touch_game_actions_t acts = touch_overlay_get_actions();
    assert(acts.fire_pressed == 1);

    mock_reset();
    touch_overlay_render(g_dummy_renderer);

    // Check that a fire rect was drawn with bright red highlight
    int found_fire_highlight = 0;
    for (int i = 0; i < g_mock.total_rect_count; i++) {
        SDL_Rect r = g_mock.recorded_rects[i];
        // Fire button is on the top-right
        if (r.x >= 350 && r.y >= 60 && r.y <= 160) {
            unsigned char red = g_mock.recorded_colors[i][0];
            unsigned char green = g_mock.recorded_colors[i][1];
            unsigned char alpha = g_mock.recorded_colors[i][3];
            if (red > 200 && green < 100 && alpha > 120) {
                found_fire_highlight = 1;
                break;
            }
        }
    }
    assert(found_fire_highlight == 1);
    touch_overlay_handle_up(2);

    // 3. Use button pressed: Tap at (430, 200)
    touch_overlay_handle_down(3, 430.0f / 480.0f, 200.0f / 320.0f);
    acts = touch_overlay_get_actions();
    assert(acts.use_pressed == 1);

    mock_reset();
    touch_overlay_render(g_dummy_renderer);

    int found_use_highlight = 0;
    for (int i = 0; i < g_mock.total_rect_count; i++) {
        SDL_Rect r = g_mock.recorded_rects[i];
        // Use button is on mid-right
        if (r.x >= 350 && r.y >= 150 && r.y <= 245) {
            unsigned char red = g_mock.recorded_colors[i][0];
            unsigned char green = g_mock.recorded_colors[i][1];
            unsigned char alpha = g_mock.recorded_colors[i][3];
            // Gold/Yellow highlight (high red, high green)
            if (red > 200 && green > 150 && alpha > 120) {
                found_use_highlight = 1;
                break;
            }
        }
    }
    assert(found_use_highlight == 1);
    touch_overlay_handle_up(3);

    printf("  [PASS] test_button_pressed_highlights\n");
}

static void test_active_thumbstick_rendering(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_set_opacity(0.75f);

    // Idle: no active stick
    mock_reset();
    touch_overlay_render(g_dummy_renderer);
    int idle_lines = g_mock.draw_line_calls;

    // Press thumbstick at (100, 200) and drag to (120, 160)
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    touch_overlay_handle_motion(1, 120.0f / 480.0f, 160.0f / 320.0f);

    int stick_active = 0;
    float ax = 0, ay = 0, cx = 0, cy = 0;
    touch_overlay_get_stick_state(&stick_active, &ax, &ay, &cx, &cy);
    assert(stick_active == 1);

    mock_reset();
    touch_overlay_render(g_dummy_renderer);

    // When thumbstick is active:
    // Dynamic anchor circle, connector line, and knob circle should be rendered
    assert(g_mock.draw_line_calls > idle_lines || g_mock.draw_point_calls > 0);

    // Verify connector line was drawn between anchor and knob
    int found_connector = 0;
    for (int i = 0; i < g_mock.total_line_count; i++) {
        int x1 = g_mock.recorded_lines[i][0];
        int y1 = g_mock.recorded_lines[i][1];
        int x2 = g_mock.recorded_lines[i][2];
        int y2 = g_mock.recorded_lines[i][3];
        if (abs(x1 - (int)ax) <= 2 && abs(y1 - (int)ay) <= 2 &&
            abs(x2 - (int)cx) <= 2 && abs(y2 - (int)cy) <= 2) {
            found_connector = 1;
            break;
        }
    }
    assert(found_connector == 1);

    touch_overlay_handle_up(1);
    printf("  [PASS] test_active_thumbstick_rendering\n");
}

static void test_touch_disabled(void) {
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    cfg.touch_enabled = 0;
    touch_overlay_init(&cfg, 480, 320);

    mock_reset();
    touch_overlay_render(g_dummy_renderer);

    // If touch_enabled is 0, HUD rendering should be skipped
    assert(g_mock.blend_mode_calls == 0);
    assert(g_mock.fill_rect_calls == 0);

    printf("  [PASS] test_touch_disabled\n");
}

int main(void) {
    printf("Starting test_touch_render...\n");
    test_opacity_cycling();
    test_hidden_hud_rendering();
    test_null_renderer_safety();
    test_idle_hud_rendering();
    test_button_pressed_highlights();
    test_active_thumbstick_rendering();
    test_touch_disabled();
    printf("test_touch_render PASSED\n");
    return 0;
}
