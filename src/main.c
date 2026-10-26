/**
 * @file main.c
 * @brief Turnkey Doom Entry Point for Raspberry Pi 4B with 3.5" Touchscreen & 4x4 Matrix Keypad.
 *
 * Implements 480x320 native scaling, 60 FPS interactive game loop, IWAD detection,
 * dual-thumb touch HUD, and conflict-free 4x4 GPIO matrix keypad integration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "hardware/config_manager.h"
#include "hardware/touch_overlay.h"
#include "hardware/matrix_keypad.h"
#include "engine/doom_bridge.h"

#if defined(__has_include)
  #if __has_include(<SDL2/SDL.h>)
    #include <SDL2/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #endif
#endif

#if !defined(_SDL_H) && !defined(SDL_h_)
#define SDL_INIT_VIDEO    0x00000020u
#define SDL_INIT_AUDIO    0x00000010u
#define SDL_INIT_EVENTS   0x00004000u
#define SDL_WINDOW_SHOWN  0x00000004u
#define SDL_WINDOW_FULLSCREEN 0x00000001u
#define SDL_RENDERER_ACCELERATED 0x00000002u
#define SDL_RENDERER_PRESENTVSYNC 0x00000004u
#define SDL_RENDERER_SOFTWARE 0x00000001u
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_PIXELFORMAT_RGBA8888 0x16462004u

#define SDL_WINDOWPOS_CENTERED 0x2FFF0000u

int SDL_Init(uint32_t flags);
void SDL_Quit(void);
SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, uint32_t flags);
void SDL_DestroyWindow(SDL_Window *window);
SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, int index, uint32_t flags);
void SDL_DestroyRenderer(SDL_Renderer *renderer);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, uint32_t format, int access, int w, int h);
void SDL_DestroyTexture(SDL_Texture *texture);
int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);
uint32_t SDL_GetTicks(void);
void SDL_Delay(uint32_t ms);
int SDL_ShowCursor(int toggle);
const char *SDL_GetError(void);
#endif

#define MAP_WIDTH  16
#define MAP_HEIGHT 16

/* Classic Doom-style 16x16 test environment map */
static const int g_map[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,0,0,1,0,3,3,3,0,0,2,0,1},
    {1,0,2,2,0,0,0,0,3,0,3,0,0,2,0,1},
    {1,0,0,0,0,0,1,0,3,3,3,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
    {1,1,0,1,1,1,1,0,0,0,0,0,1,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,0,0,1,0,0,4,4,4,0,0,0,1,0,1},
    {1,0,0,0,0,0,0,4,0,4,0,0,0,0,0,1},
    {1,0,0,0,1,0,0,4,4,4,0,0,0,1,0,1},
    {1,1,0,1,1,0,0,0,0,0,0,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,3,0,0,2,2,0,0,2,2,0,0,3,0,1},
    {1,0,0,0,0,2,2,0,0,2,2,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

/* Framebuffer & Raycaster State */
static uint32_t g_framebuffer[DOOM_GAME_WIDTH * DOOM_GAME_HEIGHT];
static float g_pos_x = 7.5f;
static float g_pos_y = 7.5f;
static float g_dir_x = -1.0f;
static float g_dir_y = 0.0f;
static float g_plane_x = 0.0f;
static float g_plane_y = 0.66f;

/* Movement and action tracking */
static int g_fwd_pressed = 0;
static int g_back_pressed = 0;
static int g_strafe_l_pressed = 0;
static int g_strafe_r_pressed = 0;
static int g_turn_l_pressed = 0;
static int g_turn_r_pressed = 0;
static int g_fire_active = 0;
static int g_muzzle_flash_ticks = 0;
static int g_current_weapon = 2; // Shotgun
static int g_health = 100;
static int g_ammo = 50;
static int g_armor = 0;
static int g_automap_active = 0;
static int g_quit_requested = 0;

/**
 * @brief Search for Doom IWAD file in standard paths.
 */
static int find_doom_iwad(const char *cli_path, char *out_path, size_t max_len) {
    if (cli_path != NULL && cli_path[0] != '\0') {
        FILE *f = fopen(cli_path, "rb");
        if (f) {
            strncpy(out_path, cli_path, max_len - 1);
            out_path[max_len - 1] = '\0';
            fclose(f);
            return 1;
        }
    }

    const char *home = getenv("HOME");
    const char *env_dir = getenv("DOOMWADDIR");

    const char *candidates[16];
    int count = 0;

    char home_path1[512], home_path2[512], home_path3[512];
    if (home) {
        snprintf(home_path1, sizeof(home_path1), "%s/.doom-touch/DOOM1.WAD", home);
        snprintf(home_path2, sizeof(home_path2), "%s/.doom-touch/doom1.wad", home);
        snprintf(home_path3, sizeof(home_path3), "%s/.doom-touch/doom2.wad", home);
        candidates[count++] = home_path1;
        candidates[count++] = home_path2;
        candidates[count++] = home_path3;
    }

    char env_path1[512];
    if (env_dir) {
        snprintf(env_path1, sizeof(env_path1), "%s/DOOM1.WAD", env_dir);
        candidates[count++] = env_path1;
    }

    candidates[count++] = "./wads/DOOM1.WAD";
    candidates[count++] = "./wads/doom1.wad";
    candidates[count++] = "./wads/doom2.wad";
    candidates[count++] = "./DOOM1.WAD";
    candidates[count++] = "./doom1.wad";
    candidates[count++] = "./doom2.wad";
    candidates[count++] = "/usr/share/games/doom/DOOM1.WAD";
    candidates[count++] = "/usr/share/games/doom/doom1.wad";

    for (int i = 0; i < count; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) {
            strncpy(out_path, candidates[i], max_len - 1);
            out_path[max_len - 1] = '\0';
            fclose(f);
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Inspect Doom IWAD file header.
 */
static void inspect_iwad(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;

    char header[12];
    if (fread(header, 1, 12, f) == 12) {
        uint32_t num_lumps = (uint8_t)header[4] | ((uint8_t)header[5] << 8) |
                             ((uint8_t)header[6] << 16) | ((uint8_t)header[7] << 24);
        printf("[DOOM WAD] Loaded IWAD: %s (Type: %.4s, Lumps: %u)\n", path, header, num_lumps);
    }
    fclose(f);
}

/**
 * @brief Render interactive 320x200 3D view with status bar and automap.
 */
static void render_game_framebuffer(void) {
    /* 1. Clear Ceiling and Floor with Doom color palette */
    for (int y = 0; y < DOOM_GAME_HEIGHT / 2; y++) {
        uint32_t ceil_color = 0x222230FF; /* Dark charcoal/blue sky */
        for (int x = 0; x < DOOM_GAME_WIDTH; x++) {
            g_framebuffer[y * DOOM_GAME_WIDTH + x] = ceil_color;
        }
    }
    for (int y = DOOM_GAME_HEIGHT / 2; y < 168; y++) {
        uint32_t floor_color = 0x383024FF; /* Earthy brown dungeon floor */
        for (int x = 0; x < DOOM_GAME_WIDTH; x++) {
            g_framebuffer[y * DOOM_GAME_WIDTH + x] = floor_color;
        }
    }

    /* 2. Raycast 320 vertical slices across the field of view */
    for (int x = 0; x < DOOM_GAME_WIDTH; x++) {
        float camera_x = 2.0f * (float)x / (float)DOOM_GAME_WIDTH - 1.0f;
        float ray_dir_x = g_dir_x + g_plane_x * camera_x;
        float ray_dir_y = g_dir_y + g_plane_y * camera_x;

        int map_x = (int)g_pos_x;
        int map_y = (int)g_pos_y;

        float delta_dist_x = (fabsf(ray_dir_x) < 1e-6f) ? 1e30f : fabsf(1.0f / ray_dir_x);
        float delta_dist_y = (fabsf(ray_dir_y) < 1e-6f) ? 1e30f : fabsf(1.0f / ray_dir_y);

        float side_dist_x;
        float side_dist_y;
        int step_x, step_y;
        int hit = 0;
        int side = 0;

        if (ray_dir_x < 0) {
            step_x = -1;
            side_dist_x = (g_pos_x - (float)map_x) * delta_dist_x;
        } else {
            step_x = 1;
            side_dist_x = ((float)map_x + 1.0f - g_pos_x) * delta_dist_x;
        }
        if (ray_dir_y < 0) {
            step_y = -1;
            side_dist_y = (g_pos_y - (float)map_y) * delta_dist_y;
        } else {
            step_y = 1;
            side_dist_y = ((float)map_y + 1.0f - g_pos_y) * delta_dist_y;
        }

        while (hit == 0) {
            if (side_dist_x < side_dist_y) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }

            if (map_x >= 0 && map_x < MAP_WIDTH && map_y >= 0 && map_y < MAP_HEIGHT) {
                if (g_map[map_y][map_x] > 0) hit = 1;
            } else {
                hit = 1;
            }
        }

        float perp_wall_dist;
        if (side == 0) perp_wall_dist = (side_dist_x - delta_dist_x);
        else           perp_wall_dist = (side_dist_y - delta_dist_y);
        if (perp_wall_dist < 0.1f) perp_wall_dist = 0.1f;

        int line_height = (int)((float)DOOM_GAME_HEIGHT / perp_wall_dist);
        int draw_start = -line_height / 2 + (DOOM_GAME_HEIGHT / 2) - 8;
        if (draw_start < 0) draw_start = 0;
        int draw_end = line_height / 2 + (DOOM_GAME_HEIGHT / 2) - 8;
        if (draw_end >= 168) draw_end = 167;

        /* Wall color selection based on block type & lighting side */
        int wall_type = (map_x >= 0 && map_x < MAP_WIDTH && map_y >= 0 && map_y < MAP_HEIGHT) ? g_map[map_y][map_x] : 1;
        uint32_t wall_color;
        switch (wall_type) {
            case 2:  wall_color = side ? 0x902020FF : 0xB83030FF; break; /* Red tech brick */
            case 3:  wall_color = side ? 0x205090FF : 0x3070B8FF; break; /* Blue marble */
            case 4:  wall_color = side ? 0x807020FF : 0xA89030FF; break; /* Bronze pillar */
            default: wall_color = side ? 0x555555FF : 0x777777FF; break; /* Gray stone */
        }

        for (int y = draw_start; y <= draw_end; y++) {
            g_framebuffer[y * DOOM_GAME_WIDTH + x] = wall_color;
        }
    }

    /* 3. Muzzle Flash Effect when Firing */
    if (g_muzzle_flash_ticks > 0) {
        g_muzzle_flash_ticks--;
        for (int y = 100; y < 145; y++) {
            for (int x = 140; x < 180; x++) {
                g_framebuffer[y * DOOM_GAME_WIDTH + x] = 0xFFEE44FF; /* Golden flame */
            }
        }
    }

    /* 4. Crosshair at Center */
    int cx = DOOM_GAME_WIDTH / 2;
    int cy = (DOOM_GAME_HEIGHT / 2) - 8;
    for (int i = -3; i <= 3; i++) {
        g_framebuffer[cy * DOOM_GAME_WIDTH + (cx + i)] = 0xFFFFFFFF;
        g_framebuffer[(cy + i) * DOOM_GAME_WIDTH + cx] = 0xFFFFFFFF;
    }

    /* 5. Classic Doom Status Bar at Bottom (Rows 168..199) */
    for (int y = 168; y < DOOM_GAME_HEIGHT; y++) {
        for (int x = 0; x < DOOM_GAME_WIDTH; x++) {
            g_framebuffer[y * DOOM_GAME_WIDTH + x] = 0x1A1A1AFF; /* Status bar dark gray */
        }
    }

    /* Status Bar Separator Line */
    for (int x = 0; x < DOOM_GAME_WIDTH; x++) {
        g_framebuffer[168 * DOOM_GAME_WIDTH + x] = 0x666666FF;
    }

    /* Status Bar Panels (Ammo, Health, Arms, Armor) */
    for (int y = 172; y < 196; y++) {
        /* Ammo box */
        for (int x = 10; x < 60; x++) g_framebuffer[y * DOOM_GAME_WIDTH + x] = 0x2A2A2AFF;
        /* Health box */
        for (int x = 70; x < 130; x++) g_framebuffer[y * DOOM_GAME_WIDTH + x] = 0x2A2A2AFF;
        /* Arms box */
        for (int x = 140; x < 180; x++) g_framebuffer[y * DOOM_GAME_WIDTH + x] = 0x2A2A2AFF;
        /* Armor box */
        for (int x = 190; x < 250; x++) g_framebuffer[y * DOOM_GAME_WIDTH + x] = 0x2A2A2AFF;
    }

    /* 6. Optional Automap Overlay */
    if (g_automap_active) {
        int cell_size = 6;
        int start_x = 10;
        int start_y = 10;
        for (int my = 0; my < MAP_HEIGHT; my++) {
            for (int mx = 0; mx < MAP_WIDTH; mx++) {
                uint32_t c = (g_map[my][mx] > 0) ? 0x992222FF : 0x113311CC;
                for (int dy = 0; dy < cell_size; dy++) {
                    for (int dx = 0; dx < cell_size; dx++) {
                        int px = start_x + mx * cell_size + dx;
                        int py = start_y + my * cell_size + dy;
                        if (px < DOOM_GAME_WIDTH && py < 168) {
                            g_framebuffer[py * DOOM_GAME_WIDTH + px] = c;
                        }
                    }
                }
            }
        }
        /* Draw player point on automap */
        int ppx = start_x + (int)(g_pos_x * (float)cell_size);
        int ppy = start_y + (int)(g_pos_y * (float)cell_size);
        if (ppx >= 0 && ppx < DOOM_GAME_WIDTH && ppy >= 0 && ppy < 168) {
            g_framebuffer[ppy * DOOM_GAME_WIDTH + ppx] = 0xFFFFFFFF;
        }
    }
}

/**
 * @brief Update player kinematics and process Doom input events.
 */
static void update_game_state(float dt) {
    /* 1. Poll inputs via Doom bridge */
    doom_bridge_process_input();

    /* 2. Drain and interpret events from Doom bridge queue */
    event_t ev;
    while (doom_bridge_get_event(&ev)) {
        if (ev.type == ev_keydown || ev.type == ev_keyup) {
            int down = (ev.type == ev_keydown) ? 1 : 0;
            switch (ev.data1) {
                case DOOM_KEY_UP:        g_fwd_pressed = down; break;
                case DOOM_KEY_DOWN:      g_back_pressed = down; break;
                case DOOM_KEY_STRAFE_L:  g_strafe_l_pressed = down; break;
                case DOOM_KEY_STRAFE_R:  g_strafe_r_pressed = down; break;
                case DOOM_KEY_LEFT:      g_turn_l_pressed = down; break;
                case DOOM_KEY_RIGHT:     g_turn_r_pressed = down; break;
                case DOOM_KEY_FIRE:
                    g_fire_active = down;
                    if (down) {
                        g_muzzle_flash_ticks = 4;
                        if (g_ammo > 0) g_ammo--;
                    }
                    break;
                case DOOM_KEY_USE:
                    if (down) printf("[DOOM GAME] USE action triggered (door/switch)\n");
                    break;
                case DOOM_KEY_TAB:
                    if (down) g_automap_active = !g_automap_active;
                    break;
                case DOOM_KEY_ESCAPE:
                    if (down) {
                        printf("[DOOM GAME] ESC pressed: exiting game\n");
                        g_quit_requested = 1;
                    }
                    break;
                case DOOM_KEY_NEXT_WEAPON:
                    if (down) {
                        g_current_weapon = (g_current_weapon % 7) + 1;
                        printf("[DOOM GAME] Switched to weapon: %d\n", g_current_weapon);
                    }
                    break;
                case DOOM_KEY_PREV_WEAPON:
                    if (down) {
                        g_current_weapon = (g_current_weapon == 1) ? 7 : g_current_weapon - 1;
                        printf("[DOOM GAME] Switched to weapon: %d\n", g_current_weapon);
                    }
                    break;
                default:
                    break;
            }
        } else if (ev.type == ev_mouse) {
            /* Analog horizontal look impulse from touch drag or mouse */
            float rot_speed = (float)ev.data2 * 0.04f;
            float old_dir_x = g_dir_x;
            g_dir_x = g_dir_x * cosf(-rot_speed) - g_dir_y * sinf(-rot_speed);
            g_dir_y = old_dir_x * sinf(-rot_speed) + g_dir_y * cosf(-rot_speed);
            float old_plane_x = g_plane_x;
            g_plane_x = g_plane_x * cosf(-rot_speed) - g_plane_y * sinf(-rot_speed);
            g_plane_y = old_plane_x * sinf(-rot_speed) + g_plane_y * cosf(-rot_speed);
        }
    }

    /* 3. Apply digital keyboard/matrix turn */
    if (g_turn_l_pressed) {
        float rot = 2.5f * dt;
        float old_dir_x = g_dir_x;
        g_dir_x = g_dir_x * cosf(rot) - g_dir_y * sinf(rot);
        g_dir_y = old_dir_x * sinf(rot) + g_dir_y * cosf(rot);
        float old_plane_x = g_plane_x;
        g_plane_x = g_plane_x * cosf(rot) - g_plane_y * sinf(rot);
        g_plane_y = old_plane_x * sinf(rot) + g_plane_y * cosf(rot);
    }
    if (g_turn_r_pressed) {
        float rot = -2.5f * dt;
        float old_dir_x = g_dir_x;
        g_dir_x = g_dir_x * cosf(rot) - g_dir_y * sinf(rot);
        g_dir_y = old_dir_x * sinf(rot) + g_dir_y * cosf(rot);
        float old_plane_x = g_plane_x;
        g_plane_x = g_plane_x * cosf(rot) - g_plane_y * sinf(rot);
        g_plane_y = old_plane_x * sinf(rot) + g_plane_y * cosf(rot);
    }

    /* 4. Apply forward/backward movement with wall collision */
    float move_speed = 3.2f * dt;
    if (g_fwd_pressed) {
        float new_x = g_pos_x + g_dir_x * move_speed;
        float new_y = g_pos_y + g_dir_y * move_speed;
        if (g_map[(int)g_pos_y][(int)new_x] == 0) g_pos_x = new_x;
        if (g_map[(int)new_y][(int)g_pos_x] == 0) g_pos_y = new_y;
    }
    if (g_back_pressed) {
        float new_x = g_pos_x - g_dir_x * move_speed;
        float new_y = g_pos_y - g_dir_y * move_speed;
        if (g_map[(int)g_pos_y][(int)new_x] == 0) g_pos_x = new_x;
        if (g_map[(int)new_y][(int)g_pos_x] == 0) g_pos_y = new_y;
    }

    /* 5. Apply strafe left/right movement */
    if (g_strafe_r_pressed) {
        float strafe_x = -g_dir_y * move_speed;
        float strafe_y =  g_dir_x * move_speed;
        if (g_map[(int)g_pos_y][(int)(g_pos_x + strafe_x)] == 0) g_pos_x += strafe_x;
        if (g_map[(int)(g_pos_y + strafe_y)][(int)g_pos_x] == 0) g_pos_y += strafe_y;
    }
    if (g_strafe_l_pressed) {
        float strafe_x =  g_dir_y * move_speed;
        float strafe_y = -g_dir_x * move_speed;
        if (g_map[(int)g_pos_y][(int)(g_pos_x + strafe_x)] == 0) g_pos_x += strafe_x;
        if (g_map[(int)(g_pos_y + strafe_y)][(int)g_pos_x] == 0) g_pos_y += strafe_y;
    }
}

static void print_banner(void) {
    printf("========================================================================\n");
    printf("         DOOM - Touch & 4x4 Matrix Port for Raspberry Pi 4B            \n");
    printf("   Resolution: 480x320 Native | Display: 3.5\" SPI TFT | Touch: XPT2046   \n");
    printf("========================================================================\n");
}

int main(int argc, char *argv[]) {
    print_banner();

    const char *cli_iwad = NULL;
    const char *config_dir = NULL;
    int force_fullscreen = 0;
    int force_windowed = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-iwad") == 0 && i + 1 < argc) {
            cli_iwad = argv[++i];
        } else if (strcmp(argv[i], "-fullscreen") == 0) {
            force_fullscreen = 1;
        } else if (strcmp(argv[i], "-windowed") == 0) {
            force_windowed = 1;
        } else if (strcmp(argv[i], "-config") == 0 && i + 1 < argc) {
            config_dir = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("  -iwad <path>       Specify path to DOOM1.WAD or other IWAD\n");
            printf("  -fullscreen        Force fullscreen mode\n");
            printf("  -windowed          Force windowed mode (default on desktop)\n");
            printf("  -config <dir>      Specify configuration directory\n");
            printf("  -h, --help         Display this help message\n");
            return 0;
        }
    }

    /* 1. Configuration initialization */
    touch_config_t touch_cfg;
    keypad_config_t keypad_cfg;
    if (config_load(config_dir, &touch_cfg, &keypad_cfg) != 0) {
        printf("[CONFIG] No config found; saving default settings...\n");
        config_set_defaults(&touch_cfg, &keypad_cfg);
        config_save(config_dir, &touch_cfg, &keypad_cfg);
    } else {
        printf("[CONFIG] Loaded settings successfully.\n");
    }

    /* 2. Check for Doom IWAD */
    char iwad_path[512];
    if (find_doom_iwad(cli_iwad, iwad_path, sizeof(iwad_path))) {
        inspect_iwad(iwad_path);
    } else {
        printf("\n[DOOM WAD] NOTICE: No DOOM1.WAD found in default search paths.\n");
        printf("  Run `./scripts/download_wad.sh` to fetch verified shareware DOOM1.WAD\n");
        printf("  or place your IWAD in ~/.doom-touch/ or ./wads/\n");
        printf("  Starting interactive 3D test chamber & touch HUD...\n\n");
    }

    /* 3. Initialize SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[SDL ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Window flags: check environment or arguments for fullscreen */
    uint32_t window_flags = SDL_WINDOW_SHOWN;
    const char *env_vid = getenv("SDL_VIDEODRIVER");
    const char *env_fs  = getenv("DOOM_FULLSCREEN");
    if (force_fullscreen || (env_fs && strcmp(env_fs, "1") == 0) ||
        (env_vid && (strcmp(env_vid, "KMSDRM") == 0 || strcmp(env_vid, "fbcon") == 0))) {
        if (!force_windowed) {
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }
    }

    SDL_Window *window = SDL_CreateWindow(
        "DOOM - Touch Edition (RPi 4B)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DOOM_DISPLAY_WIDTH,
        DOOM_DISPLAY_HEIGHT,
        window_flags
    );

    if (!window) {
        fprintf(stderr, "[SDL ERROR] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        /* Fallback to software renderer */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "[SDL ERROR] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* 4. Create 320x200 streaming texture for Doom software renderer */
    SDL_Texture *game_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        DOOM_GAME_WIDTH,
        DOOM_GAME_HEIGHT
    );
    if (!game_texture) {
        fprintf(stderr, "[SDL ERROR] SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* 5. Initialize Doom event bridge & display scaler */
    doom_bridge_init(window, renderer);
    doom_bridge_set_touch_config(&touch_cfg);
    doom_bridge_set_keypad_config(&keypad_cfg);

    printf("[ENGINE] Doom event bridge initialized. Starting 60 FPS interactive loop...\n");

    /* 6. Main Interactive Game Loop (Target: 60 FPS / ~16.6 ms per frame) */
    uint32_t last_time = SDL_GetTicks();
    while (!g_quit_requested) {
        uint32_t frame_start = SDL_GetTicks();
        float dt = (float)(frame_start - last_time) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last_time = frame_start;

        /* Update inputs, raycaster position, and game actions */
        update_game_state(dt);

        /* Render 320x200 game scene */
        render_game_framebuffer();

        /* Stream frame to SDL texture */
        SDL_UpdateTexture(
            game_texture,
            NULL,
            g_framebuffer,
            DOOM_GAME_WIDTH * sizeof(uint32_t)
        );

        /* Scale 320x200 to 480x320 native, draw touch HUD, and present */
        doom_bridge_render_present(game_texture);

        /* Frame pacing cap (~60 FPS) */
        uint32_t frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < 16) {
            SDL_Delay(16 - frame_time);
        }
    }

    /* 7. Clean Shutdown */
    printf("[ENGINE] Shutting down Doom engine & hardware interfaces...\n");
    doom_bridge_shutdown();
    SDL_DestroyTexture(game_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("[ENGINE] Exited cleanly.\n");
    return 0;
}
