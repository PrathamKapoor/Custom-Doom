#!/usr/bin/env python3
"""Small translucent X11 touch controls for Chocolate Doom."""
import os
import select
import sys
import tkinter as tk

from touch_controls import TouchControls, calibrate, find_touch_device


RAW_X_TOP, RAW_X_BOTTOM = 3932, 300
RAW_Y_LEFT, RAW_Y_RIGHT = 294, 3801
KEYS = {"forward": ("KEY_UP",), "back": ("KEY_DOWN",),
        "strafe_left": ("KEY_LEFTALT", "KEY_LEFT"), "strafe_right": ("KEY_LEFTALT", "KEY_RIGHT"),
        "fire": ("KEY_LEFTCTRL",), "use": ("KEY_SPACE",), "map": ("KEY_TAB",)}


def discover_device(evdev):
    devices = []
    for path in evdev.list_devices():
        try:
            device = evdev.InputDevice(path)
            caps = device.capabilities(absinfo=False)
            absolute = evdev.ecodes.EV_ABS in caps and evdev.ecodes.ABS_X in caps[evdev.ecodes.EV_ABS] and evdev.ecodes.ABS_Y in caps[evdev.ecodes.EV_ABS]
            devices.append({"path": path, "name": device.name, "absolute": absolute})
            device.close()
        except OSError:
            continue
    path = find_touch_device(devices, os.environ.get("DOOM_TOUCH_DEVICE"))
    if not path:
        raise RuntimeError("No ADS7846/XPT2046 touchscreen found. Set DOOM_TOUCH_DEVICE to its /dev/input/event path.")
    return evdev.InputDevice(path)


def create_virtual_inputs(evdev):
    """Expose a keyboard and relative mouse that desktop input can identify."""
    codes = evdev.ecodes
    keys = {getattr(codes, code) for mapping in KEYS.values() for code in mapping}
    keys.update(getattr(codes, "KEY_" + chr(letter)) for letter in range(ord("A"), ord("Z") + 1))
    keys.add(codes.KEY_PAUSE)
    keyboard = evdev.UInput(events={codes.EV_KEY: sorted(keys)}, name="Custom Doom Touch Keyboard")
    try:
        mouse = evdev.UInput(events={codes.EV_KEY: [codes.BTN_LEFT],
                                    codes.EV_REL: [codes.REL_X, codes.REL_Y]},
                             name="Custom Doom Touch Mouse")
    except Exception:
        keyboard.close()
        raise
    return keyboard, mouse


class Overlay:
    def __init__(self, controls, ui, mouse_ui, evdev, device):
        self.controls, self.ui, self.mouse_ui, self.evdev, self.device = controls, ui, mouse_ui, evdev, device
        self.root = tk.Tk()
        self.root.withdraw()
        self.root.title("Custom Doom Touch HUD")
        self.root.attributes("-topmost", True)
        self.windows = {}
        self.panel_signatures = {}
        self.last_keys = set()
        self.last_pause_events = controls.pause_events
        self.last_gesture = None
        self.running = True
        self.raw_x = self.raw_y = None
        self.touching = False
        self.root.bind("<Escape>", lambda _e: self.close())
        self.root.after(30, self.tick)
        self.root.after(8, self.poll_input)

    def panel(self, key, title, color, x, y, w, h, draw, state=None):
        signature = (title, color, x, y, w, h, state)
        win = self.windows.get(key)
        if win is None:
            win = tk.Toplevel(self.root, bg="#111820")
            win.overrideredirect(True)
            win.attributes("-topmost", True)
            try:
                win.attributes("-alpha", 0.70)
            except tk.TclError:
                pass
            canvas = tk.Canvas(win, width=w, height=h, bg="#111820", highlightthickness=0)
            canvas.pack()
            self.windows[key] = win
            win.canvas = canvas
        elif self.panel_signatures.get(key) == signature:
            if not win.winfo_viewable():
                win.deiconify()
            return
        win.geometry(f"{w}x{h}+{max(0, x)}+{max(0, y)}")
        win.canvas.delete("all")
        draw(win.canvas, w, h, title, color)
        self.panel_signatures[key] = signature
        if not win.winfo_viewable():
            win.deiconify()

    @staticmethod
    def badge(canvas, w, h, title, color):
        canvas.create_oval(3, 3, w-3, h-3, fill=color, outline="#ffffff", width=2)
        canvas.create_text(w/2, h/2, text=title, fill="white", font=("DejaVu Sans", 9, "bold"), justify="center")

    def render(self):
        if not self.controls.visible:
            for win in self.windows.values(): win.withdraw()
            if self.controls.active and self.controls.paused:
                sw, sh = self.root.winfo_screenwidth(), self.root.winfo_screenheight()
                self.panel("resume", "RESUME", "#4b8798", sw//2-42, sh//2-24, 84, 48, self.badge)
            return
        if "resume" in self.windows:
            self.windows["resume"].withdraw()
        sw, sh = self.root.winfo_screenwidth(), self.root.winfo_screenheight()
        sx, sy = sw/480, sh/320
        self.panel("pause", "Ⅱ", "#263746", 4, 4, 48, 48, self.badge)
        self.panel("map", "MAP", "#263746", sw-54, 4, 48, 48, self.badge)
        self.panel("auto", "AUTO\n" + ("ON" if self.controls.auto_fire else "OFF"), "#9b562f" if self.controls.auto_fire else "#263746", sw-116, 4, 58, 48, self.badge)
        for name, label, color, cx, cy in (("fire", "FIRE", "#b44239", 414, 205), ("use", "USE", "#a17b32", 414, 267)):
            size = round((74 if name == "fire" else 68)*sx)
            active = name in self.controls.held_actions
            self.panel(name, label, "#e2554a" if active else color,
                       round(cx*sx-size/2), round(cy*sy-size/2), size, size, self.badge)
        if self.controls.stick_anchor:
            ax, ay = self.controls.stick_anchor
            size = round(116*sx)
            def stick(canvas,w,h,title,color):
                canvas.create_oval(4,4,w-4,h-4, fill="#24384a", outline="#d7e2ed", width=3)
                canvas.create_oval(w/2-15,h/2-15,w/2+15,h/2+15,fill="#67849b",outline="white",width=2)
                kx,ky=self.controls.stick_knob or (ax,ay)
                ox,oy=(kx-ax)*sx,(ky-ay)*sy
                canvas.coords(2,w/2-15+ox,h/2-15+oy,w/2+15+ox,h/2+15+oy)
            self.panel("stick", "", "#24384a", round(ax*sx-size/2), round(ay*sy-size/2), size, size,
                       stick, state=self.controls.stick_knob)
        elif "stick" in self.windows:
            self.windows["stick"].withdraw()

    def emit(self):
        codes = self.evdev.ecodes
        wanted = {getattr(codes, code) for key in self.controls.keys for code in KEYS[key]}
        previous = {getattr(codes, code) for key in self.last_keys for code in KEYS[key]}
        pause_delta = self.controls.pause_events - self.last_pause_events
        if wanted == previous and pause_delta <= 0 and not self.controls.look_delta:
            return
        for code in previous-wanted: self.ui.write(codes.EV_KEY, code, 0)
        for code in wanted-previous: self.ui.write(codes.EV_KEY, code, 1)
        for _ in range(max(0, pause_delta)):
            self.ui.write(codes.EV_KEY, codes.KEY_PAUSE, 1)
            self.ui.syn()
            self.ui.write(codes.EV_KEY, codes.KEY_PAUSE, 0)
        if self.controls.look_delta:
            self.mouse_ui.write(codes.EV_REL, codes.REL_X, int(self.controls.look_delta*1.5))
            self.mouse_ui.syn()
        self.ui.syn()
        self.controls.look_delta = 0
        self.last_keys = set(self.controls.keys)
        self.last_pause_events = self.controls.pause_events

    def poll_input(self):
        if not self.running: return
        try:
            if select.select([self.device.fd], [], [], 0)[0]:
                for event in self.device.read():
                    if event.type == self.evdev.ecodes.EV_ABS:
                        if event.code == self.evdev.ecodes.ABS_X: self.raw_x = event.value
                        elif event.code == self.evdev.ecodes.ABS_Y: self.raw_y = event.value
                        if self.touching and self.raw_x is not None and self.raw_y is not None:
                            x,y=calibrate(self.raw_x, RAW_X_TOP, RAW_X_BOTTOM, self.raw_y, RAW_Y_LEFT, RAW_Y_RIGHT,
                                          self.root.winfo_screenwidth(), self.root.winfo_screenheight())
                            if self.controls.pointer is None: self.controls.down(x,y)
                            else: self.controls.move(x,y)
                    elif event.type == self.evdev.ecodes.EV_KEY and event.code == self.evdev.ecodes.BTN_TOUCH:
                        if event.value == 0:
                            self.touching = False
                            self.controls.up()
                        elif event.value == 1:
                            self.touching = True
        except OSError:
            self.close()
            return
        self.root.after(8, self.poll_input)

    def tick(self):
        if not self.running: return
        self.emit()
        self.render()
        self.root.after(30, self.tick)

    def close(self):
        self.running = False
        self.controls.cancel()
        self.emit()
        self.root.destroy()


def main():
    try:
        import evdev
        device = discover_device(evdev)
        ui, mouse_ui = create_virtual_inputs(evdev)
    except Exception as exc:
        print(f"Touch HUD unavailable: {exc}", file=sys.stderr)
        return 2
    controls = TouchControls()
    controls.set_game_active(True)
    app = Overlay(controls, ui, mouse_ui, evdev, device)
    try:
        app.root.mainloop()
    except (OSError, tk.TclError):
        pass
    finally:
        controls.cancel()
        app.emit()
        device.close(); ui.close(); mouse_ui.close()
        try: app.root.destroy()
        except tk.TclError: pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
