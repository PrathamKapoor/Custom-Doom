#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int touch_enabled;
    float touch_opacity;
    int dpad_deadzone;
    float turn_sensitivity;
    int screen_rotation; // 0, 90, 180, 270
} touch_config_t;

typedef struct {
    int keypad_enabled;
    int row_pins[4];
    int col_pins[4];
    int debounce_ms;
    int key_bindings[16]; // Mapped Doom action codes
} keypad_config_t;

/**
 * @brief Populate touch and keypad config structs with factory-safe defaults.
 *
 * Safe defaults:
 *  - Touch: enabled=1, opacity=0.55, deadzone=15, sensitivity=1.2, rotation=270
 *  - Keypad: enabled=0, rows=[4,27,22,23], cols=[24,5,16,26], debounce=15ms
 *  - Note: Physical Pin 22 (GPIO 25) is strictly reserved for LCD D/C.
 *          Col 2 is pinned to GPIO 5.
 */
void config_set_defaults(touch_config_t *tcfg, keypad_config_t *kcfg);

/**
 * @brief Load hardware configurations from doom_touch.cfg and doom_keypad.cfg.
 *
 * @param dir_path Directory containing the cfg files (e.g. "~/.doom-touch" or custom dir).
 *                 If NULL or empty, defaults to "~/.doom-touch".
 * @param tcfg Output touch configuration struct (can be NULL if not needed).
 * @param kcfg Output keypad configuration struct (can be NULL if not needed).
 * @return 0 if at least one file was successfully read, -1 if neither existed on disk.
 */
int config_load(const char *dir_path, touch_config_t *tcfg, keypad_config_t *kcfg);

/**
 * @brief Save hardware configurations to doom_touch.cfg and doom_keypad.cfg.
 *
 * @param dir_path Directory where cfg files should be saved.
 *                 If NULL or empty, defaults to "~/.doom-touch".
 *                 The directory is created if it does not already exist.
 * @param tcfg Input touch configuration struct (can be NULL if not saving touch).
 * @param kcfg Input keypad configuration struct (can be NULL if not saving keypad).
 * @return 0 on success, non-zero on failure.
 */
int config_save(const char *dir_path, const touch_config_t *tcfg, const keypad_config_t *kcfg);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_MANAGER_H */
