"""Wiring, default bindings, and debounce state for the physical 4x4 pad."""
from __future__ import annotations


# Matrix ribbon wires 1-4 are rows; wires 5-8 are columns.
# These BCM pins match the user's verified 40-pin-header diagram.
KEYPAD_ROWS = (21, 20, 16, 19)
KEYPAD_COLS = (13, 12, 6, 5)
KEYPAD_LAYOUT = ("123A", "456B", "789C", "*0#D")
KEYPAD_KEYS = tuple("".join(KEYPAD_LAYOUT))

# Doom bindings: direction keys follow the standard PC layout. The number pad
# supplies both aim directions and strafe chords for one-handed play.
KEYMAP = {
    "1": ("KEY_LEFTALT", "KEY_LEFT"), "2": ("KEY_DOWN",),
    "3": ("KEY_LEFTALT", "KEY_RIGHT"), "4": ("KEY_LEFT",),
    "5": ("KEY_LEFTCTRL",), "6": ("KEY_RIGHT",),
    "7": ("KEY_1",), "8": ("KEY_UP",), "9": ("KEY_2",),
    "0": ("KEY_SPACE",), "*": ("KEY_LEFTSHIFT",), "#": ("KEY_TAB",),
    "A": ("KEY_LEFTBRACE",), "B": ("KEY_RIGHTBRACE",),
    "C": ("KEY_PAUSE",), "D": ("KEY_ESC",),
}


def validate_gpio_pin_map(rows=KEYPAD_ROWS, cols=KEYPAD_COLS) -> bool:
    if len(rows) != 4 or len(cols) != 4:
        raise ValueError("A 4×4 keypad needs exactly four row and four column pins")
    pins = tuple(rows) + tuple(cols)
    if any(not isinstance(pin, int) or pin < 0 or pin > 27 for pin in pins):
        raise ValueError("BCM GPIO pins must be integers in the range 0..27")
    if len(set(pins)) != 8:
        raise ValueError("Each keypad row and column must use a unique GPIO pin")
    if 25 in pins:
        raise ValueError("BCM GPIO 25 is reserved by the display")
    return True


def key_names_for(pressed_keys) -> set[str]:
    return {name for key in pressed_keys for name in KEYMAP.get(key, ())}


class MatrixDebouncer:
    """Turn raw matrix snapshots into stable press/release transitions."""

    def __init__(self, debounce_ms: int = 20):
        self.debounce_ms = max(0, int(debounce_ms))
        self.pressed: set[str] = set()
        self._candidate = {key: False for key in KEYPAD_KEYS}
        self._changed_at = {key: 0 for key in KEYPAD_KEYS}

    def update(self, raw_pressed, now_ms: int) -> tuple[tuple[str, bool], ...]:
        raw_pressed = set(raw_pressed) & set(KEYPAD_KEYS)
        events = []
        for key in KEYPAD_KEYS:
            raw = key in raw_pressed
            stable = key in self.pressed
            if raw != self._candidate[key]:
                self._candidate[key] = raw
                self._changed_at[key] = now_ms
            if raw != stable and now_ms - self._changed_at[key] >= self.debounce_ms:
                if raw:
                    self.pressed.add(key)
                else:
                    self.pressed.discard(key)
                events.append((key, raw))
        return tuple(events)
