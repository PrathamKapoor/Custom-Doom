#ifndef TOUCH_OVERLAY_H
#define TOUCH_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config_manager.h"

#if defined(__has_include)
  #if __has_include(<SDL2/SDL.h>)
    #include <SDL2/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #endif
#endif

#if !defined(_SDL_H) && !defined(SDL_h_)
typedef struct SDL_Renderer SDL_Renderer;

typedef struct SDL_Rect {
    int x, y;
    int w, h;
} SDL_Rect;

typedef enum {
    SDL_BLENDMODE_NONE = 0x00000000,
    SDL_BLENDMODE_BLEND = 0x00000001,
    SDL_BLENDMODE_ADD = 0x00000002,
    SDL_BLENDMODE_MOD = 0x00000004,
    SDL_BLENDMODE_MUL = 0x00000008,
    SDL_BLENDMODE_INVALID = 0x7FFFFFFF
} SDL_BlendMode;

int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);
int SDL_SetRenderDrawColor(SDL_Renderer *renderer, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect);
int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect);
int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2);
int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y);
#endif

#define TOUCH_STICK_MAX_RADIUS 60.0f
#define MAX_TOUCH_FINGERS 10

typedef enum {
    TOUCH_ZONE_NONE = 0,
    TOUCH_ZONE_STICK,
    TOUCH_ZONE_FIRE,
    TOUCH_ZONE_USE,
    TOUCH_ZONE_PREV_WEAPON,
    TOUCH_ZONE_NEXT_WEAPON,
    TOUCH_ZONE_MENU,
    TOUCH_ZONE_AUTOMAP,
    TOUCH_ZONE_TOGGLE_HUD,
    TOUCH_ZONE_LOOK
} touch_zone_t;

typedef struct {
    float move_forward;  // -1.0 to 1.0 (positive = forward, negative = backward)
    float strafe_right;  // -1.0 to 1.0 (positive = right, negative = left)
    float turn_speed;    // -1.0 to 1.0 (positive = turn right, negative = turn left)
    int fire_pressed;    // 1 when Fire zone is held down
    int use_pressed;     // 1 when Use zone is held down
    int next_weapon;     // 1 when Next Weapon zone is held down
    int prev_weapon;     // 1 when Prev Weapon zone is held down
    int menu_pressed;    // 1 when Menu / ESC zone is held down
    int automap_pressed; // 1 when Automap zone is held down
    int toggle_hud;      // 1 when HUD toggle zone is held down
} touch_game_actions_t;

/**
 * @brief Initialize the touch overlay subsystem with the provided config and screen size.
 */
void touch_overlay_init(const touch_config_t *config, int screen_w, int screen_h);

/**
 * @brief Process a touch finger press event.
 * @param finger_id Unique finger/touch identifier.
 * @param norm_x Normalized X coordinate [0.0, 1.0].
 * @param norm_y Normalized Y coordinate [0.0, 1.0].
 */
void touch_overlay_handle_down(int finger_id, float norm_x, float norm_y);

/**
 * @brief Process a touch finger motion/drag event.
 * @param finger_id Unique finger/touch identifier.
 * @param norm_x Normalized X coordinate [0.0, 1.0].
 * @param norm_y Normalized Y coordinate [0.0, 1.0].
 */
void touch_overlay_handle_motion(int finger_id, float norm_x, float norm_y);

/**
 * @brief Process a touch finger release event.
 * @param finger_id Unique finger/touch identifier.
 */
void touch_overlay_handle_up(int finger_id);

/**
 * @brief Retrieve current aggregated game actions from active touches.
 */
touch_game_actions_t touch_overlay_get_actions(void);

/**
 * @brief Transform raw 12-bit ADC coordinates (0-4095) into normalized screen coordinates (0.0-1.0)
 *        applying the configured screen rotation (0, 90, 180, 270).
 */
void touch_overlay_transform_raw(int raw_x, int raw_y, float *norm_x, float *norm_y);

/**
 * @brief Apply single-pole low-pass IIR jitter filter (pos = 0.7 * raw + 0.3 * prev).
 */
void touch_overlay_apply_iir_filter(float raw_x, float raw_y, float prev_x, float prev_y, float *out_x, float *out_y);

/**
 * @brief Set the touch HUD opacity.
 */
void touch_overlay_set_opacity(float opacity);

/**
 * @brief Get the current touch HUD opacity.
 */
float touch_overlay_get_opacity(void);

/**
 * @brief Cycle HUD visibility between Full (0.75), Dim (0.35), and Hidden (0.0).
 */
void touch_overlay_cycle_hud_visibility(void);

/**
 * @brief Query current floating stick status and pixel positions (for HUD rendering).
 * @return 1 if stick is active, 0 otherwise.
 */
int touch_overlay_get_stick_state(int *active, float *anchor_x, float *anchor_y, float *curr_x, float *curr_y);

/**
 * @brief Render touch controls overlay onto SDL2 renderer.
 */
void touch_overlay_render(SDL_Renderer *renderer);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_OVERLAY_H */
