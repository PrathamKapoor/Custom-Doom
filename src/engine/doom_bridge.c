#include "doom_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EVENT_QUEUE_MAX 256
#define MOVE_THRESHOLD 0.20f

/* Internal State */
static SDL_Window    *g_window = NULL;
static SDL_Renderer  *g_renderer = NULL;
static int            g_is_headless = 0;

static touch_config_t  g_touch_config;
static keypad_config_t g_keypad_config;

static doom_post_event_cb_t g_post_event_cb = NULL;

/* Event queue */
static event_t g_event_queue[EVENT_QUEUE_MAX];
static int     g_queue_head = 0;
static int     g_queue_tail = 0;
static int     g_queue_count = 0;

/* Fire active states */
static int g_injected_fire = 0;
static int g_keypad_fire = 0;

/* Previous touch action states for transition edge detection */
static int g_prev_move_forward = 0;
static int g_prev_move_backward = 0;
static int g_prev_strafe_left = 0;
static int g_prev_strafe_right = 0;
static int g_prev_touch_fire = 0;
static int g_prev_touch_use = 0;
static int g_prev_touch_menu = 0;
static int g_prev_touch_automap = 0;
static int g_prev_touch_toggle_hud = 0;
static int g_prev_touch_prev_weapon = 0;
static int g_prev_touch_next_weapon = 0;

static void reset_internal_state(void) {
    g_injected_fire = 0;
    g_keypad_fire = 0;
    g_prev_move_forward = 0;
    g_prev_move_backward = 0;
    g_prev_strafe_left = 0;
    g_prev_strafe_right = 0;
    g_prev_touch_fire = 0;
    g_prev_touch_use = 0;
    g_prev_touch_menu = 0;
    g_prev_touch_automap = 0;
    g_prev_touch_toggle_hud = 0;
    g_prev_touch_prev_weapon = 0;
    g_prev_touch_next_weapon = 0;
    doom_bridge_clear_events();
}

int doom_bridge_action_to_keycode(int action_or_key) {
    switch (action_or_key) {
        case DOOM_ACTION_STRAFE_LEFT:   return DOOM_KEY_STRAFE_L;
        case DOOM_ACTION_MOVE_FORWARD:  return DOOM_KEY_UP;
        case DOOM_ACTION_STRAFE_RIGHT:  return DOOM_KEY_STRAFE_R;
        case DOOM_ACTION_FIRE:          return DOOM_KEY_FIRE;
        case DOOM_ACTION_TURN_LEFT:     return DOOM_KEY_LEFT;
        case DOOM_ACTION_MOVE_BACKWARD: return DOOM_KEY_DOWN;
        case DOOM_ACTION_TURN_RIGHT:    return DOOM_KEY_RIGHT;
        case DOOM_ACTION_USE:           return DOOM_KEY_USE;
        case DOOM_ACTION_PREV_WEAPON:   return DOOM_KEY_PREV_WEAPON;
        case DOOM_ACTION_SPEED:         return DOOM_KEY_SPEED;
        case DOOM_ACTION_NEXT_WEAPON:   return DOOM_KEY_NEXT_WEAPON;
        case DOOM_ACTION_AUTOMAP:       return DOOM_KEY_TAB;
        case DOOM_ACTION_MENU:          return DOOM_KEY_ESCAPE;
        case DOOM_ACTION_ENTER:         return DOOM_KEY_ENTER;
        case DOOM_ACTION_GAMMA:         return DOOM_KEY_GAMMA;
        case DOOM_ACTION_TOGGLE_HUD:    return 0; /* special handler */
        default:                        return action_or_key;
    }
}

int doom_bridge_sdl_key_to_doom_key(int sdl_sym) {
    switch (sdl_sym) {
        case 27:            /* SDLK_ESCAPE */
            return DOOM_KEY_ESCAPE;
        case 13:            /* SDLK_RETURN */
        case 10:
            return DOOM_KEY_ENTER;
        case 9:             /* SDLK_TAB */
            return DOOM_KEY_TAB;
        case 32:            /* SDLK_SPACE */
            return DOOM_KEY_USE;
        case 8:             /* SDLK_BACKSPACE */
            return DOOM_KEY_BACKSPACE;
        case 0x40000052:    /* SDLK_UP */
            return DOOM_KEY_UP;
        case 0x40000051:    /* SDLK_DOWN */
            return DOOM_KEY_DOWN;
        case 0x40000050:    /* SDLK_LEFT */
            return DOOM_KEY_LEFT;
        case 0x4000004f:    /* SDLK_RIGHT */
            return DOOM_KEY_RIGHT;
        case 0x400000e0:    /* SDLK_LCTRL */
        case 0x400000e4:    /* SDLK_RCTRL */
            return DOOM_KEY_RCTRL;
        case 0x400000e1:    /* SDLK_LSHIFT */
        case 0x400000e5:    /* SDLK_RSHIFT */
            return DOOM_KEY_RSHIFT;
        case 0x400000e2:    /* SDLK_LALT */
        case 0x400000e6:    /* SDLK_RALT */
            return DOOM_KEY_RALT;
        case 0x40000044:    /* SDLK_F11 */
            return DOOM_KEY_F11;
        case ',':
            return DOOM_KEY_STRAFE_L;
        case '.':
            return DOOM_KEY_STRAFE_R;
        case '[':
            return DOOM_KEY_PREV_WEAPON;
        case ']':
            return DOOM_KEY_NEXT_WEAPON;
        default:
            if (sdl_sym >= 32 && sdl_sym <= 126) {
                return sdl_sym;
            }
            return 0;
    }
}

void doom_bridge_init(SDL_Window *window, SDL_Renderer *renderer) {
    g_window = window;
    g_renderer = renderer;
    g_is_headless = (renderer == NULL) ? 1 : 0;

    if (config_load(NULL, &g_touch_config, &g_keypad_config) != 0) {
        config_set_defaults(&g_touch_config, &g_keypad_config);
    }

    touch_overlay_init(&g_touch_config, DOOM_DISPLAY_WIDTH, DOOM_DISPLAY_HEIGHT);

    if (g_keypad_config.keypad_enabled) {
        matrix_keypad_init(&g_keypad_config, doom_bridge_on_keypad_event);
        matrix_keypad_start_thread();
    }

    reset_internal_state();
}

void doom_bridge_init_headless(void) {
    g_window = NULL;
    g_renderer = NULL;
    g_is_headless = 1;

    config_set_defaults(&g_touch_config, &g_keypad_config);
    touch_overlay_init(&g_touch_config, DOOM_DISPLAY_WIDTH, DOOM_DISPLAY_HEIGHT);

    reset_internal_state();
}

void doom_bridge_post_event(const event_t *ev) {
    if (ev == NULL) return;

    if (g_queue_count < EVENT_QUEUE_MAX) {
        g_event_queue[g_queue_head] = *ev;
        g_queue_head = (g_queue_head + 1) % EVENT_QUEUE_MAX;
        g_queue_count++;
    } else {
        /* Circular ring drop-oldest overwrite */
        g_event_queue[g_queue_head] = *ev;
        g_queue_head = (g_queue_head + 1) % EVENT_QUEUE_MAX;
        g_queue_tail = (g_queue_tail + 1) % EVENT_QUEUE_MAX;
    }

    if (g_post_event_cb != NULL) {
        g_post_event_cb(ev);
    }
}

int doom_bridge_get_event(event_t *ev) {
    if (g_queue_count <= 0 || ev == NULL) {
        return 0;
    }
    *ev = g_event_queue[g_queue_tail];
    g_queue_tail = (g_queue_tail + 1) % EVENT_QUEUE_MAX;
    g_queue_count--;
    return 1;
}

int doom_bridge_peek_event(int offset, event_t *ev) {
    if (offset < 0 || offset >= g_queue_count || ev == NULL) {
        return 0;
    }
    int idx = (g_queue_tail + offset) % EVENT_QUEUE_MAX;
    *ev = g_event_queue[idx];
    return 1;
}

int doom_bridge_get_queued_events_count(void) {
    return g_queue_count;
}

void doom_bridge_clear_events(void) {
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
}

void doom_bridge_set_post_event_callback(doom_post_event_cb_t cb) {
    g_post_event_cb = cb;
}

int doom_bridge_is_fire_active(void) {
    if (g_injected_fire) return 1;
    if (g_keypad_fire) return 1;
    touch_game_actions_t acts = touch_overlay_get_actions();
    if (acts.fire_pressed) return 1;
    return 0;
}

void doom_bridge_inject_fire(int active) {
    int was_active = doom_bridge_is_fire_active();
    g_injected_fire = active ? 1 : 0;
    int is_active = doom_bridge_is_fire_active();

    if (!was_active && is_active) {
        event_t ev = { ev_keydown, DOOM_KEY_FIRE, 0, 0 };
        doom_bridge_post_event(&ev);
    } else if (was_active && !is_active) {
        event_t ev = { ev_keyup, DOOM_KEY_FIRE, 0, 0 };
        doom_bridge_post_event(&ev);
    }
}

void doom_bridge_on_keypad_event(int key_index, int is_down) {
    if (key_index < 0 || key_index >= 16) return;

    int action_or_key = g_keypad_config.key_bindings[key_index];

    if (action_or_key == DOOM_ACTION_TOGGLE_HUD) {
        if (is_down) {
            touch_overlay_cycle_hud_visibility();
        }
        return;
    }

    int keycode = doom_bridge_action_to_keycode(action_or_key);
    if (keycode == 0) return;

    if (action_or_key == DOOM_ACTION_FIRE || keycode == DOOM_KEY_FIRE) {
        int was_fire = doom_bridge_is_fire_active();
        g_keypad_fire = is_down ? 1 : 0;
        int is_fire = doom_bridge_is_fire_active();

        if (!was_fire && is_fire) {
            event_t ev = { ev_keydown, DOOM_KEY_FIRE, 0, 0 };
            doom_bridge_post_event(&ev);
        } else if (was_fire && !is_fire) {
            event_t ev = { ev_keyup, DOOM_KEY_FIRE, 0, 0 };
            doom_bridge_post_event(&ev);
        }
        return;
    }

    event_t ev;
    ev.type = is_down ? ev_keydown : ev_keyup;
    ev.data1 = keycode;
    ev.data2 = 0;
    ev.data3 = 0;
    doom_bridge_post_event(&ev);
}

void doom_bridge_inject_keypad_event(int key_index, int is_down) {
    doom_bridge_on_keypad_event(key_index, is_down);
}

void doom_bridge_set_keypad_config(const keypad_config_t *kcfg) {
    if (kcfg != NULL) {
        g_keypad_config = *kcfg;
    }
}

const keypad_config_t *doom_bridge_get_keypad_config(void) {
    return &g_keypad_config;
}

void doom_bridge_set_touch_config(const touch_config_t *tcfg) {
    if (tcfg != NULL) {
        g_touch_config = *tcfg;
        touch_overlay_init(&g_touch_config, DOOM_DISPLAY_WIDTH, DOOM_DISPLAY_HEIGHT);
    }
}

const touch_config_t *doom_bridge_get_touch_config(void) {
    return &g_touch_config;
}

void doom_bridge_process_input(void) {
    /* 1. Poll SDL Events if active */
    if (!g_is_headless) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: {
                    event_t dev = { ev_keydown, DOOM_KEY_ESCAPE, 0, 0 };
                    doom_bridge_post_event(&dev);
                    break;
                }
                case SDL_FINGERDOWN:
                    touch_overlay_handle_down((int)ev.tfinger.fingerId, ev.tfinger.x, ev.tfinger.y);
                    break;
                case SDL_FINGERMOTION:
                    touch_overlay_handle_motion((int)ev.tfinger.fingerId, ev.tfinger.x, ev.tfinger.y);
                    break;
                case SDL_FINGERUP:
                    touch_overlay_handle_up((int)ev.tfinger.fingerId);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        float nx = (float)ev.button.x / (float)DOOM_DISPLAY_WIDTH;
                        float ny = (float)ev.button.y / (float)DOOM_DISPLAY_HEIGHT;
                        touch_overlay_handle_down(100, nx, ny);
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (ev.motion.state & SDL_BUTTON_LMASK) {
                        float nx = (float)ev.motion.x / (float)DOOM_DISPLAY_WIDTH;
                        float ny = (float)ev.motion.y / (float)DOOM_DISPLAY_HEIGHT;
                        touch_overlay_handle_motion(100, nx, ny);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        touch_overlay_handle_up(100);
                    }
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    int dkey = doom_bridge_sdl_key_to_doom_key(ev.key.keysym.sym);
                    if (dkey != 0) {
                        event_t dev;
                        dev.type = (ev.type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;
                        dev.data1 = dkey;
                        dev.data2 = 0;
                        dev.data3 = 0;
                        doom_bridge_post_event(&dev);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    /* 2. Read aggregated touch actions */
    touch_game_actions_t acts = touch_overlay_get_actions();

    /* Movement: Virtual stick forward/backward */
    int curr_fwd = (acts.move_forward > MOVE_THRESHOLD) ? 1 : 0;
    int curr_back = (acts.move_forward < -MOVE_THRESHOLD) ? 1 : 0;

    if (curr_fwd != g_prev_move_forward) {
        event_t ev = { curr_fwd ? ev_keydown : ev_keyup, DOOM_KEY_UP, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_move_forward = curr_fwd;
    }
    if (curr_back != g_prev_move_backward) {
        event_t ev = { curr_back ? ev_keydown : ev_keyup, DOOM_KEY_DOWN, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_move_backward = curr_back;
    }

    /* Movement: Virtual stick strafe right/left */
    int curr_strafe_r = (acts.strafe_right > MOVE_THRESHOLD) ? 1 : 0;
    int curr_strafe_l = (acts.strafe_right < -MOVE_THRESHOLD) ? 1 : 0;

    if (curr_strafe_r != g_prev_strafe_right) {
        event_t ev = { curr_strafe_r ? ev_keydown : ev_keyup, DOOM_KEY_STRAFE_R, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_strafe_right = curr_strafe_r;
    }
    if (curr_strafe_l != g_prev_strafe_left) {
        event_t ev = { curr_strafe_l ? ev_keydown : ev_keyup, DOOM_KEY_STRAFE_L, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_strafe_left = curr_strafe_l;
    }

    /* Touch Fire Button */
    int curr_touch_fire = acts.fire_pressed;
    if (curr_touch_fire != g_prev_touch_fire) {
        int was_fire = (g_injected_fire || g_keypad_fire || g_prev_touch_fire);
        int is_fire  = (g_injected_fire || g_keypad_fire || curr_touch_fire);
        if (!was_fire && is_fire) {
            event_t ev = { ev_keydown, DOOM_KEY_FIRE, 0, 0 };
            doom_bridge_post_event(&ev);
        } else if (was_fire && !is_fire) {
            event_t ev = { ev_keyup, DOOM_KEY_FIRE, 0, 0 };
            doom_bridge_post_event(&ev);
        }
        g_prev_touch_fire = curr_touch_fire;
    }

    /* Touch Use Button */
    if (acts.use_pressed != g_prev_touch_use) {
        event_t ev = { acts.use_pressed ? ev_keydown : ev_keyup, DOOM_KEY_USE, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_touch_use = acts.use_pressed;
    }

    /* Touch Menu / ESC */
    if (acts.menu_pressed != g_prev_touch_menu) {
        event_t ev = { acts.menu_pressed ? ev_keydown : ev_keyup, DOOM_KEY_ESCAPE, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_touch_menu = acts.menu_pressed;
    }

    /* Touch Automap / Tab */
    if (acts.automap_pressed != g_prev_touch_automap) {
        event_t ev = { acts.automap_pressed ? ev_keydown : ev_keyup, DOOM_KEY_TAB, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_touch_automap = acts.automap_pressed;
    }

    /* Touch HUD Toggle */
    if (acts.toggle_hud && !g_prev_touch_toggle_hud) {
        touch_overlay_cycle_hud_visibility();
    }
    g_prev_touch_toggle_hud = acts.toggle_hud;

    /* Next / Prev Weapon */
    if (acts.next_weapon != g_prev_touch_next_weapon) {
        event_t ev = { acts.next_weapon ? ev_keydown : ev_keyup, DOOM_KEY_NEXT_WEAPON, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_touch_next_weapon = acts.next_weapon;
    }
    if (acts.prev_weapon != g_prev_touch_prev_weapon) {
        event_t ev = { acts.prev_weapon ? ev_keydown : ev_keyup, DOOM_KEY_PREV_WEAPON, 0, 0 };
        doom_bridge_post_event(&ev);
        g_prev_touch_prev_weapon = acts.prev_weapon;
    }

    /* Look / Turn Impulse */
    if (fabsf(acts.turn_speed) > 0.005f) {
        event_t ev;
        ev.type = ev_mouse;
        ev.data1 = doom_bridge_is_fire_active() ? 1 : 0;
        ev.data2 = (int)(acts.turn_speed * 40.0f);
        if (ev.data2 == 0) {
            ev.data2 = (acts.turn_speed > 0.0f) ? 1 : -1;
        }
        ev.data3 = 0;
        doom_bridge_post_event(&ev);
    }

    /* CRITICAL NOTE: consume as an instantaneous per-frame impulse,
       clearing it after each doom_bridge_process_input() tick so player doesn't spin forever! */
    touch_overlay_clear_turn_impulse();
}

void doom_bridge_render_present(SDL_Texture *game_framebuffer) {
    if (g_renderer == NULL) {
        return;
    }

    /* 1. Clear background to black */
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    /* 2. Scale game framebuffer to 480x320 native resolution */
    if (game_framebuffer != NULL) {
        SDL_Rect dst_rect;
        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.w = DOOM_DISPLAY_WIDTH;
        dst_rect.h = DOOM_DISPLAY_HEIGHT;
        SDL_RenderCopy(g_renderer, game_framebuffer, NULL, &dst_rect);
    }

    /* 3. Render alpha-blended touch HUD overlay on top of game frame */
    touch_overlay_render(g_renderer);

    /* 4. Present final rendered frame to display */
    SDL_RenderPresent(g_renderer);
}
