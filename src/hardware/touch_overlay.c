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

void touch_overlay_render(SDL_Renderer *renderer) {
    (void)renderer;
    // Overlay HUD rendering will be expanded in Task 3
}
