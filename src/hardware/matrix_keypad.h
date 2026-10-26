#ifndef MATRIX_KEYPAD_H
#define MATRIX_KEYPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "config_manager.h"

#define MATRIX_KEYPAD_ROWS 4
#define MATRIX_KEYPAD_COLS 4
#define MATRIX_KEYPAD_TOTAL_KEYS 16
#define MATRIX_KEYPAD_RESERVED_PIN_LCD_DC 25
#define MATRIX_KEYPAD_POLL_RATE_HZ 100
#define MATRIX_KEYPAD_POLL_INTERVAL_US 10000

/**
 * @brief Callback invoked whenever a key changes state (press or release).
 *
 * @param key_index Index from 0 to 15 (row * 4 + col)
 * @param is_down 1 if pressed, 0 if released
 */
typedef void (*keypad_event_callback_t)(int key_index, int is_down);

/**
 * @brief Initialize the 4x4 matrix keypad driver.
 *
 * Verifies pin safety (rejects GPIO 25), configures row outputs and column inputs,
 * sets up debouncing state machine, and registers event callback.
 *
 * @param config Keypad configuration structure with row/col pins and debounce_ms.
 * @param callback Function called on debounced key events.
 * @return 0 on success, non-zero if invalid args or pin safety violation.
 */
int matrix_keypad_init(const keypad_config_t *config, keypad_event_callback_t callback);

/**
 * @brief Perform a single matrix scan cycle and update debounced key states.
 *
 * Used for deterministic single-tick testing and manual polling.
 */
void matrix_keypad_poll_once(void);

/**
 * @brief Start the POSIX background thread scanning the matrix at 100 Hz.
 */
void matrix_keypad_start_thread(void);

/**
 * @brief Gracefully signal the background scanning thread to stop and join it.
 */
void matrix_keypad_stop(void);

/**
 * @brief Query current debounced state of a key.
 *
 * @param key_index Index 0..15.
 * @return 1 if pressed, 0 if released or out of bounds.
 */
int matrix_keypad_is_key_down(int key_index);

/**
 * @brief Get the standard matrix label character ('1'..'D') for a key index.
 *
 * @param key_index Index 0..15.
 * @return Character ('1'-'9', '0', 'A'-'D', '*', '#') or '\0' if invalid.
 */
char matrix_keypad_get_key_char(int key_index);

/**
 * @brief Simulate a raw physical contact switch state for host/testing.
 *
 * @param row Matrix row (0..3)
 * @param col Matrix col (0..3)
 * @param pressed 1 for closed contact, 0 for open contact
 */
void matrix_keypad_simulate_raw_state(int row, int col, int pressed);

/**
 * @brief Set simulated monotonic time in milliseconds for precise debounce testing.
 *
 * @param time_ms Current simulated timestamp in milliseconds.
 */
void matrix_keypad_set_simulated_time(uint32_t time_ms);

/**
 * @brief Advance simulated monotonic time by a specified delta in milliseconds.
 *
 * @param delta_ms Milliseconds to advance.
 */
void matrix_keypad_advance_simulated_time(uint32_t delta_ms);

/**
 * @brief Reset simulated time override back to automatic progression.
 */
void matrix_keypad_reset_simulated_time(void);

#ifdef __cplusplus
}
#endif

#endif /* MATRIX_KEYPAD_H */
