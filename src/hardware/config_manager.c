#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

#define LCD_RESERVED_GPIO 25

static char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void resolve_dir_path(const char *input_path, char *out_buf, size_t out_len) {
    if (!input_path || input_path[0] == '\0') {
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home) home = getenv("USERPROFILE");
#endif
        if (!home) home = ".";
        snprintf(out_buf, out_len, "%s/.doom-touch", home);
    } else if (input_path[0] == '~') {
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home) home = getenv("USERPROFILE");
#endif
        if (!home) home = ".";
        snprintf(out_buf, out_len, "%s%s", home, input_path + 1);
    } else {
        snprintf(out_buf, out_len, "%s", input_path);
    }
}

static int ensure_dir_exists(const char *dir) {
    if (!dir || dir[0] == '\0') return 0;

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    size_t len = strlen(tmp);

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char slash = tmp[i];
            tmp[i] = '\0';
            if (MKDIR(tmp) != 0 && errno != EEXIST) {
                // Ignore failure if it exists or drive letter root
            }
            tmp[i] = slash;
        }
    }

    if (MKDIR(tmp) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void parse_int_array(const char *csv, int *arr, int max_count) {
    if (!csv || !arr || max_count <= 0) return;
    char buf[256];
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ", \t");
    int count = 0;
    while (token != NULL && count < max_count) {
        arr[count++] = atoi(token);
        token = strtok(NULL, ", \t");
    }
}

static void enforce_gpio_safety(keypad_config_t *kcfg) {
    if (!kcfg) return;

    // Factory safe fallbacks
    static const int safe_rows[4] = {4, 27, 22, 23};
    static const int safe_cols[4] = {24, 5, 16, 26};

    for (int i = 0; i < 4; i++) {
        if (kcfg->row_pins[i] == LCD_RESERVED_GPIO) {
            fprintf(stderr, "[CONFIG SAFETY] GPIO %d is strictly reserved for LCD D/C! Overriding row %d to %d\n",
                    LCD_RESERVED_GPIO, i, safe_rows[i]);
            kcfg->row_pins[i] = safe_rows[i];
        }
    }

    for (int i = 0; i < 4; i++) {
        if (kcfg->col_pins[i] == LCD_RESERVED_GPIO) {
            fprintf(stderr, "[CONFIG SAFETY] GPIO %d is strictly reserved for LCD D/C! Overriding col %d to %d\n",
                    LCD_RESERVED_GPIO, i, safe_cols[i]);
            kcfg->col_pins[i] = safe_cols[i];
        }
    }

    // Explicit check for Col 2 (physical pin 29) -> MUST be GPIO 5
    if (kcfg->col_pins[1] != 5 && kcfg->col_pins[1] == LCD_RESERVED_GPIO) {
        kcfg->col_pins[1] = 5;
    }
}

void config_set_defaults(touch_config_t *tcfg, keypad_config_t *kcfg) {
    if (tcfg) {
        tcfg->touch_enabled = 1;
        tcfg->touch_opacity = 0.55f;
        tcfg->dpad_deadzone = 15;
        tcfg->turn_sensitivity = 1.2f;
        tcfg->screen_rotation = 270;
    }

    if (kcfg) {
        kcfg->keypad_enabled = 0;
        kcfg->row_pins[0] = 4;
        kcfg->row_pins[1] = 27;
        kcfg->row_pins[2] = 22;
        kcfg->row_pins[3] = 23;
        kcfg->col_pins[0] = 24;
        kcfg->col_pins[1] = 5;  // GPIO 5, NOT GPIO 25!
        kcfg->col_pins[2] = 16;
        kcfg->col_pins[3] = 26;
        kcfg->debounce_ms = 15;
        for (int i = 0; i < 16; i++) {
            kcfg->key_bindings[i] = i;
        }
    }
}

int config_load(const char *dir_path, touch_config_t *tcfg, keypad_config_t *kcfg) {
    char dir[512];
    resolve_dir_path(dir_path, dir, sizeof(dir));

    char path[1024];
    char line[256];

    // Load touch config
    if (tcfg) {
        config_set_defaults(tcfg, NULL);

        snprintf(path, sizeof(path), "%s/doom_touch.cfg", dir);
        FILE *fp = fopen(path, "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                char *trimmed = trim_whitespace(line);
                if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') continue;
                if (trimmed[0] == '[') continue; // Section header

                char *eq = strchr(trimmed, '=');
                if (eq) {
                    *eq = '\0';
                    char *key = trim_whitespace(trimmed);
                    char *val = trim_whitespace(eq + 1);

                    if (strcmp(key, "enabled") == 0) {
                        tcfg->touch_enabled = atoi(val);
                    } else if (strcmp(key, "opacity") == 0) {
                        tcfg->touch_opacity = (float)atof(val);
                    } else if (strcmp(key, "dpad_deadzone") == 0) {
                        tcfg->dpad_deadzone = atoi(val);
                    } else if (strcmp(key, "turn_sensitivity") == 0) {
                        tcfg->turn_sensitivity = (float)atof(val);
                    } else if (strcmp(key, "screen_rotation") == 0) {
                        tcfg->screen_rotation = atoi(val);
                    }
                }
            }
            fclose(fp);
        }
    }

    // Load keypad config
    if (kcfg) {
        config_set_defaults(NULL, kcfg);

        snprintf(path, sizeof(path), "%s/doom_keypad.cfg", dir);
        FILE *fp = fopen(path, "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                char *trimmed = trim_whitespace(line);
                if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') continue;
                if (trimmed[0] == '[') continue;

                char *eq = strchr(trimmed, '=');
                if (eq) {
                    *eq = '\0';
                    char *key = trim_whitespace(trimmed);
                    char *val = trim_whitespace(eq + 1);

                    if (strcmp(key, "enabled") == 0) {
                        kcfg->keypad_enabled = atoi(val);
                    } else if (strcmp(key, "rows") == 0) {
                        parse_int_array(val, kcfg->row_pins, 4);
                    } else if (strcmp(key, "cols") == 0) {
                        parse_int_array(val, kcfg->col_pins, 4);
                    } else if (strcmp(key, "debounce_ms") == 0) {
                        kcfg->debounce_ms = atoi(val);
                    } else if (strcmp(key, "bindings") == 0) {
                        parse_int_array(val, kcfg->key_bindings, 16);
                    }
                }
            }
            fclose(fp);
        }
        enforce_gpio_safety(kcfg);
    }

    return 0;
}

int config_save(const char *dir_path, const touch_config_t *tcfg, const keypad_config_t *kcfg) {
    char dir[512];
    resolve_dir_path(dir_path, dir, sizeof(dir));

    if (ensure_dir_exists(dir) != 0) {
        return -1;
    }

    char path[1024];

    if (tcfg) {
        snprintf(path, sizeof(path), "%s/doom_touch.cfg", dir);
        FILE *fp = fopen(path, "w");
        if (!fp) return -1;

        fprintf(fp, "# DOOM Touch Screen Configuration\n");
        fprintf(fp, "[touch]\n");
        fprintf(fp, "enabled = %d\n", tcfg->touch_enabled);
        fprintf(fp, "opacity = %.2f\n", tcfg->touch_opacity);
        fprintf(fp, "dpad_deadzone = %d\n", tcfg->dpad_deadzone);
        fprintf(fp, "turn_sensitivity = %.2f\n", tcfg->turn_sensitivity);
        fprintf(fp, "screen_rotation = %d\n", tcfg->screen_rotation);

        fclose(fp);
    }

    if (kcfg) {
        // Enforce safety before saving
        keypad_config_t safe_copy = *kcfg;
        enforce_gpio_safety(&safe_copy);

        snprintf(path, sizeof(path), "%s/doom_keypad.cfg", dir);
        FILE *fp = fopen(path, "w");
        if (!fp) return -1;

        fprintf(fp, "# DOOM 4x4 Matrix Keypad Configuration\n");
        fprintf(fp, "# CRITICAL: GPIO 25 is reserved for LCD D/C. Col 2 is GPIO 5.\n");
        fprintf(fp, "[keypad]\n");
        fprintf(fp, "enabled = %d\n", safe_copy.keypad_enabled);
        fprintf(fp, "rows = %d,%d,%d,%d\n",
                safe_copy.row_pins[0], safe_copy.row_pins[1],
                safe_copy.row_pins[2], safe_copy.row_pins[3]);
        fprintf(fp, "cols = %d,%d,%d,%d\n",
                safe_copy.col_pins[0], safe_copy.col_pins[1],
                safe_copy.col_pins[2], safe_copy.col_pins[3]);
        fprintf(fp, "debounce_ms = %d\n", safe_copy.debounce_ms);
        fprintf(fp, "bindings = %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                safe_copy.key_bindings[0], safe_copy.key_bindings[1],
                safe_copy.key_bindings[2], safe_copy.key_bindings[3],
                safe_copy.key_bindings[4], safe_copy.key_bindings[5],
                safe_copy.key_bindings[6], safe_copy.key_bindings[7],
                safe_copy.key_bindings[8], safe_copy.key_bindings[9],
                safe_copy.key_bindings[10], safe_copy.key_bindings[11],
                safe_copy.key_bindings[12], safe_copy.key_bindings[13],
                safe_copy.key_bindings[14], safe_copy.key_bindings[15]);

        fclose(fp);
    }

    return 0;
}
