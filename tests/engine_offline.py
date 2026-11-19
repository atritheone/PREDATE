"""Exercise cached Steam Offline Mode and local DayZ startup without OS network changes.

Steam's existing WantsOfflineMode preference is changed only while Steam is
stopped. Original bytes and launcher settings are restored in finally. No cached
credentials, account identity, authentication or game entitlement data is changed.
"""
from pathlib import Path
import json
import re
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from engine_sessions import Bridge, fixture
import platform_win
import session_store as store
import world

STEAM = Path(r"C:\Program Files (x86)\Steam")


def steam(action):
    subprocess.Popen([str(STEAM / 'steam.exe'), *action], creationflags=subprocess.CREATE_NO_WINDOW)


def stop_steam():
    steam(['-shutdown'])
    until = time.monotonic() + 45
    while platform_win.find_processes('steam.exe') and time.monotonic() < until: time.sleep(.25)
    if platform_win.find_processes('steam.exe'): raise RuntimeError('Steam did not close normally')


def main():
    assert not platform_win.find_processes('DayZ_x64.exe') and not platform_win.find_processes('DayZServer_x64.exe')
    preferences = store.preferences()
    config = STEAM / 'config/loginusers.vdf'
    original = config.read_bytes()
    pattern = rb'("WantsOfflineMode"\s*")0(")'
    offline, count = re.subn(pattern, rb'\g<1>1\2', original)
    if count != 1: raise RuntimeError('Expected exactly one cached online account preference')
    profile = fixture('crashlab-primitive-send-d1612')
    bridge = Bridge()
    report = {'profile': profile, 'passed': False, 'os_network_unchanged': True}
    try:
        stop_steam()
        config.write_bytes(offline)
        steam(['-silent'])
        time.sleep(12)
        assert b'"WantsOfflineMode"' in config.read_bytes()
        flags = re.findall(rb'"WantsOfflineMode"\s*"([01])"', config.read_bytes())
        assert flags == [b'1'], 'Steam did not retain Offline Mode'
        report['steam_offline_preference_active'] = True
        bridge.call('preferences', {'offline': True})
        bridge.wait_action(bridge.action('continue', profile=profile))
        bridge.wait_phase({'playing'}, 180)
        assert re.search(r'BattlEye\s*=\s*0;', (world.WORLD/'serverDZ.cfg').read_text())
        assert world.native_status()['Players'] == 1
        report['offline_local_connection'] = True
        print('PASS: local DayZ connected with Steam Offline Mode', flush=True)
        bridge.wait_action(bridge.action('quit'))
        bridge.wait_phase({'suspended'})
        report['offline_save_quit'] = True
        report['passed'] = True
    except Exception as error:
        report['failure'] = str(error)
        report['client_login_timeout_seen'] = any('WaitAuthPlayerLoginState' in path.read_text(errors='replace')
            for path in (world.WORLD/'client').glob('*.RPT'))
        raise
    finally:
        try:
            if world.running() or platform_win.find_processes('DayZ_x64.exe'):
                bridge.wait_action(bridge.action('quit'), 90)
        finally:
            try: bridge.close()
            finally:
                store.save_preferences({k: v for k, v in preferences.items() if k != 'schema'})
                if not platform_win.find_processes('DayZ_x64.exe') and not platform_win.find_processes('DayZServer_x64.exe'):
                    stop_steam()
                    config.write_bytes(original)
                    steam(['-silent'])
                    report['steam_preferences_restored'] = True
                store.atomic_json(store.ROOT/'build/engine-offline-results.json', report)
                print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
