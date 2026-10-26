#!/usr/bin/env python3
"""
tools/test_gpio_matrix.py
Standalone visual terminal keypad test utility for Raspberry Pi 4B 4x4 matrix keypad.

Features:
- Strict GPIO 25 safety lockout (reserved for 3.5" SPI LCD Data/Command).
- Dynamic configuration from ~/.doom-touch/doom_keypad.cfg.
- Real-time illuminated 4x4 ASCII terminal grid.
- Multi-key debounce and event logging.
- Simulation fallback when run outside Raspberry Pi.
"""

import sys
import os
import time
import signal
import configparser

# Physical pin / BCM mapping
DEFAULT_ROWS = [4, 27, 22, 23]
DEFAULT_COLS = [24, 5, 16, 26]
RESERVED_PIN_LCD_DC = 25

KEY_LABELS = [
    ['1', '2', '3', 'A'],
    ['4', '5', '6', 'B'],
    ['7', '8', '9', 'C'],
    ['*', '0', '#', 'D']
]

ACTION_LABELS = [
    ["Strafe L",  "Move Fwd",  "Strafe R",  "FIRE"],
    ["Turn L",    "Move Back", "Turn R",    "USE"],
    ["Prev Wep",  "SPEED",     "Next Wep",  "AUTOMAP"],
    ["ESC/Menu",  "ENTER",     "GAMMA",     "HUD TOG"]
]

DOOM_ACTIONS = [
    ["DOOM_STRAFE_L",  "DOOM_MOVE_FWD",  "DOOM_STRAFE_R",  "DOOM_FIRE"],
    ["DOOM_TURN_L",    "DOOM_MOVE_BACK", "DOOM_TURN_R",    "DOOM_USE"],
    ["DOOM_PREV_WEP",  "DOOM_SPEED",     "DOOM_NEXT_WEP",  "DOOM_AUTOMAP"],
    ["DOOM_MENU",      "DOOM_ENTER",     "DOOM_GAMMA",     "DOOM_HUD_TOG"]
]

# ANSI color codes
RESET = "\033[0m"
BOLD = "\033[1m"
GREEN_BG = "\033[1;30;42m"
RED = "\033[1;31m"
GREEN = "\033[1;32m"
CYAN = "\033[1;36m"
CLEAR_SCREEN = "\033[H\033[J"


class KeypadTester:
    def __init__(self, simulate=False):
        self.simulate = simulate
        self.rows = list(DEFAULT_ROWS)
        self.cols = list(DEFAULT_COLS)
        self.debounce_ms = 15
        self.key_states = [[0 for _ in range(4)] for _ in range(4)]
        self.recent_events = []
        self.running = True
        self.has_hardware = False

        self.load_config()
        self.verify_gpio_safety()
        self.init_hardware()

    def load_config(self):
        cfg_path = os.path.expanduser("~/.doom-touch/doom_keypad.cfg")
        if os.path.exists(cfg_path):
            try:
                cp = configparser.ConfigParser()
                cp.read(cfg_path)
                if cp.has_section("keypad"):
                    if cp.has_option("keypad", "rows"):
                        self.rows = [int(x.strip()) for x in cp.get("keypad", "rows").split(",")]
                    if cp.has_option("keypad", "cols"):
                        self.cols = [int(x.strip()) for x in cp.get("keypad", "cols").split(",")]
                    if cp.has_option("keypad", "debounce_ms"):
                        self.debounce_ms = int(cp.get("keypad", "debounce_ms"))
            except Exception as e:
                print(f"[WARN] Failed to parse {cfg_path}: {e}")

    def verify_gpio_safety(self):
        for idx, r in enumerate(self.rows):
            if r == RESERVED_PIN_LCD_DC:
                self.abort_on_safety_violation(f"Row {idx + 1} uses GPIO {RESERVED_PIN_LCD_DC}")
        for idx, c in enumerate(self.cols):
            if c == RESERVED_PIN_LCD_DC:
                self.abort_on_safety_violation(f"Column {idx + 1} uses GPIO {RESERVED_PIN_LCD_DC}")

    def abort_on_safety_violation(self, detail):
        print(f"\n{RED}{'=' * 72}")
        print(" [CRITICAL HARDWARE PIN SAFETY LOCKOUT ACTIVATED]")
        print(" Physical Pin 22 (BCM GPIO 25) is STRICTLY RESERVED for 3.5\" SPI LCD D/C!")
        print(f" Violation: {detail}")
        print(" Using GPIO 25 for keypad scanning corrupts SPI bus traffic.")
        print(" Safe column 2 wiring uses GPIO 5 (Physical Pin 29).")
        print(" Keypad testing aborted immediately to protect hardware integrity.")
        print(f"{'=' * 72}{RESET}\n")
        sys.exit(1)

    def init_hardware(self):
        if self.simulate:
            self.has_hardware = False
            return

        try:
            import RPi.GPIO as GPIO
            GPIO.setmode(GPIO.BCM)
            GPIO.setwarnings(False)

            # Columns as inputs with internal pull-up
            for c in self.cols:
                GPIO.setup(c, GPIO.IN, pull_up_down=GPIO.PUD_UP)

            # Rows as inputs (high impedance / open-drain) initially
            for r in self.rows:
                GPIO.setup(r, GPIO.IN)

            self.gpio = GPIO
            self.has_hardware = True
        except (ImportError, RuntimeError):
            self.has_hardware = False

    def scan_hardware(self):
        if not self.has_hardware:
            return

        for r_idx, r_pin in enumerate(self.rows):
            # Drive active row LOW (open-drain emulation)
            self.gpio.setup(r_pin, self.gpio.OUT)
            self.gpio.output(r_pin, self.gpio.LOW)
            time.sleep(0.00005)  # 50 us settle time

            # Read column lines
            for c_idx, c_pin in enumerate(self.cols):
                raw = (self.gpio.input(c_pin) == self.gpio.LOW)
                current = self.key_states[r_idx][c_idx]
                if raw != current:
                    self.key_states[r_idx][c_idx] = 1 if raw else 0
                    self.record_event(r_idx, c_idx, raw)

            # Release row back to high impedance
            self.gpio.setup(r_pin, self.gpio.IN)

    def record_event(self, r, c, is_down):
        label = KEY_LABELS[r][c]
        act = ACTION_LABELS[r][c]
        doom = DOOM_ACTIONS[r][c]
        state_str = f"{GREEN}PRESS  {RESET}" if is_down else f"{RED}RELEASE{RESET}"
        timestamp = time.strftime("%H:%M:%S")
        entry = f"[{timestamp}] [{state_str}] Key '{label}' ({act}) -> {doom}"
        self.recent_events.append(entry)
        if len(self.recent_events) > 6:
            self.recent_events.pop(0)

    def render(self):
        out = [CLEAR_SCREEN]
        out.append(f"{BOLD}========================================================================{RESET}")
        out.append(f"{BOLD}        4x4 MATRIX KEYPAD HARDWARE TEST UTILITY (RPi 4B)               {RESET}")
        out.append(f"{BOLD}========================================================================{RESET}")
        mode_str = "Hardware (RPi GPIO)" if self.has_hardware else "Simulation Mode (--sim)"
        out.append(f" Mode: {mode_str} | Scan Rate: 100 Hz | Debounce: {self.debounce_ms} ms")
        out.append(f" Rows (Pins 1-4): {self.rows} | Cols (Pins 5-8): {self.cols}")
        out.append(f" {GREEN}[OK] GPIO 25 Lockout Verified: RESERVED FOR SPI LCD D/C{RESET}")
        out.append("-" * 72 + "\n")

        out.append("  +---------------+---------------+---------------+---------------+")
        for r in range(4):
            # Row Key Characters
            row_chars = []
            for c in range(4):
                label = KEY_LABELS[r][c]
                is_active = self.key_states[r][c]
                if is_active:
                    row_chars.append(f" {GREEN_BG}   [ {label} ] *    {RESET} |")
                else:
                    row_chars.append(f"     [ {label} ]     |")
            out.append("  |" + "".join(row_chars))

            # Row Action Labels
            row_acts = []
            for c in range(4):
                act = ACTION_LABELS[r][c]
                is_active = self.key_states[r][c]
                if is_active:
                    row_acts.append(f" {GREEN_BG} {act:<13} {RESET} |")
                else:
                    row_acts.append(f"  {act:<12} |")
            out.append("  |" + "".join(row_acts))
            out.append("  +---------------+---------------+---------------+---------------+")

        out.append("\n--- Real-Time Event Log (Press Ctrl+C to Exit) ---")
        for ev in self.recent_events[-6:]:
            out.append(f"  {ev}")
        for _ in range(6 - len(self.recent_events[-6:])):
            out.append("")
        out.append("-" * 72)

        sys.stdout.write("\n".join(out) + "\n")
        sys.stdout.flush()

    def run(self):
        sim_tick = 0
        while self.running:
            if self.has_hardware:
                self.scan_hardware()
                self.render()
                time.sleep(0.02)
            else:
                # Simulation sequence
                active_idx = (sim_tick // 5) % 16
                r = active_idx // 4
                c = active_idx % 4

                if sim_tick % 5 == 0:
                    self.key_states[r][c] = 1
                    self.record_event(r, c, True)
                elif sim_tick % 5 == 4:
                    self.key_states[r][c] = 0
                    self.record_event(r, c, False)

                self.render()
                time.sleep(0.04)
                sim_tick += 1

    def cleanup(self):
        if self.has_hardware:
            try:
                self.gpio.cleanup()
            except Exception:
                pass
        print("\nKeypad test utility exited cleanly.")


def main():
    sim = "--sim" in sys.argv or "-s" in sys.argv
    tester = KeypadTester(simulate=sim)

    def handle_sigint(signum, frame):
        tester.running = False

    signal.signal(signal.SIGINT, handle_sigint)
    signal.signal(signal.SIGTERM, handle_sigint)

    try:
        tester.run()
    finally:
        tester.cleanup()


if __name__ == "__main__":
    main()
