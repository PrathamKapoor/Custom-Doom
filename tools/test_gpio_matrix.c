/**
 * @file test_gpio_matrix.c
 * @brief Standalone visual terminal keypad testing utility for 4x4 matrix.
 *
 * Verifies 4x4 matrix wiring, pin safety (strict GPIO 25 lockout),
 * debouncing, and visualizes live key presses in an illuminated ASCII terminal grid.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "hardware/config_manager.h"
#include "hardware/matrix_keypad.h"

static volatile int g_running = 1;
static int g_key_states[MATRIX_KEYPAD_TOTAL_KEYS] = {0};

typedef struct {
    char key_char;
    const char *action_name;
    const char *doom_action;
} key_meta_t;

static const key_meta_t g_meta[MATRIX_KEYPAD_TOTAL_KEYS] = {
    {'1', "Strafe L", "DOOM_STRAFE_L"},
    {'2', "Move Fwd", "DOOM_MOVE_FWD"},
    {'3', "Strafe R", "DOOM_STRAFE_R"},
    {'A', "FIRE",     "DOOM_FIRE"},
    {'4', "Turn L",   "DOOM_TURN_L"},
    {'5', "Move Back","DOOM_MOVE_BACK"},
    {'6', "Turn R",   "DOOM_TURN_R"},
    {'B', "USE",      "DOOM_USE"},
    {'7', "Prev Wep", "DOOM_PREV_WEP"},
    {'8', "SPEED",    "DOOM_SPEED"},
    {'9', "Next Wep", "DOOM_NEXT_WEP"},
    {'C', "AUTOMAP",  "DOOM_AUTOMAP"},
    {'*', "ESC/Menu", "DOOM_MENU"},
    {'0', "ENTER",    "DOOM_ENTER"},
    {'#', "GAMMA",    "DOOM_GAMMA"},
    {'D', "HUD TOG",  "DOOM_HUD_TOG"}
};

#define MAX_LOG_ENTRIES 6
static char g_log[MAX_LOG_ENTRIES][128];
static int g_log_count = 0;

static void add_log(const char *msg) {
    if (g_log_count < MAX_LOG_ENTRIES) {
        strncpy(g_log[g_log_count++], msg, 127);
    } else {
        for (int i = 0; i < MAX_LOG_ENTRIES - 1; i++) {
            strncpy(g_log[i], g_log[i + 1], 127);
        }
        strncpy(g_log[MAX_LOG_ENTRIES - 1], msg, 127);
    }
}

static void on_key_event(int key_index, int is_down) {
    if (key_index >= 0 && key_index < MATRIX_KEYPAD_TOTAL_KEYS) {
        g_key_states[key_index] = is_down;
        char buf[128];
        snprintf(buf, sizeof(buf), "[%s] Key '%c' (%s) -> %s",
                 is_down ? "\033[1;32mPRESS  \033[0m" : "\033[1;31mRELEASE\033[0m",
                 g_meta[key_index].key_char,
                 g_meta[key_index].action_name,
                 g_meta[key_index].doom_action);
        add_log(buf);
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void render_grid(const keypad_config_t *cfg) {
    /* Clear screen and move cursor to home position */
    printf("\033[H\033[J");

    printf("========================================================================\n");
    printf("        4x4 MATRIX KEYPAD HARDWARE TEST UTILITY (RPi 4B)              \n");
    printf("========================================================================\n");
    printf(" Hardware Pins:  Rows: %d, %d, %d, %d  |  Cols: %d, %d, %d, %d\n",
           cfg->row_pins[0], cfg->row_pins[1], cfg->row_pins[2], cfg->row_pins[3],
           cfg->col_pins[0], cfg->col_pins[1], cfg->col_pins[2], cfg->col_pins[3]);
    printf(" Debounce Filter: %d ms | Scan Rate: 100 Hz | \033[1;32mGPIO 25: RESERVED FOR LCD\033[0m\n",
           cfg->debounce_ms);
    printf("------------------------------------------------------------------------\n\n");

    /* Draw 4x4 Grid */
    printf("  +---------------+---------------+---------------+---------------+\n");
    for (int r = 0; r < 4; r++) {
        /* Row Header Line with Key Characters */
        printf("  |");
        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            int down = g_key_states[idx];
            if (down) {
                /* Highlighted in bold green inverse */
                printf(" \033[1;30;42m   [ %c ] *    \033[0m |", g_meta[idx].key_char);
            } else {
                printf("     [ %c ]     |", g_meta[idx].key_char);
            }
        }
        printf("\n");

        /* Row Action Name Line */
        printf("  |");
        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            int down = g_key_states[idx];
            if (down) {
                printf(" \033[1;30;42m %-13s \033[0m |", g_meta[idx].action_name);
            } else {
                printf("  %-12s |", g_meta[idx].action_name);
            }
        }
        printf("\n");
        printf("  +---------------+---------------+---------------+---------------+\n");
    }

    printf("\n--- Real-Time Event Log (Press Ctrl+C to Exit) ---\n");
    for (int i = 0; i < g_log_count; i++) {
        printf("  %s\n", g_log[i]);
    }
    for (int i = g_log_count; i < MAX_LOG_ENTRIES; i++) {
        printf("\n");
    }
    printf("------------------------------------------------------------------------\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    int simulate_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sim") == 0 || strcmp(argv[i], "-s") == 0) {
            simulate_mode = 1;
        }
    }

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    config_load(NULL, NULL, &cfg);
    cfg.keypad_enabled = 1;

    /* STRICT GPIO 25 SAFETY CHECK */
    for (int i = 0; i < 4; i++) {
        if (cfg.row_pins[i] == 25 || cfg.col_pins[i] == 25) {
            fprintf(stderr, "\n\033[1;31m========================================================================\n");
            fprintf(stderr, " [CRITICAL HARDWARE PIN COLLISION DETECTED!]\n");
            fprintf(stderr, " Physical Pin 22 (BCM GPIO 25) is strictly reserved for 3.5\" SPI LCD D/C!\n");
            fprintf(stderr, " Wiring the keypad to GPIO 25 corrupts SPI display communication.\n");
            fprintf(stderr, " Scanning is strictly ABORTED to protect hardware.\n");
            fprintf(stderr, "========================================================================\033[0m\n\n");
            return 1;
        }
    }

    if (matrix_keypad_init(&cfg, on_key_event) != 0) {
        fprintf(stderr, "[ERROR] matrix_keypad_init failed.\n");
        return 1;
    }

    matrix_keypad_start_thread();
    add_log("Matrix scanning thread started at 100 Hz. Monitoring inputs...");

    if (simulate_mode) {
        add_log("Running interactive self-test simulation sequence...");
    }

    int sim_step = 0;
    while (g_running) {
        if (simulate_mode) {
            /* Cycle through each of the 16 keys every 200 ms */
            int active_key = sim_step % 16;
            int r = active_key / 4;
            int c = active_key % 4;

            matrix_keypad_simulate_raw_state(r, c, 1);
            matrix_keypad_poll_once();
            render_grid(&cfg);
            usleep(150000);

            matrix_keypad_simulate_raw_state(r, c, 0);
            matrix_keypad_poll_once();
            render_grid(&cfg);
            usleep(50000);

            sim_step++;
        } else {
            render_grid(&cfg);
            usleep(50000); // 20 FPS terminal refresh
        }
    }

    printf("\nStopping keypad thread and restoring terminal...\n");
    matrix_keypad_stop();
    printf("Keypad test completed successfully.\n");
    return 0;
}
