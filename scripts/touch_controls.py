"""Pure, hardware-independent touch mapping for the 480x320 overlay."""
from __future__ import annotations

from dataclasses import dataclass
from math import hypot
from typing import Iterable


def calibrate(raw_x: int, raw_x_top: int, raw_x_bottom: int,
              raw_y: int, raw_y_left: int, raw_y_right: int,
              width: int = 480, height: int = 320) -> tuple[int, int]:
    """Map ADS7846 raw coordinates to screen coordinates with clamping."""
    def scale(value: int, start: int, end: int, size: int) -> int:
        if start == end:
            return 0
        return round(max(0.0, min(1.0, (value - start) / (end - start))) * (size - 1))
    return (scale(raw_y, raw_y_left, raw_y_right, width),
            scale(raw_x, raw_x_top, raw_x_bottom, height))


def find_touch_device(devices: Iterable[dict], override: str | None = None) -> str | None:
    if override:
        return override
    for device in devices:
        name = str(device.get("name", "")).lower()
        if device.get("absolute") and any(tag in name for tag in ("ads7846", "xpt2046", "touchscreen")):
            return str(device.get("path"))
    return None


@dataclass
class TouchControls:
    width: int = 480
    height: int = 320
    deadzone: float = 0.12
    active: bool = False
    paused: bool = False
    auto_fire: bool = False
    pointer: tuple[int, int] | None = None
    stick_anchor: tuple[int, int] | None = None
    stick_knob: tuple[int, int] | None = None
    movement: tuple[float, float] = (0.0, 0.0)
    look_delta: int = 0
    held_actions: set[str] | None = None
    gesture: str | None = None
    pause_events: int = 0

    def __post_init__(self):
        self.held_actions = set()

    @property
    def visible(self) -> bool:
        return self.active and not self.paused

    def set_game_active(self, active: bool) -> None:
        self.active = active
        if not active:
            self.cancel()
            self.paused = False

    def pause(self) -> None:
        if self.active:
            self.cancel()
            self.paused = True
            self.pause_events += 1

    def resume(self) -> None:
        if self.active:
            self.paused = False
            self.pause_events += 1

    def _button(self, x: int, y: int) -> str | None:
        sx, sy = self.width / 480, self.height / 320
        spots = {
            "pause": (34 * sx, 30 * sy), "map": (446 * sx, 30 * sy),
            "fire": (414 * sx, 205 * sy), "use": (414 * sx, 267 * sy),
            "auto": (350 * sx, 30 * sy),
        }
        radii = {"pause": 24, "map": 24, "fire": 37, "use": 34, "auto": 27}
        for name, (cx, cy) in spots.items():
            if hypot((x-cx)/sx, (y-cy)/sy) <= radii[name]:
                return name
        return None

    def down(self, x: int, y: int) -> None:
        self.look_delta = 0
        self.pointer = (x, y)
        if not self.visible:
            if self.active and self.paused:
                self.resume()
            return
        button = self._button(x, y)
        if button:
            self.gesture = button
            if button == "auto":
                self.auto_fire = not self.auto_fire
            elif button == "pause":
                self.pause()
            elif button == "map":
                self.held_actions.add("map")
            else:
                self.held_actions.add(button)
            return
        if x < self.width * 0.48 and y > self.height * 0.28:
            self.gesture = "move"
            self.stick_anchor = self.pointer
            self.stick_knob = self.pointer
        elif x >= self.width * 0.48:
            self.gesture = "look"

    def move(self, x: int, y: int) -> None:
        if self.gesture == "move" and self.stick_anchor:
            ax, ay = self.stick_anchor
            dx, dy = x - ax, ay - y
            radius = max(1.0, min(self.width, self.height) * 0.20)
            magnitude = hypot(dx, dy)
            if magnitude > radius:
                dx, dy = dx * radius / magnitude, dy * radius / magnitude
            nx, ny = dx / radius, dy / radius
            self.movement = (0.0 if abs(nx) < self.deadzone else nx,
                             0.0 if abs(ny) < self.deadzone else ny)
            self.stick_knob = (round(ax + dx), round(ay - dy))
        elif self.gesture == "look" and self.pointer:
            self.look_delta += x - self.pointer[0]
        self.pointer = (x, y)
        self._sync_auto_fire()

    def up(self) -> None:
        if self.gesture in {"fire", "use", "map"}:
            self.held_actions.discard(self.gesture)
        self.gesture = None
        self.pointer = None
        self.stick_anchor = self.stick_knob = None
        self.movement = (0.0, 0.0)
        self.held_actions.discard("fire_auto")

    def cancel(self) -> None:
        self.gesture = None
        self.pointer = None
        self.stick_anchor = self.stick_knob = None
        self.movement = (0.0, 0.0)
        self.look_delta = 0
        self.held_actions.clear()

    def _sync_auto_fire(self) -> None:
        if self.auto_fire and self.gesture == "move":
            self.held_actions.add("fire_auto")
        else:
            self.held_actions.discard("fire_auto")

    @property
    def keys(self) -> set[str]:
        keys = set()
        x, y = self.movement
        if y > 0: keys.add("forward")
        if y < 0: keys.add("back")
        if x < 0: keys.add("strafe_left")
        if x > 0: keys.add("strafe_right")
        for action in self.held_actions:
            if action in {"fire", "fire_auto"}: keys.add("fire")
            elif action == "use": keys.add("use")
            elif action == "map": keys.add("map")
        return keys
