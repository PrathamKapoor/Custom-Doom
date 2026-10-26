#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "../src/hardware/matrix_keypad.h"
#include "../src/hardware/config_manager.h"

static int g_last_key = -1;
static int g_last_state = -1;
static int g_event_count = 0;

static void test_callback(int key, int down) {
    g_last_key = key;
    g_last_state = down;
    g_event_count++;
}

static void reset_test_tracker(void) {
    g_last_key = -1;
    g_last_state = -1;
    g_event_count = 0;
}

static void test_step1_brief_scenario(void) {
    printf("Running test_step1_brief_scenario...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;

    assert(matrix_keypad_init(&cfg, test_callback) == 0);

    /* Simulate Row 1, Col 1 press (Key '1', Index 0) */
    matrix_keypad_simulate_raw_state(0, 0, 1);
    matrix_keypad_poll_once();
    assert(g_last_key == 0);
    assert(g_last_state == 1);

    /* Release */
    matrix_keypad_simulate_raw_state(0, 0, 0);
    matrix_keypad_poll_once();
    assert(g_last_key == 0);
    assert(g_last_state == 0);

    matrix_keypad_stop();
    printf("  [PASS] test_step1_brief_scenario\n");
}

static void test_all_16_keys_mapping(void) {
    printf("Running test_all_16_keys_mapping...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;

    assert(matrix_keypad_init(&cfg, test_callback) == 0);

    static const char expected_chars[16] = {
        '1', '2', '3', 'A',
        '4', '5', '6', 'B',
        '7', '8', '9', 'C',
        '*', '0', '#', 'D'
    };

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int key_index = r * 4 + c;
            char ch = matrix_keypad_get_key_char(key_index);
            assert(ch == expected_chars[key_index]);

            /* Press key */
            reset_test_tracker();
            matrix_keypad_simulate_raw_state(r, c, 1);
            matrix_keypad_poll_once();
            assert(g_last_key == key_index);
            assert(g_last_state == 1);
            assert(matrix_keypad_is_key_down(key_index) == 1);

            /* Release key */
            reset_test_tracker();
            matrix_keypad_simulate_raw_state(r, c, 0);
            matrix_keypad_poll_once();
            assert(g_last_key == key_index);
            assert(g_last_state == 0);
            assert(matrix_keypad_is_key_down(key_index) == 0);
        }
    }

    /* Out-of-bounds key index checks */
    assert(matrix_keypad_get_key_char(-1) == '\0');
    assert(matrix_keypad_get_key_char(16) == '\0');
    assert(matrix_keypad_is_key_down(-1) == 0);
    assert(matrix_keypad_is_key_down(16) == 0);

    matrix_keypad_stop();
    printf("  [PASS] test_all_16_keys_mapping\n");
}

static void test_multi_key_rollover(void) {
    printf("Running test_multi_key_rollover...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;
    assert(matrix_keypad_init(&cfg, test_callback) == 0);

    /* 1. Hold Move Forward (Row 0, Col 1, Key '2', index 1) */
    matrix_keypad_simulate_raw_state(0, 1, 1);
    matrix_keypad_poll_once();
    assert(g_last_key == 1 && g_last_state == 1);
    assert(matrix_keypad_is_key_down(1) == 1);

    /* 2. Simultaneously hold Speed / Run (Row 2, Col 1, Key '8', index 9) */
    matrix_keypad_simulate_raw_state(2, 1, 1);
    matrix_keypad_poll_once();
    assert(g_last_key == 9 && g_last_state == 1);
    assert(matrix_keypad_is_key_down(1) == 1);
    assert(matrix_keypad_is_key_down(9) == 1);

    /* 3. Simultaneously press Fire (Row 0, Col 3, Key 'A', index 3) */
    matrix_keypad_simulate_raw_state(0, 3, 1);
    matrix_keypad_poll_once();
    assert(g_last_key == 3 && g_last_state == 1);
    assert(matrix_keypad_is_key_down(1) == 1);
    assert(matrix_keypad_is_key_down(9) == 1);
    assert(matrix_keypad_is_key_down(3) == 1);

    /* 4. Release Run */
    matrix_keypad_simulate_raw_state(2, 1, 0);
    matrix_keypad_poll_once();
    assert(g_last_key == 9 && g_last_state == 0);
    assert(matrix_keypad_is_key_down(1) == 1);
    assert(matrix_keypad_is_key_down(9) == 0);
    assert(matrix_keypad_is_key_down(3) == 1);

    /* 5. Release Fire */
    matrix_keypad_simulate_raw_state(0, 3, 0);
    matrix_keypad_poll_once();
    assert(g_last_key == 3 && g_last_state == 0);
    assert(matrix_keypad_is_key_down(1) == 1);
    assert(matrix_keypad_is_key_down(3) == 0);

    /* 6. Release Forward */
    matrix_keypad_simulate_raw_state(0, 1, 0);
    matrix_keypad_poll_once();
    assert(g_last_key == 1 && g_last_state == 0);
    assert(matrix_keypad_is_key_down(1) == 0);

    matrix_keypad_stop();
    printf("  [PASS] test_multi_key_rollover\n");
}

static void test_debounce_filter(void) {
    printf("Running test_debounce_filter...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;
    cfg.debounce_ms = 15;
    assert(matrix_keypad_init(&cfg, test_callback) == 0);

    /* Initial state at t = 1000ms */
    matrix_keypad_set_simulated_time(1000);
    matrix_keypad_simulate_raw_state(1, 0, 1); /* Row 1, Col 0: Key '4', index 4 */
    matrix_keypad_poll_once();
    assert(g_last_key == 4 && g_last_state == 1);
    assert(g_event_count == 1);

    /* Glitch/bounce: raw drops to 0 at t = 1005ms (5ms < 15ms debounce threshold) */
    matrix_keypad_set_simulated_time(1005);
    matrix_keypad_simulate_raw_state(1, 0, 0);
    matrix_keypad_poll_once();
    /* Event must be filtered out! State remains pressed */
    assert(g_event_count == 1);
    assert(matrix_keypad_is_key_down(4) == 1);

    /* Chatter returns to 1 at t = 1010ms */
    matrix_keypad_set_simulated_time(1010);
    matrix_keypad_simulate_raw_state(1, 0, 1);
    matrix_keypad_poll_once();
    assert(g_event_count == 1);
    assert(matrix_keypad_is_key_down(4) == 1);

    /* Valid release at t = 1025ms (25ms >= 15ms debounce threshold) */
    matrix_keypad_set_simulated_time(1025);
    matrix_keypad_simulate_raw_state(1, 0, 0);
    matrix_keypad_poll_once();
    assert(g_last_key == 4 && g_last_state == 0);
    assert(g_event_count == 2);
    assert(matrix_keypad_is_key_down(4) == 0);

    matrix_keypad_reset_simulated_time();
    matrix_keypad_stop();
    printf("  [PASS] test_debounce_filter\n");
}

static void test_gpio25_safety_lockout(void) {
    printf("Running test_gpio25_safety_lockout...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;

    /* Violate safety: put GPIO 25 in col_pins[0] */
    cfg.col_pins[0] = 25;
    assert(matrix_keypad_init(&cfg, test_callback) != 0);

    /* Restore and violate in row_pins[2] */
    cfg.col_pins[0] = 24;
    cfg.row_pins[2] = 25;
    assert(matrix_keypad_init(&cfg, test_callback) != 0);

    printf("  [PASS] test_gpio25_safety_lockout\n");
}

static void test_null_and_disabled_safety(void) {
    printf("Running test_null_and_disabled_safety...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;

    /* Null checks */
    assert(matrix_keypad_init(NULL, test_callback) != 0);
    assert(matrix_keypad_init(&cfg, NULL) != 0);

    /* Disabled keypad */
    cfg.keypad_enabled = 0;
    assert(matrix_keypad_init(&cfg, test_callback) == 0);

    /* Key events should be ignored when disabled */
    matrix_keypad_simulate_raw_state(0, 0, 1);
    matrix_keypad_poll_once();
    assert(g_event_count == 0);
    assert(matrix_keypad_is_key_down(0) == 0);

    matrix_keypad_stop();
    printf("  [PASS] test_null_and_disabled_safety\n");
}

static void test_background_thread_scanning(void) {
    printf("Running test_background_thread_scanning...\n");
    reset_test_tracker();

    keypad_config_t cfg;
    config_set_defaults(NULL, &cfg);
    cfg.keypad_enabled = 1;
    cfg.debounce_ms = 10;

    assert(matrix_keypad_init(&cfg, test_callback) == 0);

    matrix_keypad_start_thread();

    /* Simulate press asynchronously */
    matrix_keypad_simulate_raw_state(0, 2, 1); /* Row 0, Col 2: Key '3', index 2 */

    /* Wait for thread poll (thread runs at 100Hz = 10ms) */
    int loops = 0;
    while (g_last_state != 1 && loops < 50) {
        usleep(5000); /* 5ms */
        loops++;
    }
    assert(g_last_key == 2);
    assert(g_last_state == 1);

    /* Release asynchronously */
    matrix_keypad_simulate_raw_state(0, 2, 0);
    loops = 0;
    while (g_last_state != 0 && loops < 50) {
        usleep(5000);
        loops++;
    }
    assert(g_last_key == 2);
    assert(g_last_state == 0);

    /* Calling start_thread again when already running should be a safe no-op */
    matrix_keypad_start_thread();

    /* Stop thread */
    matrix_keypad_stop();

    /* Calling stop again should be a safe no-op */
    matrix_keypad_stop();

    printf("  [PASS] test_background_thread_scanning\n");
}

int main(void) {
    printf("Starting test_matrix_keypad...\n");
    test_step1_brief_scenario();
    test_all_16_keys_mapping();
    test_multi_key_rollover();
    test_debounce_filter();
    test_gpio25_safety_lockout();
    test_null_and_disabled_safety();
    test_background_thread_scanning();
    printf("test_matrix_keypad PASSED\n");
    return 0;
}
