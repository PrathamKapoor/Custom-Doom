#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "../src/hardware/touch_overlay.h"
#include "../src/hardware/config_manager.h"

int main(void) {
    printf("Running test_touch_math...\n");
    touch_config_t cfg;
    config_set_defaults(&cfg, NULL);
    touch_overlay_init(&cfg, 480, 320);

    // 1. Test Left Thumb Dynamic Anchor
    // Press down at X: 100, Y: 200
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    // Drag forward 50 pixels to Y: 150
    touch_overlay_handle_motion(1, 100.0f / 480.0f, 150.0f / 320.0f);
    touch_game_actions_t actions = touch_overlay_get_actions();
    assert(actions.move_forward > 0.5f);
    assert(fabsf(actions.strafe_right) < 0.1f);

    // Verify stick state query
    int stick_active = 0;
    float ax = 0, ay = 0, cx = 0, cy = 0;
    assert(touch_overlay_get_stick_state(&stick_active, &ax, &ay, &cx, &cy) == 1);
    assert(stick_active == 1);
    assert(fabsf(ax - 100.0f) < 0.5f);
    assert(fabsf(ay - 200.0f) < 0.5f);
    assert(fabsf(cx - 100.0f) < 0.5f);
    assert(fabsf(cy - 150.0f) < 0.5f);

    // Release thumb
    touch_overlay_handle_up(1);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward == 0.0f);
    assert(actions.strafe_right == 0.0f);
    assert(touch_overlay_get_stick_state(&stick_active, NULL, NULL, NULL, NULL) == 0);
    assert(stick_active == 0);

    // 2. Test Deadzone Clamping (deadzone default is 15 pixels)
    // Press down at X: 100, Y: 200
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    // Move only 8 pixels forward to Y: 192 (inside deadzone)
    touch_overlay_handle_motion(1, 100.0f / 480.0f, 192.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward == 0.0f);
    assert(actions.strafe_right == 0.0f);
    touch_overlay_handle_up(1);

    // 3. Test Backward & Strafe
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    // Move backward 50 pixels to Y: 250
    touch_overlay_handle_motion(1, 100.0f / 480.0f, 250.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward < -0.5f);
    assert(fabsf(actions.strafe_right) < 0.1f);

    // Strafe right 50 pixels at Y: 200
    touch_overlay_handle_motion(1, 150.0f / 480.0f, 200.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(fabsf(actions.move_forward) < 0.1f);
    assert(actions.strafe_right > 0.5f);
    touch_overlay_handle_up(1);

    // 4. Test Fire Button Zone (Top-right tap at X: 450, Y: 120)
    touch_overlay_handle_down(2, 450.0f / 480.0f, 120.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.fire_pressed == 1);
    touch_overlay_handle_up(2);
    actions = touch_overlay_get_actions();
    assert(actions.fire_pressed == 0);

    // 5. Test Use Button Zone (Mid-right tap at X: 450, Y: 200)
    touch_overlay_handle_down(3, 450.0f / 480.0f, 200.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.use_pressed == 1);
    touch_overlay_handle_up(3);
    actions = touch_overlay_get_actions();
    assert(actions.use_pressed == 0);

    // 6. Test Weapon Switch Zones (Prev at X: 370, Y: 280; Next at X: 450, Y: 280)
    touch_overlay_handle_down(4, 370.0f / 480.0f, 280.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.prev_weapon == 1);
    touch_overlay_handle_up(4);
    actions = touch_overlay_get_actions();
    assert(actions.prev_weapon == 0);

    touch_overlay_handle_down(5, 450.0f / 480.0f, 280.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.next_weapon == 1);
    touch_overlay_handle_up(5);
    actions = touch_overlay_get_actions();
    assert(actions.next_weapon == 0);

    // 7. Test Menu & Automap Zones (Menu at X: 20, Y: 20; Automap at X: 460, Y: 20)
    touch_overlay_handle_down(6, 20.0f / 480.0f, 20.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.menu_pressed == 1);
    touch_overlay_handle_up(6);
    actions = touch_overlay_get_actions();
    assert(actions.menu_pressed == 0);

    touch_overlay_handle_down(7, 460.0f / 480.0f, 20.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.automap_pressed == 1);
    touch_overlay_handle_up(7);
    actions = touch_overlay_get_actions();
    assert(actions.automap_pressed == 0);

    // 8. Test HUD Toggle Zone (X: 240, Y: 20)
    touch_overlay_handle_down(9, 240.0f / 480.0f, 20.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.toggle_hud == 1);
    touch_overlay_handle_up(9);
    actions = touch_overlay_get_actions();
    assert(actions.toggle_hud == 0);

    // 9. Test Look Swipe (Right-half swipe X: 300 -> 350, Y: 160)
    touch_overlay_handle_down(8, 300.0f / 480.0f, 160.0f / 320.0f);
    touch_overlay_handle_motion(8, 350.0f / 480.0f, 160.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.turn_speed > 0.0f);
    touch_overlay_handle_up(8);
    actions = touch_overlay_get_actions();
    assert(actions.turn_speed == 0.0f);

    // 10. Test Simultaneous Multi-Touch (Thumbstick moving while Fire held)
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    touch_overlay_handle_motion(1, 100.0f / 480.0f, 150.0f / 320.0f);
    touch_overlay_handle_down(2, 450.0f / 480.0f, 120.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward > 0.5f);
    assert(actions.fire_pressed == 1);

    touch_overlay_handle_up(2);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward > 0.5f);
    assert(actions.fire_pressed == 0);

    touch_overlay_handle_up(1);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward == 0.0f);

    // 11. Test Raw Coordinate Transform & Screen Rotation
    float nx = 0, ny = 0;
    // Default cfg is rotation=270: nx = v, ny = 1.0 - u
    touch_overlay_transform_raw(0, 0, &nx, &ny);
    assert(fabsf(nx - 0.0f) < 0.01f);
    assert(fabsf(ny - 1.0f) < 0.01f);

    // Rotation 0
    cfg.screen_rotation = 0;
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_transform_raw(4095, 0, &nx, &ny);
    assert(fabsf(nx - 1.0f) < 0.01f);
    assert(fabsf(ny - 0.0f) < 0.01f);

    // Rotation 90: nx = 1.0 - v, ny = u
    cfg.screen_rotation = 90;
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_transform_raw(4095, 0, &nx, &ny);
    assert(fabsf(nx - 1.0f) < 0.01f);
    assert(fabsf(ny - 1.0f) < 0.01f);

    // Rotation 180: nx = 1.0 - u, ny = 1.0 - v
    cfg.screen_rotation = 180;
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_transform_raw(4095, 0, &nx, &ny);
    assert(fabsf(nx - 0.0f) < 0.01f);
    assert(fabsf(ny - 1.0f) < 0.01f);

    // 12. Test IIR Filter
    float fx = 0, fy = 0;
    touch_overlay_apply_iir_filter(100.0f, 200.0f, 50.0f, 150.0f, &fx, &fy);
    // 0.7 * 100 + 0.3 * 50 = 70 + 15 = 85
    assert(fabsf(fx - 85.0f) < 0.01f);
    // 0.7 * 200 + 0.3 * 150 = 140 + 45 = 185
    assert(fabsf(fy - 185.0f) < 0.01f);

    // 13. Test HUD Opacity & Cycle Visibility
    touch_overlay_set_opacity(0.75f);
    assert(fabsf(touch_overlay_get_opacity() - 0.75f) < 0.01f);
    touch_overlay_cycle_hud_visibility(); // 0.75 -> 0.35
    assert(fabsf(touch_overlay_get_opacity() - 0.35f) < 0.01f);
    touch_overlay_cycle_hud_visibility(); // 0.35 -> 0.0
    assert(fabsf(touch_overlay_get_opacity() - 0.0f) < 0.01f);
    touch_overlay_cycle_hud_visibility(); // 0.0 -> 0.75
    assert(fabsf(touch_overlay_get_opacity() - 0.75f) < 0.01f);

    // 14. Test Touch Disabled Behavior
    cfg.touch_enabled = 0;
    touch_overlay_init(&cfg, 480, 320);
    touch_overlay_handle_down(1, 100.0f / 480.0f, 200.0f / 320.0f);
    actions = touch_overlay_get_actions();
    assert(actions.move_forward == 0.0f);
    assert(actions.fire_pressed == 0);

    printf("test_touch_math PASSED\n");
    return 0;
}
