#include "touch_overlay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int active;
    int finger_id;
    float start_x;
    float start_y;
    float current_x;
    float current_y;
    float last_x;
    float last_y;
    float turn_speed;
    touch_zone_t zone;
} active_finger_t;

static touch_config_t g_config;
static int g_screen_w = 480;
static int g_screen_h = 320;
static touch_game_actions_t g_actions;
static active_finger_t g_fingers[MAX_TOUCH_FINGERS];

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static touch_zone_t classify_touch_zone(float px, float py) {
    float sw = (float)g_screen_w;
    float sh = (float)g_screen_h;

    // Top-Left: Menu / ESC
    if (px >= 0.0f && px <= 60.0f && py >= 0.0f && py <= 60.0f) {
        return TOUCH_ZONE_MENU;
    }

    // Top-Right: Automap
    if (px >= (sw - 60.0f) && px <= sw && py >= 0.0f && py <= 60.0f) {
        return TOUCH_ZONE_AUTOMAP;
    }

    // Center-Top: HUD Visibility Toggle
    if (px >= (sw * 0.35f) && px <= (sw * 0.65f) && py >= 0.0f && py <= 50.0f) {
        return TOUCH_ZONE_TOGGLE_HUD;
    }

    // Action Zones on the right side:
    // Fire button: Top-right action zone
    if (px >= (sw * 0.80f) && px <= sw && py >= (sh * 0.22f) && py <= (sh * 0.50f)) {
        return TOUCH_ZONE_FIRE;
    }

    // Use button: Mid-right action zone
    if (px >= (sw * 0.80f) && px <= sw && py > (sh * 0.50f) && py <= (sh * 0.75f)) {
        return TOUCH_ZONE_USE;
    }

    // Prev Weapon button: Bottom-right
    if (px >= (sw * 0.68f) && px < (sw * 0.84f) && py > (sh * 0.75f)) {
        return TOUCH_ZONE_PREV_WEAPON;
    }

    // Next Weapon button: Bottom-right edge
    if (px >= (sw * 0.84f) && px <= sw && py > (sh * 0.75f)) {
        return TOUCH_ZONE_NEXT_WEAPON;
    }

    // Left side: Floating virtual thumbstick
    if (px <= (sw * 0.50f)) {
        return TOUCH_ZONE_STICK;
    }

    // Unoccupied right side: Look / Turn swipe
    return TOUCH_ZONE_LOOK;
}

static void touch_overlay_recompute_actions(void) {
    memset(&g_actions, 0, sizeof(g_actions));

    if (!g_config.touch_enabled) {
        return;
    }

    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (!g_fingers[i].active) {
            continue;
        }

        switch (g_fingers[i].zone) {
            case TOUCH_ZONE_STICK: {
                float dx = g_fingers[i].current_x - g_fingers[i].start_x;
                float dy = g_fingers[i].current_y - g_fingers[i].start_y;
                float dist = sqrtf(dx * dx + dy * dy);

                float deadzone = (float)g_config.dpad_deadzone;
                if (deadzone < 0.0f) deadzone = 0.0f;
                float max_radius = TOUCH_STICK_MAX_RADIUS;
                if (max_radius <= deadzone) {
                    max_radius = deadzone + 45.0f;
                }

                if (dist > deadzone) {
                    float norm_dist = (dist - deadzone) / (max_radius - deadzone);
                    if (norm_dist > 1.0f) norm_dist = 1.0f;

                    float ux = dx / dist;
                    float uy = dy / dist;

                    g_actions.strafe_right = ux * norm_dist;
                    // Y increases downwards on screen; upward drag (dy < 0) moves forward
                    g_actions.move_forward = -uy * norm_dist;
                }
                break;
            }
            case TOUCH_ZONE_FIRE:
                g_actions.fire_pressed = 1;
                break;
            case TOUCH_ZONE_USE:
                g_actions.use_pressed = 1;
                break;
            case TOUCH_ZONE_PREV_WEAPON:
                g_actions.prev_weapon = 1;
                break;
            case TOUCH_ZONE_NEXT_WEAPON:
                g_actions.next_weapon = 1;
                break;
            case TOUCH_ZONE_MENU:
                g_actions.menu_pressed = 1;
                break;
            case TOUCH_ZONE_AUTOMAP:
                g_actions.automap_pressed = 1;
                break;
            case TOUCH_ZONE_TOGGLE_HUD:
                g_actions.toggle_hud = 1;
                break;
            case TOUCH_ZONE_LOOK:
                g_actions.turn_speed = g_fingers[i].turn_speed;
                break;
            default:
                break;
        }
    }
}

void touch_overlay_init(const touch_config_t *config, int screen_w, int screen_h) {
    if (config != NULL) {
        g_config = *config;
    } else {
        config_set_defaults(&g_config, NULL);
    }

    g_screen_w = (screen_w > 0) ? screen_w : 480;
    g_screen_h = (screen_h > 0) ? screen_h : 320;

    memset(g_fingers, 0, sizeof(g_fingers));
    memset(&g_actions, 0, sizeof(g_actions));
}

void touch_overlay_handle_down(int finger_id, float norm_x, float norm_y) {
    if (!g_config.touch_enabled) return;

    norm_x = clampf(norm_x, 0.0f, 1.0f);
    norm_y = clampf(norm_y, 0.0f, 1.0f);

    float px = norm_x * (float)g_screen_w;
    float py = norm_y * (float)g_screen_h;

    // Find slot for finger or first free slot
    int slot = -1;
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (g_fingers[i].active && g_fingers[i].finger_id == finger_id) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (!g_fingers[i].active) {
                slot = i;
                break;
            }
        }
    }
    if (slot == -1) {
        return; // No free slots
    }

    touch_zone_t zone = classify_touch_zone(px, py);

    g_fingers[slot].active = 1;
    g_fingers[slot].finger_id = finger_id;
    g_fingers[slot].zone = zone;
    g_fingers[slot].start_x = px;
    g_fingers[slot].start_y = py;
    g_fingers[slot].current_x = px;
    g_fingers[slot].current_y = py;
    g_fingers[slot].last_x = px;
    g_fingers[slot].last_y = py;
    g_fingers[slot].turn_speed = 0.0f;

    touch_overlay_recompute_actions();
}

void touch_overlay_handle_motion(int finger_id, float norm_x, float norm_y) {
    if (!g_config.touch_enabled) return;

    norm_x = clampf(norm_x, 0.0f, 1.0f);
    norm_y = clampf(norm_y, 0.0f, 1.0f);

    float px = norm_x * (float)g_screen_w;
    float py = norm_y * (float)g_screen_h;

    int slot = -1;
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (g_fingers[i].active && g_fingers[i].finger_id == finger_id) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        return;
    }

    if (g_fingers[slot].zone == TOUCH_ZONE_STICK) {
        g_fingers[slot].current_x = px;
        g_fingers[slot].current_y = py;
    } else if (g_fingers[slot].zone == TOUCH_ZONE_LOOK) {
        float dx = px - g_fingers[slot].last_x;
        float sens = (g_config.turn_sensitivity > 0.0f) ? g_config.turn_sensitivity : 1.2f;
        float turn = (dx / ((float)g_screen_w * 0.15f)) * sens;
        g_fingers[slot].turn_speed = clampf(turn, -1.0f, 1.0f);
        g_fingers[slot].last_x = px;
        g_fingers[slot].last_y = py;
    }

    touch_overlay_recompute_actions();
}

void touch_overlay_handle_up(int finger_id) {
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (g_fingers[i].active && g_fingers[i].finger_id == finger_id) {
            g_fingers[i].active = 0;
            g_fingers[i].turn_speed = 0.0f;
            break;
        }
    }

    touch_overlay_recompute_actions();
}

touch_game_actions_t touch_overlay_get_actions(void) {
    return g_actions;
}

void touch_overlay_clear_turn_impulse(void) {
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (g_fingers[i].active && g_fingers[i].zone == TOUCH_ZONE_LOOK) {
            g_fingers[i].turn_speed = 0.0f;
        }
    }
    g_actions.turn_speed = 0.0f;
}

void touch_overlay_transform_raw(int raw_x, int raw_y, float *norm_x, float *norm_y) {
    float u = clampf((float)raw_x / 4095.0f, 0.0f, 1.0f);
    float v = clampf((float)raw_y / 4095.0f, 0.0f, 1.0f);

    float nx = u;
    float ny = v;

    switch (g_config.screen_rotation) {
        case 90:
            nx = 1.0f - v;
            ny = u;
            break;
        case 180:
            nx = 1.0f - u;
            ny = 1.0f - v;
            break;
        case 270:
            nx = v;
            ny = 1.0f - u;
            break;
        case 0:
        default:
            nx = u;
            ny = v;
            break;
    }

    if (norm_x != NULL) *norm_x = nx;
    if (norm_y != NULL) *norm_y = ny;
}

void touch_overlay_apply_iir_filter(float raw_x, float raw_y, float prev_x, float prev_y, float *out_x, float *out_y) {
    if (out_x != NULL) *out_x = 0.7f * raw_x + 0.3f * prev_x;
    if (out_y != NULL) *out_y = 0.7f * raw_y + 0.3f * prev_y;
}

void touch_overlay_set_opacity(float opacity) {
    g_config.touch_opacity = clampf(opacity, 0.0f, 1.0f);
}

float touch_overlay_get_opacity(void) {
    return g_config.touch_opacity;
}

void touch_overlay_cycle_hud_visibility(void) {
    if (g_config.touch_opacity > 0.50f) {
        g_config.touch_opacity = 0.35f;
    } else if (g_config.touch_opacity > 0.10f) {
        g_config.touch_opacity = 0.0f;
    } else {
        g_config.touch_opacity = 0.75f;
    }
}

int touch_overlay_get_stick_state(int *active, float *anchor_x, float *anchor_y, float *curr_x, float *curr_y) {
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (g_fingers[i].active && g_fingers[i].zone == TOUCH_ZONE_STICK) {
            if (active != NULL) *active = 1;
            if (anchor_x != NULL) *anchor_x = g_fingers[i].start_x;
            if (anchor_y != NULL) *anchor_y = g_fingers[i].start_y;
            if (curr_x != NULL) *curr_x = g_fingers[i].current_x;
            if (curr_y != NULL) *curr_y = g_fingers[i].current_y;
            return 1;
        }
    }

    if (active != NULL) *active = 0;
    return 0;
}

#if !defined(_SDL_H) && !defined(SDL_h_)
__attribute__((weak)) int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode) {
    (void)renderer; (void)blendMode; return 0;
}
__attribute__((weak)) int SDL_SetRenderDrawColor(SDL_Renderer *renderer, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    (void)renderer; (void)r; (void)g; (void)b; (void)a; return 0;
}
__attribute__((weak)) int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    (void)renderer; (void)rect; return 0;
}
__attribute__((weak)) int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    (void)renderer; (void)rect; return 0;
}
__attribute__((weak)) int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2) {
    (void)renderer; (void)x1; (void)y1; (void)x2; (void)y2; return 0;
}
__attribute__((weak)) int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y) {
    (void)renderer; (void)x; (void)y; return 0;
}
#endif

#ifndef TOUCH_PI
#define TOUCH_PI 3.14159265358979323846f
#endif

static void draw_circle_lines(SDL_Renderer *renderer, int cx, int cy, int radius, int segments) {
    if (radius <= 0 || segments < 3) return;
    float step = (2.0f * TOUCH_PI) / (float)segments;
    int prev_x = cx + (int)((float)radius * cosf(0.0f));
    int prev_y = cy + (int)((float)radius * sinf(0.0f));

    for (int i = 1; i <= segments; i++) {
        float angle = (float)i * step;
        int next_x = cx + (int)((float)radius * cosf(angle));
        int next_y = cy + (int)((float)radius * sinf(angle));
        SDL_RenderDrawLine(renderer, prev_x, prev_y, next_x, next_y);
        prev_x = next_x;
        prev_y = next_y;
    }
}

void touch_overlay_render(SDL_Renderer *renderer) {
    if (renderer == NULL) {
        return;
    }
    if (!g_config.touch_enabled) {
        return;
    }
    if (g_config.touch_opacity <= 0.001f) {
        return;
    }

    float opacity = clampf(g_config.touch_opacity, 0.0f, 1.0f);
    float sw = (float)g_screen_w;
    float sh = (float)g_screen_h;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 1. Menu / ESC button (Top-Left)
    SDL_Rect menu_rect = { 6, 6, 48, 36 };
    if (g_actions.menu_pressed) {
        SDL_SetRenderDrawColor(renderer, 220, 50, 50, (unsigned char)clampf(opacity * 255.0f * 1.2f, 130.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 50, 60, (unsigned char)(opacity * 130.0f));
    }
    SDL_RenderFillRect(renderer, &menu_rect);
    SDL_SetRenderDrawColor(renderer, 180, 180, 190, (unsigned char)(opacity * 220.0f));
    SDL_RenderDrawRect(renderer, &menu_rect);
    SDL_RenderDrawLine(renderer, menu_rect.x + 10, menu_rect.y + 10, menu_rect.x + menu_rect.w - 10, menu_rect.y + 10);
    SDL_RenderDrawLine(renderer, menu_rect.x + 10, menu_rect.y + 18, menu_rect.x + menu_rect.w - 10, menu_rect.y + 18);
    SDL_RenderDrawLine(renderer, menu_rect.x + 10, menu_rect.y + 26, menu_rect.x + menu_rect.w - 10, menu_rect.y + 26);

    // 2. Automap button (Top-Right)
    int am_w = 48;
    int am_h = 36;
    SDL_Rect automap_rect = { g_screen_w - am_w - 6, 6, am_w, am_h };
    if (g_actions.automap_pressed) {
        SDL_SetRenderDrawColor(renderer, 50, 200, 80, (unsigned char)clampf(opacity * 255.0f * 1.2f, 130.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 40, 60, 50, (unsigned char)(opacity * 130.0f));
    }
    SDL_RenderFillRect(renderer, &automap_rect);
    SDL_SetRenderDrawColor(renderer, 160, 200, 170, (unsigned char)(opacity * 220.0f));
    SDL_RenderDrawRect(renderer, &automap_rect);
    int ax1 = automap_rect.x + 12;
    int ax2 = automap_rect.x + automap_rect.w / 2;
    int ax3 = automap_rect.x + automap_rect.w - 12;
    int ay_top = automap_rect.y + 10;
    int ay_mid = automap_rect.y + 19;
    int ay_bot = automap_rect.y + automap_rect.h - 10;
    SDL_RenderDrawLine(renderer, ax1, ay_bot, ax1, ay_top);
    SDL_RenderDrawLine(renderer, ax1, ay_top, ax2, ay_mid);
    SDL_RenderDrawLine(renderer, ax2, ay_mid, ax3, ay_top);
    SDL_RenderDrawLine(renderer, ax3, ay_top, ax3, ay_bot);

    // 3. Center-Top HUD Visibility Toggle
    int hud_w = (int)(sw * 0.16f);
    int hud_h = 24;
    int hud_x = (g_screen_w - hud_w) / 2;
    SDL_Rect hud_rect = { hud_x, 4, hud_w, hud_h };
    if (g_actions.toggle_hud) {
        SDL_SetRenderDrawColor(renderer, 90, 140, 220, (unsigned char)clampf(opacity * 255.0f * 1.2f, 130.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 45, 50, 60, (unsigned char)(opacity * 110.0f));
    }
    SDL_RenderFillRect(renderer, &hud_rect);
    SDL_SetRenderDrawColor(renderer, 150, 160, 180, (unsigned char)(opacity * 180.0f));
    SDL_RenderDrawRect(renderer, &hud_rect);
    int hx_mid = hud_rect.x + hud_rect.w / 2;
    int hy_mid = hud_rect.y + hud_rect.h / 2;
    SDL_RenderDrawLine(renderer, hx_mid - 12, hy_mid, hx_mid + 12, hy_mid);
    SDL_RenderDrawLine(renderer, hx_mid, hy_mid - 4, hx_mid, hy_mid + 4);

    // 4. Fire button (Right Top)
    int fire_x = (int)(sw * 0.81f);
    int fire_y = (int)(sh * 0.23f);
    int fire_w = (int)(sw * 0.17f);
    int fire_h = (int)(sh * 0.25f);
    SDL_Rect fire_rect = { fire_x, fire_y, fire_w, fire_h };
    if (g_actions.fire_pressed) {
        SDL_SetRenderDrawColor(renderer, 245, 35, 35, (unsigned char)clampf(opacity * 255.0f * 1.2f, 140.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 140, 35, 35, (unsigned char)(opacity * 110.0f));
    }
    SDL_RenderFillRect(renderer, &fire_rect);
    SDL_SetRenderDrawColor(renderer, 255, 120, 120, (unsigned char)(opacity * 220.0f));
    SDL_RenderDrawRect(renderer, &fire_rect);
    int fx_mid = fire_rect.x + fire_rect.w / 2;
    int fy_mid = fire_rect.y + fire_rect.h / 2;
    SDL_RenderDrawLine(renderer, fx_mid - 14, fy_mid, fx_mid + 14, fy_mid);
    SDL_RenderDrawLine(renderer, fx_mid, fy_mid - 14, fx_mid, fy_mid + 14);

    // 5. Use button (Right Mid)
    int use_x = (int)(sw * 0.81f);
    int use_y = (int)(sh * 0.52f);
    int use_w = (int)(sw * 0.17f);
    int use_h = (int)(sh * 0.21f);
    SDL_Rect use_rect = { use_x, use_y, use_w, use_h };
    if (g_actions.use_pressed) {
        SDL_SetRenderDrawColor(renderer, 245, 205, 30, (unsigned char)clampf(opacity * 255.0f * 1.2f, 140.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 110, 25, (unsigned char)(opacity * 110.0f));
    }
    SDL_RenderFillRect(renderer, &use_rect);
    SDL_SetRenderDrawColor(renderer, 255, 230, 100, (unsigned char)(opacity * 220.0f));
    SDL_RenderDrawRect(renderer, &use_rect);
    int ux_mid = use_rect.x + use_rect.w / 2;
    int uy_mid = use_rect.y + use_rect.h / 2;
    SDL_RenderDrawLine(renderer, ux_mid - 8, uy_mid - 8, ux_mid - 8, uy_mid + 8);
    SDL_RenderDrawLine(renderer, ux_mid - 8, uy_mid + 8, ux_mid + 8, uy_mid + 8);
    SDL_RenderDrawLine(renderer, ux_mid + 8, uy_mid + 8, ux_mid + 8, uy_mid - 8);

    // 6. Prev Weapon button (Right Bottom-Left: '<')
    int pw_x = (int)(sw * 0.69f);
    int pw_y = (int)(sh * 0.77f);
    int pw_w = (int)(sw * 0.13f);
    int pw_h = (int)(sh * 0.20f);
    SDL_Rect pw_rect = { pw_x, pw_y, pw_w, pw_h };
    if (g_actions.prev_weapon) {
        SDL_SetRenderDrawColor(renderer, 60, 180, 240, (unsigned char)clampf(opacity * 255.0f * 1.2f, 130.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 45, 60, 75, (unsigned char)(opacity * 110.0f));
    }
    SDL_RenderFillRect(renderer, &pw_rect);
    SDL_SetRenderDrawColor(renderer, 140, 200, 250, (unsigned char)(opacity * 220.0f));
    SDL_RenderDrawRect(renderer, &pw_rect);
    int pw_mid_x = pw_rect.x + pw_rect.w / 2;
    int pw_mid_y = pw_rect.y + pw_rect.h / 2;
    SDL_RenderDrawLine(renderer, pw_mid_x + 6, pw_mid_y - 10, pw_mid_x - 6, pw_mid_y);
    SDL_RenderDrawLine(renderer, pw_mid_x - 6, pw_mid_y, pw_mid_x + 6, pw_mid_y + 10);

    // 7. Next Weapon button (Right Bottom-Right: '>')
    int nw_x = (int)(sw * 0.85f);
    int nw_y = (int)(sh * 0.77f);
    int nw_w = (int)(sw * 0.13f);
    int nw_h = (int)(sh * 0.20f);
    SDL_Rect nw_rect = { nw_x, nw_y, nw_w, nw_h };
    if (g_actions.next_weapon) {
        SDL_SetRenderDrawColor(renderer, 60, 180, 240, (unsigned char)clampf(opacity * 255.0f * 1.2f, 130.0f, 255.0f));
    } else {
        SDL_SetRenderDrawColor(renderer, 45, 60, 75, (unsigned char)(opacity * 110.0f));
    }
    SDL_RenderFillRect(renderer, &nw_rect);
    SDL_SetRenderDrawColor(renderer, 140, 200, 250, (unsigned char)(opacity * 220.0f));
    SDL_RenderDrawRect(renderer, &nw_rect);
    int nw_mid_x = nw_rect.x + nw_rect.w / 2;
    int nw_mid_y = nw_rect.y + nw_rect.h / 2;
    SDL_RenderDrawLine(renderer, nw_mid_x - 6, nw_mid_y - 10, nw_mid_x + 6, nw_mid_y);
    SDL_RenderDrawLine(renderer, nw_mid_x + 6, nw_mid_y, nw_mid_x - 6, nw_mid_y + 10);

    // 8. Virtual Thumbstick (Dynamic active stick or idle guide)
    int stick_active = 0;
    float ax = 0.0f, ay = 0.0f, cx = 0.0f, cy = 0.0f;
    touch_overlay_get_stick_state(&stick_active, &ax, &ay, &cx, &cy);

    if (stick_active) {
        // Connector line from anchor (ax, ay) to knob (cx, cy)
        SDL_SetRenderDrawColor(renderer, 220, 240, 255, (unsigned char)clampf(opacity * 255.0f, 120.0f, 255.0f));
        SDL_RenderDrawLine(renderer, (int)ax, (int)ay, (int)cx, (int)cy);

        // Anchor outer boundary circle (max radius)
        SDL_SetRenderDrawColor(renderer, 100, 180, 255, (unsigned char)(opacity * 160.0f));
        draw_circle_lines(renderer, (int)ax, (int)ay, (int)TOUCH_STICK_MAX_RADIUS, 16);

        // Anchor inner deadzone circle
        int deadzone = g_config.dpad_deadzone > 0 ? g_config.dpad_deadzone : 15;
        SDL_SetRenderDrawColor(renderer, 80, 120, 180, (unsigned char)(opacity * 120.0f));
        draw_circle_lines(renderer, (int)ax, (int)ay, deadzone, 8);

        // Knob circle at current touch position
        SDL_SetRenderDrawColor(renderer, 240, 250, 255, (unsigned char)clampf(opacity * 255.0f * 1.2f, 140.0f, 255.0f));
        draw_circle_lines(renderer, (int)cx, (int)cy, 16, 12);
    } else {
        // Idle stick guide in lower-left area
        int guide_x = (int)(sw * 0.22f);
        int guide_y = (int)(sh * 0.68f);

        SDL_SetRenderDrawColor(renderer, 120, 140, 170, (unsigned char)(opacity * 100.0f));
        draw_circle_lines(renderer, guide_x, guide_y, (int)(TOUCH_STICK_MAX_RADIUS * 0.75f), 12);

        // Crosshair in center
        SDL_RenderDrawLine(renderer, guide_x - 14, guide_y, guide_x + 14, guide_y);
        SDL_RenderDrawLine(renderer, guide_x, guide_y - 14, guide_x, guide_y + 14);
    }
}
