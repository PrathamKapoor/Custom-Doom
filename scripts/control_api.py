"""Pi-side commands for the laptop controller; JSON in and out over SSH."""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

from launch_config import prepare_crispy_config

REPO = Path(__file__).resolve().parent.parent


def validate_settings(data):
    if not isinstance(data, dict):
        raise ValueError('Settings must be an object.')
    result = {}
    for key in ('forward', 'backward', 'use'):
        value = data.get(key)
        if type(value) is not int or value not in (2, 5, 6):
            raise ValueError('Choose a valid mouse button for each action.')
        result[key] = value
    if len({result[k] for k in ('forward', 'backward', 'use')}) != 3:
        raise ValueError('Forward, backward and use must have different buttons.')
    for key in ('mouselook', 'freeaim', 'hires'):
        value = data.get(key)
        if type(value) is not bool:
            raise ValueError(f'{key} must be on or off.')
        result[key] = value
    sensitivity = data.get('sensitivity')
    if type(sensitivity) is not int or not 0 <= sensitivity <= 9:
        raise ValueError('Mouse sensitivity must be between 0 and 9.')
    result['sensitivity'] = sensitivity
    return result


def update_config(path, changes):
    contents = path.read_text(encoding='utf-8') if path.exists() else ''
    for key, value in changes.items():
        pattern = rf'(?m)^{re.escape(key)}\s+[^\n\r]*$'
        line = f'{key} {value}'
        if re.search(pattern, contents):
            contents = re.sub(pattern, line, contents)
        else:
            contents = contents.rstrip('\n') + '\n' + line + '\n'
    path.write_text(contents, encoding='utf-8')


def config_values(path):
    if not path.is_file():
        return {}
    result = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        pieces = line.split(None, 1)
        if len(pieces) == 2:
            result[pieces[0]] = pieces[1].strip()
    return result


def read_settings(home):
    state = home / '.doom-touch'
    default = config_values(state / 'crispy-default.cfg')
    extra = config_values(state / 'crispy-extra.cfg')
    def number(source, name, default_value):
        try:
            return int(source.get(name, default_value))
        except ValueError:
            return default_value
    return {'forward': number(default, 'mouseb_forward', 6),
            'backward': number(extra, 'mouseb_backward', 5),
            'use': number(extra, 'mouseb_use', 2),
            'mouselook': bool(number(extra, 'crispy_mouselook', 1)),
            'freeaim': bool(number(extra, 'crispy_freeaim', 1)),
            'hires': bool(number(extra, 'crispy_hires', 0)),
            'sensitivity': max(0, min(9, number(default, 'mouse_sensitivity', 5)))}


def game_processes():
    result = subprocess.run(['ps', '-eo', 'pid=,comm='], capture_output=True, text=True, check=True)
    games = []
    for line in result.stdout.splitlines():
        fields = line.split(None, 1)
        if len(fields) == 2 and fields[1] in {'crispy-doom', 'chocolate-doom'}:
            games.append((int(fields[0]), fields[1]))
    return games


def status():
    games = game_processes()
    try:
        temp = round(int(Path('/sys/class/thermal/thermal_zone0/temp').read_text()) / 1000, 1)
    except (OSError, ValueError):
        temp = None
    return {'host': os.uname().nodename, 'running': bool(games),
            'engine': games[0][1] if games else None, 'temperature': temp,
            'settings': read_settings(Path.home())}


def stop_game():
    games = game_processes()
    for pid, _ in games:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 6
    while game_processes() and time.monotonic() < deadline:
        time.sleep(.2)
    if game_processes():
        raise RuntimeError('The game did not stop. Try closing it on the Pi.')
    # Allow the launcher to release its helpers and restore mouse mappings.
    time.sleep(2)


def start_game():
    if game_processes():
        raise RuntimeError('The game is already running.')
    state = Path.home() / '.doom-touch'
    state.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env['DISPLAY'] = ':0'
    env['XAUTHORITY'] = str(Path.home() / '.Xauthority')
    with (state / 'controller-launch.log').open('a', encoding='utf-8') as log:
        proc = subprocess.Popen([str(REPO / 'scripts' / 'launch.sh')], cwd=REPO,
                                env=env, stdin=subprocess.DEVNULL, stdout=log,
                                stderr=subprocess.STDOUT, start_new_session=True)
    time.sleep(2)
    if proc.poll() is not None:
        raise RuntimeError('The game could not start. Check controller-launch.log on the Pi.')


def apply_settings(data):
    settings = validate_settings(data)
    main, extra = prepare_crispy_config(Path.home())
    update_config(main, {'mouseb_forward': settings['forward'], 'mouse_sensitivity': settings['sensitivity']})
    update_config(extra, {'mouseb_backward': settings['backward'], 'mouseb_use': settings['use'],
                          'crispy_freelook': int(settings['mouselook']),
                          'crispy_mouselook': int(settings['mouselook']),
                          'crispy_freeaim': int(settings['freeaim']), 'crispy_hires': int(settings['hires'])})


def main():
    try:
        command = sys.argv[1] if len(sys.argv) > 1 else 'status'
        if command == 'status':
            result = status()
        elif command == 'start':
            start_game()
            result = status()
        elif command == 'stop':
            stop_game()
            result = status()
        elif command == 'apply':
            data = json.load(sys.stdin)
            validate_settings(data)
            was_running = bool(game_processes())
            if was_running:
                stop_game()
            apply_settings(data)
            if was_running:
                start_game()
            result = status()
        else:
            raise ValueError('Unknown controller command.')
        print(json.dumps({'ok': True, 'data': result}))
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        print(json.dumps({'ok': False, 'error': str(exc)}))
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
