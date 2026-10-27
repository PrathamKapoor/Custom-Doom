#!/usr/bin/env python3
"""Scan the wired BCM 4x4 keypad and forward debounced keys to Doom."""
from __future__ import annotations

import signal
import sys
import time

try:
    from .keypad_controls import (
        KEYPAD_COLS, KEYPAD_LAYOUT, KEYPAD_ROWS, KEYMAP, MatrixDebouncer,
        key_names_for, validate_gpio_pin_map,
    )
except ImportError:  # Direct execution as scripts/keypad_bridge.py
    from keypad_controls import (
        KEYPAD_COLS, KEYPAD_LAYOUT, KEYPAD_ROWS, KEYMAP, MatrixDebouncer,
        key_names_for, validate_gpio_pin_map,
    )

SCAN_PERIOD = 0.010  # 100 Hz, matching the existing matrix driver.
SETTLE_SECONDS = 0.00015


def scan_matrix(gpio, rows=KEYPAD_ROWS, cols=KEYPAD_COLS):
    """Read one frame while driving only one row low at a time."""
    validate_gpio_pin_map(rows, cols)
    pressed = set()
    for row_index, row_pin in enumerate(rows):
        # The active row is pulled low; all other rows stay high-impedance.
        gpio.setup(row_pin, gpio.OUT, initial=gpio.LOW)
        try:
            time.sleep(SETTLE_SECONDS)
            for col_index, col_pin in enumerate(cols):
                if gpio.input(col_pin) == gpio.LOW:
                    pressed.add(KEYPAD_LAYOUT[row_index][col_index])
        finally:
            gpio.setup(row_pin, gpio.IN, pull_up_down=gpio.PUD_OFF)
    return frozenset(pressed)


def initialize_gpio(gpio):
    validate_gpio_pin_map()
    gpio.setwarnings(False)
    gpio.setmode(gpio.BCM)
    for row_pin in KEYPAD_ROWS:
        gpio.setup(row_pin, gpio.IN, pull_up_down=gpio.PUD_OFF)
    for col_pin in KEYPAD_COLS:
        gpio.setup(col_pin, gpio.IN, pull_up_down=gpio.PUD_UP)


def main() -> int:
    try:
        import evdev
        import RPi.GPIO as GPIO

        initialize_gpio(GPIO)
        codes = evdev.ecodes
        key_codes = sorted({getattr(codes, key_name)
                            for names in KEYMAP.values() for key_name in names})
        device = evdev.UInput(events={codes.EV_KEY: key_codes}, name="Custom Doom 4x4 Keypad")
    except Exception as exc:
        print(f"Keypad bridge unavailable: {exc}", file=sys.stderr, flush=True)
        try:
            import RPi.GPIO as GPIO
            GPIO.cleanup(list(KEYPAD_ROWS + KEYPAD_COLS))
        except Exception:
            pass
        return 2

    running = True

    def stop(_signal, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    debounce = MatrixDebouncer(20)
    last_codes = set()
    try:
        print("Keypad bridge active on BCM rows 21,20,16,19 and columns 13,12,6,5", flush=True)
        while running:
            started = time.monotonic()
            try:
                raw = scan_matrix(GPIO)
            except Exception as exc:
                print(f"Keypad scan failed: {exc}", file=sys.stderr, flush=True)
                break
            transitions = debounce.update(raw, int(started * 1000))
            for key, is_down in transitions:
                print(f"Keypad {key} {'down' if is_down else 'up'}", flush=True)
            new_codes = {getattr(codes, name) for name in key_names_for(debounce.pressed)}
            if new_codes != last_codes:
                for code in sorted(last_codes - new_codes):
                    device.write(codes.EV_KEY, code, 0)
                for code in sorted(new_codes - last_codes):
                    device.write(codes.EV_KEY, code, 1)
                device.syn()
                last_codes = new_codes
            remaining = SCAN_PERIOD - (time.monotonic() - started)
            if remaining > 0:
                time.sleep(remaining)
    finally:
        for code in sorted(last_codes):
            device.write(codes.EV_KEY, code, 0)
        if last_codes:
            device.syn()
        device.close()
        GPIO.cleanup(list(KEYPAD_ROWS + KEYPAD_COLS))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
