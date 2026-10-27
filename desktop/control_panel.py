#!/usr/bin/env python3
"""Windows control panel for a Raspberry Pi running Custom Doom."""
from __future__ import annotations

import json
import os
from pathlib import Path
import threading

import customtkinter as ctk
import paramiko


BG = '#0d1015'
PANEL = '#171c24'
PANEL_ALT = '#1e2631'
BORDER = '#303947'
TEXT = '#f3f4f1'
MUTED = '#aab3c0'
ACCENT = '#ef8b45'
GREEN = '#79d3a5'
BUTTONS = {'Wheel press': 2, 'Rear side button': 5, 'Front side button': 6}


class PiConnection:
    def __init__(self):
        self.client = None
        self.lock = threading.Lock()

    def connect(self, host, user, password):
        client = paramiko.SSHClient()
        client.load_system_host_keys()
        known = Path.home() / '.ssh' / 'known_hosts'
        if known.is_file():
            client.load_host_keys(str(known))
        client.set_missing_host_key_policy(paramiko.RejectPolicy())
        try:
            client.connect(hostname=host, username=user, password=password or None,
                           timeout=8, banner_timeout=8, auth_timeout=8,
                           allow_agent=True, look_for_keys=True)
        except Exception:
            client.close()
            raise
        with self.lock:
            if self.client:
                self.client.close()
            self.client = client

    def command(self, action, payload=None):
        if action not in {'status', 'start', 'stop', 'apply'}:
            raise ValueError('Unknown action.')
        with self.lock:
            if not self.client:
                raise RuntimeError('Connect to your Raspberry Pi first.')
            stdin, stdout, stderr = self.client.exec_command(
                f'python3 "$HOME/Custom-Doom/scripts/control_api.py" {action}', timeout=25)
            if payload is not None:
                stdin.write(json.dumps(payload))
                stdin.flush()
            stdin.channel.shutdown_write()
            raw = stdout.read().decode('utf-8', 'replace')
            error = stderr.read().decode('utf-8', 'replace').strip()
            try:
                response = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise RuntimeError('The Pi controller is unavailable. Update the project on the Pi. ' + error[:250]) from exc
            if not response.get('ok'):
                raise RuntimeError(response.get('error', 'The Pi could not complete that action.'))
            return response['data']

    def close(self):
        with self.lock:
            if self.client:
                self.client.close()
                self.client = None


def preferences_path():
    return Path(os.environ.get('APPDATA', str(Path.home()))) / 'CustomDoom' / 'connection.json'


def read_preferences():
    try:
        value = json.loads(preferences_path().read_text(encoding='utf-8'))
        return {'host': str(value.get('host', 'gamingconsole.local')), 'user': str(value.get('user', 'admin'))}
    except (OSError, ValueError, TypeError):
        return {'host': 'gamingconsole.local', 'user': 'admin'}


def save_preferences(host, user):
    path = preferences_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({'host': host, 'user': user}, indent=2), encoding='utf-8')


class ControlPanel(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title('Custom Doom Control')
        self.geometry('1120x760')
        self.minsize(980, 650)
        self.configure(fg_color=BG)
        self.connection = PiConnection()
        self.connected = False
        self.busy = False
        self.running = False
        self.preferences = read_preferences()
        self.protocol('WM_DELETE_WINDOW', self.close)
        self.make_ui()

    def label(self, parent, text, size=13, color=TEXT, weight='normal', **kw):
        return ctk.CTkLabel(parent, text=text, text_color=color,
                            font=ctk.CTkFont(family='Segoe UI', size=size, weight=weight), **kw)

    def card(self, parent):
        return ctk.CTkFrame(parent, fg_color=PANEL, border_color=BORDER, border_width=1, corner_radius=14)

    def entry(self, parent, placeholder='', show=None):
        return ctk.CTkEntry(parent, placeholder_text=placeholder, show=show, height=38,
                            fg_color=BG, border_color=BORDER, text_color=TEXT, corner_radius=8)

    def button(self, parent, text, command, primary=False):
        return ctk.CTkButton(parent, text=text, command=command, height=40, corner_radius=8,
                             fg_color=ACCENT if primary else PANEL_ALT,
                             hover_color='#dc7936' if primary else '#344052',
                             text_color='#15100c' if primary else TEXT,
                             font=ctk.CTkFont(family='Segoe UI', size=13, weight='bold'))

    def make_ui(self):
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        header = ctk.CTkFrame(self, fg_color='transparent')
        header.grid(row=0, column=0, sticky='ew', padx=30, pady=(23, 16))
        header.grid_columnconfigure(0, weight=1)
        self.label(header, 'CUSTOM DOOM', 25, weight='bold', anchor='w').grid(row=0, column=0, sticky='w')
        self.label(header, 'PI CONTROL  /  COMMAND CENTER', 11, MUTED, weight='bold', anchor='w').grid(row=1, column=0, sticky='w', pady=(1, 0))
        self.connection_badge = self.label(header, '●  OFFLINE', 12, MUTED, weight='bold')
        self.connection_badge.grid(row=0, column=1, rowspan=2, sticky='e')

        body = ctk.CTkFrame(self, fg_color='transparent')
        body.grid(row=1, column=0, sticky='nsew', padx=30, pady=(0, 14))
        body.grid_columnconfigure(0, minsize=305, weight=0)
        body.grid_columnconfigure(1, weight=1)
        body.grid_rowconfigure(0, weight=1)

        left = ctk.CTkFrame(body, fg_color='transparent')
        left.grid(row=0, column=0, sticky='nsew', padx=(0, 15))
        left.grid_columnconfigure(0, weight=1)
        connect = self.card(left)
        connect.grid(row=0, column=0, sticky='ew')
        connect.grid_columnconfigure(0, weight=1)
        self.label(connect, 'CONNECT TO PI', 12, ACCENT, 'bold', anchor='w').grid(row=0, column=0, sticky='ew', padx=18, pady=(17, 4))
        self.label(connect, 'Use the Pi on your local network.', 12, MUTED, anchor='w').grid(row=1, column=0, sticky='ew', padx=18, pady=(0, 12))
        for row, caption in ((2, 'Address'), (4, 'Username'), (6, 'Password')):
            self.label(connect, caption, 12, MUTED, anchor='w').grid(row=row, column=0, sticky='ew', padx=18, pady=(3, 2))
        self.host = self.entry(connect, 'gamingconsole.local')
        self.host.insert(0, self.preferences['host'])
        self.host.grid(row=3, column=0, sticky='ew', padx=18)
        self.user = self.entry(connect, 'admin')
        self.user.insert(0, self.preferences['user'])
        self.user.grid(row=5, column=0, sticky='ew', padx=18)
        self.password = self.entry(connect, 'Pi password', show='•')
        self.password.grid(row=7, column=0, sticky='ew', padx=18)
        self.connect_button = self.button(connect, 'CONNECT', self.connect, True)
        self.connect_button.grid(row=8, column=0, sticky='ew', padx=18, pady=(15, 7))
        self.label(connect, 'Password is not saved.', 11, MUTED, anchor='w').grid(row=9, column=0, sticky='ew', padx=18, pady=(0, 15))

        status = self.card(left)
        status.grid(row=1, column=0, sticky='ew', pady=(14, 0))
        status.grid_columnconfigure(1, weight=1)
        self.label(status, 'SYSTEM STATUS', 12, ACCENT, 'bold', anchor='w').grid(row=0, column=0, columnspan=2, sticky='ew', padx=18, pady=(17, 9))
        for row, title in ((1, 'Game'), (2, 'Engine'), (3, 'Pi temperature')):
            self.label(status, title, 12, MUTED, anchor='w').grid(row=row, column=0, sticky='w', padx=18, pady=4)
        self.game_status = self.label(status, '—', 12, TEXT, 'bold', anchor='e')
        self.engine_status = self.label(status, '—', 12, TEXT, 'bold', anchor='e')
        self.temp_status = self.label(status, '—', 12, TEXT, 'bold', anchor='e')
        for row, widget in ((1, self.game_status), (2, self.engine_status), (3, self.temp_status)):
            widget.grid(row=row, column=1, sticky='e', padx=18, pady=4)
        actions = ctk.CTkFrame(status, fg_color='transparent')
        actions.grid(row=4, column=0, columnspan=2, sticky='ew', padx=18, pady=(12, 17))
        actions.grid_columnconfigure((0, 1), weight=1)
        self.start_button = self.button(actions, 'LAUNCH GAME', lambda: self.run_command('start'), True)
        self.start_button.grid(row=0, column=0, sticky='ew', padx=(0, 5))
        self.stop_button = self.button(actions, 'STOP', lambda: self.run_command('stop'))
        self.stop_button.grid(row=0, column=1, sticky='ew', padx=(5, 0))

        right = ctk.CTkScrollableFrame(body, fg_color=PANEL, border_color=BORDER, border_width=1, corner_radius=14)
        right.grid(row=0, column=1, sticky='nsew')
        right.grid_columnconfigure(0, weight=1)
        self.label(right, 'GAME SETTINGS', 12, ACCENT, 'bold', anchor='w').grid(row=0, column=0, sticky='ew', padx=23, pady=(19, 3))
        self.label(right, 'Make it yours.', 26, TEXT, 'bold', anchor='w').grid(row=1, column=0, sticky='ew', padx=23)
        self.label(right, 'Adjust the controls and view on your Raspberry Pi.', 13, MUTED, anchor='w').grid(row=2, column=0, sticky='ew', padx=23, pady=(1, 16))

        controls = ctk.CTkFrame(right, fg_color=PANEL_ALT, corner_radius=12)
        controls.grid(row=3, column=0, sticky='ew', padx=20, pady=(0, 13))
        controls.grid_columnconfigure(1, weight=1)
        self.label(controls, 'MOUSE BUTTONS', 12, TEXT, 'bold', anchor='w').grid(row=0, column=0, columnspan=2, sticky='ew', padx=18, pady=(15, 5))
        self.label(controls, 'Left click fires · scroll changes weapons', 12, MUTED, anchor='w').grid(row=1, column=0, columnspan=2, sticky='ew', padx=18, pady=(0, 8))
        self.binding_vars = {}
        for row, (key, title, default) in enumerate((('forward', 'Move forward', 'Front side button'), ('backward', 'Move backward', 'Rear side button'), ('use', 'Use / open', 'Wheel press')), 2):
            self.label(controls, title, 13, TEXT, anchor='w').grid(row=row, column=0, sticky='w', padx=18, pady=5)
            var = ctk.StringVar(value=default)
            menu = ctk.CTkOptionMenu(controls, variable=var, values=list(BUTTONS), fg_color='#303b49', button_color='#435268', button_hover_color='#536680', height=34)
            menu.grid(row=row, column=1, sticky='ew', padx=(10, 18), pady=5)
            self.binding_vars[key] = var
        self.label(controls, 'Choose a different button for each action.', 11, MUTED, anchor='w').grid(row=5, column=0, columnspan=2, sticky='ew', padx=18, pady=(4, 14))

        view = ctk.CTkFrame(right, fg_color=PANEL_ALT, corner_radius=12)
        view.grid(row=4, column=0, sticky='ew', padx=20, pady=(0, 14))
        view.grid_columnconfigure(0, weight=1)
        self.label(view, 'AIM & DISPLAY', 12, TEXT, 'bold', anchor='w').grid(row=0, column=0, sticky='ew', padx=18, pady=(15, 8))
        self.mouselook = ctk.BooleanVar(value=True)
        self.freeaim = ctk.BooleanVar(value=True)
        self.hires = ctk.BooleanVar(value=False)
        for row, (title, var) in enumerate((('Mouse look up and down', self.mouselook), ('Vertical aiming', self.freeaim), ('High resolution rendering', self.hires)), 1):
            ctk.CTkSwitch(view, text=title, variable=var, progress_color=ACCENT,
                          button_color=TEXT, text_color=TEXT, font=ctk.CTkFont(family='Segoe UI', size=13)).grid(row=row, column=0, sticky='w', padx=18, pady=6)
        speed_header = ctk.CTkFrame(view, fg_color='transparent')
        speed_header.grid(row=4, column=0, sticky='ew', padx=18, pady=(13, 0))
        speed_header.grid_columnconfigure(0, weight=1)
        self.label(speed_header, 'Mouse sensitivity', 13, TEXT, anchor='w').grid(row=0, column=0, sticky='w')
        self.speed_value = self.label(speed_header, '5', 13, ACCENT, 'bold', anchor='e')
        self.speed_value.grid(row=0, column=1, sticky='e')
        self.sensitivity = ctk.CTkSlider(view, from_=0, to=9, number_of_steps=9, progress_color=ACCENT,
                                         button_color=ACCENT, button_hover_color='#f4a76f', command=self.on_speed)
        self.sensitivity.set(5)
        self.sensitivity.grid(row=5, column=0, sticky='ew', padx=18, pady=(10, 18))

        self.apply_button = self.button(right, 'APPLY SETTINGS  ·  RESTART IF RUNNING', self.apply, True)
        self.apply_button.grid(row=5, column=0, sticky='ew', padx=20, pady=(0, 7))
        self.label(right, 'Applying while playing restarts the current level.', 11, MUTED, anchor='w').grid(row=6, column=0, sticky='ew', padx=23, pady=(0, 16))

        footer = ctk.CTkFrame(self, fg_color='transparent')
        footer.grid(row=2, column=0, sticky='ew', padx=30, pady=(0, 15))
        footer.grid_columnconfigure(0, weight=1)
        self.message = self.label(footer, 'Connect to see your Pi and game status.', 12, MUTED, anchor='w')
        self.message.grid(row=0, column=0, sticky='ew')
        self.refresh_button = self.button(footer, 'REFRESH STATUS', lambda: self.run_command('status'))
        self.refresh_button.configure(width=145, height=32)
        self.refresh_button.grid(row=0, column=1, sticky='e')
        self.set_buttons()

    def on_speed(self, value):
        self.speed_value.configure(text=str(round(value)))

    def set_buttons(self):
        self.connect_button.configure(state='disabled' if self.busy else 'normal')
        for widget in (self.start_button, self.stop_button, self.apply_button, self.refresh_button):
            widget.configure(state='normal' if self.connected and not self.busy else 'disabled')

    def run_background(self, operation, done):
        if self.busy:
            return
        self.busy = True
        self.set_buttons()
        def worker():
            try:
                result, error = operation(), None
            except Exception as exc:
                result, error = None, str(exc)
            self.after(0, lambda: self.finish(done, result, error))
        threading.Thread(target=worker, daemon=True).start()

    def finish(self, done, result, error):
        self.busy = False
        self.set_buttons()
        if error:
            self.message.configure(text=error, text_color='#f2a0a0')
            return
        done(result)

    def connect(self):
        host, user, password = self.host.get().strip(), self.user.get().strip(), self.password.get()
        if not host or not user:
            self.message.configure(text='Enter the Pi address and username.', text_color='#f2a0a0')
            return
        self.connected = False
        self.connection_badge.configure(text='●  OFFLINE', text_color=MUTED)
        self.set_buttons()
        self.password.delete(0, 'end')
        self.message.configure(text='Connecting to the Pi…', text_color=MUTED)
        def operation():
            self.connection.connect(host, user, password)
            result = self.connection.command('status')
            try:
                save_preferences(host, user)
            except OSError:
                pass
            return result
        def done(result):
            self.connected = True
            self.connection_badge.configure(text='●  CONNECTED', text_color=GREEN)
            self.set_buttons()
            self.show_status(result, load_settings=True)
        self.run_background(operation, done)

    def show_status(self, data, load_settings=False):
        self.running = data['running']
        self.game_status.configure(text='RUNNING' if self.running else 'STOPPED', text_color=GREEN if self.running else MUTED)
        self.engine_status.configure(text=data.get('engine') or '—')
        temp = data.get('temperature')
        self.temp_status.configure(text=f'{temp:.1f} °C' if temp is not None else '—')
        self.message.configure(text=f"Connected to {data['host']}  ·  {'Game is running' if self.running else 'Ready to launch'}", text_color=MUTED)
        if load_settings:
            settings = data['settings']
            reverse = {value: name for name, value in BUTTONS.items()}
            for key, var in self.binding_vars.items():
                var.set(reverse.get(settings[key], 'Wheel press'))
            self.mouselook.set(settings['mouselook'])
            self.freeaim.set(settings['freeaim'])
            self.hires.set(settings['hires'])
            self.sensitivity.set(settings['sensitivity'])
            self.on_speed(settings['sensitivity'])

    def run_command(self, action):
        self.message.configure(text={'status': 'Refreshing status…', 'start': 'Starting the game…', 'stop': 'Stopping the game…'}[action], text_color=MUTED)
        self.run_background(lambda: self.connection.command(action), lambda data: self.show_status(data, load_settings=action == 'status'))

    def apply(self):
        settings = {key: BUTTONS[var.get()] for key, var in self.binding_vars.items()}
        if len(set(settings.values())) != 3:
            self.message.configure(text='Choose a different button for each mouse action.', text_color='#f2a0a0')
            return
        settings.update(mouselook=self.mouselook.get(), freeaim=self.freeaim.get(),
                        hires=self.hires.get(), sensitivity=round(self.sensitivity.get()))
        self.message.configure(text='Applying settings to the Pi…', text_color=MUTED)
        self.run_background(lambda: self.connection.command('apply', settings), lambda data: self.show_status(data, load_settings=True))

    def close(self):
        # Avoid blocking the UI if an SSH operation is still ending.
        threading.Thread(target=self.connection.close, daemon=True).start()
        self.destroy()


if __name__ == '__main__':
    ctk.set_appearance_mode('dark')
    ControlPanel().mainloop()
