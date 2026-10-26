#ifndef DOOM_BRIDGE_H
#define DOOM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "../hardware/config_manager.h"
#include "../hardware/touch_overlay.h"
#include "../hardware/matrix_keypad.h"

#if defined(__has_include)
  #if __has_include(<SDL2/SDL.h>)
    #include <SDL2/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #endif
#endif

#if !defined(_SDL_H) && !defined(SDL_h_)
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

typedef struct SDL_TouchFingerEvent {
    uint32_t type;
    uint32_t timestamp;
    int64_t touchId;
    int64_t fingerId;
    float x;
    float y;
    float dx;
    float dy;
    float pressure;
    uint32_t windowID;
} SDL_TouchFingerEvent;

typedef struct SDL_MouseButtonEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    uint8_t button;
    uint8_t state;
    uint8_t clicks;
    uint8_t padding1;
    int32_t x;
    int32_t y;
} SDL_MouseButtonEvent;

typedef struct SDL_MouseMotionEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    uint32_t state;
    int32_t x;
    int32_t y;
    int32_t xrel;
    int32_t yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_Keysym {
    int32_t scancode;
    int32_t sym;
    uint16_t mod;
    uint32_t unused;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint8_t state;
    uint8_t repeat;
    uint8_t padding2;
    uint8_t padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_QuitEvent {
    uint32_t type;
    uint32_t timestamp;
} SDL_QuitEvent;

typedef union SDL_Event {
    uint32_t type;
    SDL_QuitEvent quit;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_TouchFingerEvent tfinger;
    uint8_t padding[56];
} SDL_Event;

#define SDL_QUIT            0x100
#define SDL_KEYDOWN         0x300
#define SDL_KEYUP           0x301
#define SDL_MOUSEMOTION     0x400
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP   0x402
#define SDL_FINGERDOWN      0x700
#define SDL_FINGERUP        0x701
#define SDL_FINGERMOTION    0x702

#define SDL_BUTTON_LEFT     1
#define SDL_BUTTON_LMASK    (1 << (SDL_BUTTON_LEFT - 1))

int SDL_PollEvent(SDL_Event *event);
int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect);
void SDL_RenderPresent(SDL_Renderer *renderer);
int SDL_RenderClear(SDL_Renderer *renderer);
#endif

/* Native Screen & Texture Dimensions */
#define DOOM_DISPLAY_WIDTH  480
#define DOOM_DISPLAY_HEIGHT 320
#define DOOM_GAME_WIDTH     320
#define DOOM_GAME_HEIGHT    200

/* Doom Event Architecture */
typedef enum {
    ev_keydown = 0,
    ev_keyup   = 1,
    ev_mouse   = 2,
    ev_joystick= 3
} evtype_t;

typedef struct {
    evtype_t type;
    int data1;   /* Keycode / Mouse button flags */
    int data2;   /* Mouse X movement (turn impulse) or Joystick X */
    int data3;   /* Mouse Y movement or Joystick Y */
} event_t;

/* Standard Doom Key Definitions */
#define DOOM_KEY_RIGHT        0xae
#define DOOM_KEY_LEFT         0xac
#define DOOM_KEY_UP           0xad
#define DOOM_KEY_DOWN         0xaf
#define DOOM_KEY_ESCAPE       27
#define DOOM_KEY_ENTER        13
#define DOOM_KEY_TAB          9
#define DOOM_KEY_F11          (0x80 + 0x57)
#define DOOM_KEY_BACKSPACE    127
#define DOOM_KEY_PAUSE        0xff

#define DOOM_KEY_RSHIFT       (0x80 + 0x36)
#define DOOM_KEY_RCTRL        (0x80 + 0x1d)
#define DOOM_KEY_RALT         (0x80 + 0x38)

#define DOOM_KEY_FIRE         DOOM_KEY_RCTRL
#define DOOM_KEY_USE          ' '
#define DOOM_KEY_STRAFE_L     ','
#define DOOM_KEY_STRAFE_R     '.'
#define DOOM_KEY_SPEED        DOOM_KEY_RSHIFT
#define DOOM_KEY_GAMMA        DOOM_KEY_F11
#define DOOM_KEY_PREV_WEAPON  '['
#define DOOM_KEY_NEXT_WEAPON  ']'

/* Mapped Doom Actions (indices 0..15 corresponding to default 4x4 matrix) */
typedef enum {
    DOOM_ACTION_STRAFE_LEFT   = 0,  /* Key 0:  [1] */
    DOOM_ACTION_MOVE_FORWARD  = 1,  /* Key 1:  [2] */
    DOOM_ACTION_STRAFE_RIGHT  = 2,  /* Key 2:  [3] */
    DOOM_ACTION_FIRE          = 3,  /* Key 3:  [A] */
    DOOM_ACTION_TURN_LEFT     = 4,  /* Key 4:  [4] */
    DOOM_ACTION_MOVE_BACKWARD = 5,  /* Key 5:  [5] */
    DOOM_ACTION_TURN_RIGHT    = 6,  /* Key 6:  [6] */
    DOOM_ACTION_USE           = 7,  /* Key 7:  [B] */
    DOOM_ACTION_PREV_WEAPON   = 8,  /* Key 8:  [7] */
    DOOM_ACTION_SPEED         = 9,  /* Key 9:  [8] */
    DOOM_ACTION_NEXT_WEAPON   = 10, /* Key 10: [9] */
    DOOM_ACTION_AUTOMAP       = 11, /* Key 11: [C] */
    DOOM_ACTION_MENU          = 12, /* Key 12: [*] */
    DOOM_ACTION_ENTER         = 13, /* Key 13: [0] */
    DOOM_ACTION_GAMMA         = 14, /* Key 14: [#] */
    DOOM_ACTION_TOGGLE_HUD    = 15, /* Key 15: [D] */
    DOOM_ACTION_COUNT         = 16
} doom_action_t;

/* Optional callback hook for direct Doom engine event dispatching */
typedef void (*doom_post_event_cb_t)(const event_t *ev);

/**
 * @brief Initialize Doom event bridge and display scaler with active SDL window and renderer.
 */
void doom_bridge_init(SDL_Window *window, SDL_Renderer *renderer);

/**
 * @brief Initialize Doom event bridge in headless mode (no SDL window/renderer) for testing.
 */
void doom_bridge_init_headless(void);

/**
 * @brief Cleanly shut down Doom event bridge and stop background keypad scanning thread.
 */
void doom_bridge_shutdown(void);

/**
 * @brief Pump SDL events, query touch/keypad states, and generate Doom event queue entries.
 *        CRITICAL: Consumes turn_speed as an instantaneous impulse and clears it per frame.
 */
void doom_bridge_process_input(void);

/**
 * @brief Scale 320x200 game frame texture to 480x320 native screen, draw touch HUD, and present.
 */
void doom_bridge_render_present(SDL_Texture *game_framebuffer);

/* Event Queue API */
void doom_bridge_post_event(const event_t *ev);
int  doom_bridge_get_event(event_t *ev);
int  doom_bridge_peek_event(int offset, event_t *ev);
int  doom_bridge_get_queued_events_count(void);
void doom_bridge_clear_events(void);
void doom_bridge_set_post_event_callback(doom_post_event_cb_t cb);

/* Test & Injection Helpers */
void doom_bridge_inject_fire(int active);
int  doom_bridge_is_fire_active(void);
void doom_bridge_inject_keypad_event(int key_index, int is_down);
void doom_bridge_on_keypad_event(int key_index, int is_down);

/* Configuration Management */
void doom_bridge_set_keypad_config(const keypad_config_t *kcfg);
const keypad_config_t *doom_bridge_get_keypad_config(void);
void doom_bridge_set_touch_config(const touch_config_t *tcfg);
const touch_config_t *doom_bridge_get_touch_config(void);

/* Translation Helpers */
int  doom_bridge_action_to_keycode(int action_or_key);
int  doom_bridge_sdl_key_to_doom_key(int sdl_sym);

#ifdef __cplusplus
}
#endif

#endif /* DOOM_BRIDGE_H */
