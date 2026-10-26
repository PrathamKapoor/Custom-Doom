#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "../src/hardware/config_manager.h"

int main(void) {
    printf("Running test_config_manager...\n");

    touch_config_t tcfg;
    keypad_config_t kcfg;
    config_set_defaults(&tcfg, &kcfg);

    // Verify touch defaults
    assert(tcfg.touch_enabled == 1);
    assert(fabsf(tcfg.touch_opacity - 0.55f) < 0.001f);
    assert(tcfg.dpad_deadzone == 15);
    assert(fabsf(tcfg.turn_sensitivity - 1.2f) < 0.001f);
    assert(tcfg.screen_rotation == 270);

    // Verify keypad defaults
    assert(kcfg.keypad_enabled == 0);
    assert(kcfg.row_pins[0] == 4);
    assert(kcfg.row_pins[1] == 27);
    assert(kcfg.row_pins[2] == 22);
    assert(kcfg.row_pins[3] == 23);
    assert(kcfg.col_pins[0] == 24);
    assert(kcfg.col_pins[1] == 5); // CRITICAL: Must be GPIO 5, NOT GPIO 25!
    assert(kcfg.col_pins[2] == 16);
    assert(kcfg.col_pins[3] == 26);
    assert(kcfg.debounce_ms == 15);

    // Test NULL safety in config_set_defaults
    config_set_defaults(&tcfg, NULL);
    assert(tcfg.touch_enabled == 1);
    config_set_defaults(NULL, &kcfg);
    assert(kcfg.col_pins[1] == 5);

    // Save defaults to test dir
    const char *test_dir = "./tmp_config_test";
    assert(config_save(test_dir, &tcfg, &kcfg) == 0);

    // Load back from test dir
    touch_config_t loaded_t;
    keypad_config_t loaded_k;
    memset(&loaded_t, 0, sizeof(loaded_t));
    memset(&loaded_k, 0, sizeof(loaded_k));
    assert(config_load(test_dir, &loaded_t, &loaded_k) == 0);

    assert(loaded_t.touch_enabled == 1);
    assert(fabsf(loaded_t.touch_opacity - 0.55f) < 0.001f);
    assert(loaded_t.dpad_deadzone == 15);
    assert(fabsf(loaded_t.turn_sensitivity - 1.2f) < 0.001f);
    assert(loaded_t.screen_rotation == 270);

    assert(loaded_k.keypad_enabled == 0);
    assert(loaded_k.row_pins[0] == 4);
    assert(loaded_k.row_pins[1] == 27);
    assert(loaded_k.row_pins[2] == 22);
    assert(loaded_k.row_pins[3] == 23);
    assert(loaded_k.col_pins[0] == 24);
    assert(loaded_k.col_pins[1] == 5); // CRITICAL: Must be GPIO 5, NOT GPIO 25!
    assert(loaded_k.col_pins[2] == 16);
    assert(loaded_k.col_pins[3] == 26);
    assert(loaded_k.debounce_ms == 15);

    // Test custom values round-trip
    tcfg.touch_enabled = 0;
    tcfg.touch_opacity = 0.85f;
    tcfg.dpad_deadzone = 25;
    tcfg.turn_sensitivity = 2.0f;
    tcfg.screen_rotation = 90;

    kcfg.keypad_enabled = 1;
    kcfg.debounce_ms = 30;
    assert(config_save(test_dir, &tcfg, &kcfg) == 0);

    assert(config_load(test_dir, &loaded_t, &loaded_k) == 0);
    assert(loaded_t.touch_enabled == 0);
    assert(fabsf(loaded_t.touch_opacity - 0.85f) < 0.001f);
    assert(loaded_t.dpad_deadzone == 25);
    assert(fabsf(loaded_t.turn_sensitivity - 2.0f) < 0.001f);
    assert(loaded_t.screen_rotation == 90);
    assert(loaded_k.keypad_enabled == 1);
    assert(loaded_k.debounce_ms == 30);

    // CRITICAL SAFETY CHECK: Pin 22 (GPIO 25) reservation
    // If a configuration file attempts to assign GPIO 25 to keypad, config_load MUST override it to safe GPIO 5
    FILE *fp = fopen("./tmp_config_test/doom_keypad.cfg", "w");
    assert(fp != NULL);
    fprintf(fp, "[keypad]\nenabled = 1\nrows = 4,27,22,23\ncols = 24,25,16,26\ndebounce_ms = 20\n");
    fclose(fp);

    assert(config_load(test_dir, &loaded_t, &loaded_k) == 0);
    assert(loaded_k.col_pins[1] == 5); // Must NEVER allow GPIO 25!

    // Cleanup test artifacts
    remove("./tmp_config_test/doom_touch.cfg");
    remove("./tmp_config_test/doom_keypad.cfg");
#ifdef _WIN32
    _rmdir("./tmp_config_test");
#else
    rmdir("./tmp_config_test");
#endif

    printf("test_config_manager PASSED\n");
    return 0;
}
