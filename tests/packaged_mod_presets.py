"""Prove the frozen backend rejects an updated optional mod before world startup."""
from pathlib import Path
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import mod_presets
import platform_win
import session_store as store
import world


def main():
    if any(platform_win.find_processes(name) for name in ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe')):
        raise RuntimeError('Close the game/server before this pre-launch rejection fixture')
    executable = (ROOT/'launcher-app/backend/PREDATEBackend.exe').resolve()
    report = {'passed':False, 'backend_sha256':store.digest(executable)}
    with tempfile.TemporaryDirectory(prefix='predate-packaged-mods-') as directory:
        root = Path(directory)
        mod = root/'extra'
        (mod/'addons').mkdir(parents=True)
        pbo = mod/'addons/fixture.pbo'
        pbo.write_bytes(b'original fixture content')
        info = {'schema':1, 'id':'op-fixture', 'name':'Disposable fixture', 'extra_mods':[str(mod)], 'kind':'operation', 'mod_lock':mod_presets.capture([str(mod)])}
        saved = root/'worlds/op-fixture/profile/PredateProgress.json'
        store.atomic_json(saved.parent.parent/'operation.json', info)
        saved.parent.mkdir(exist_ok=True)
        saved.write_bytes(b'{"Selected":true,"PulseRemaining":38}')
        original = saved.read_bytes()
        pbo.write_bytes(b'updated fixture content')
        environment = dict(os.environ, PREDATE_ROOT=str(root))
        log = (root/'bridge.log').open('w')
        bridge = subprocess.Popen([str(executable), 'bridge'], env=environment, cwd=root,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log, text=True,
            creationflags=subprocess.CREATE_NO_WINDOW)
        sequence = 0
        def call(method, params):
            nonlocal sequence
            sequence += 1
            bridge.stdin.write(json.dumps({'id':sequence, 'method':method, 'params':params})+'\n')
            bridge.stdin.flush()
            response = json.loads(bridge.stdout.readline())
            assert response['ok'], response
            return response['result']
        try:
            overview = call('overview', {})
            assert [kit['id'] for kit in overview['loadouts']] == ['m4a1','ak74'], overview['loadouts']
            assert not (root/'source/Predate/data/loadouts.json').exists()
            report['embedded_starting_kit_catalog'] = True
            request = call('action', {'action':'continue', 'profile':'op-fixture'})['requestId']
            deadline = time.monotonic()+20
            result = None
            while time.monotonic()<deadline:
                result = call('result', {'requestId':request})
                if result: break
                time.sleep(.1)
            assert result and not result['ok'] and 'Mod content changed' in result['error'], result
            assert saved.read_bytes() == original
            assert not (root/'worlds/op-fixture/server-process.json').exists()
            assert not (root/'worlds/op-fixture/last-session.json').exists()
            assert not (root/'config/launcher-settings.local.json').exists()
            report.update(passed=True, changed_mod_rejected_before_start=True, saved_bytes_retained=True,
                          user_preferences_untouched=True, message=result['error'])
        finally:
            bridge.stdin.close()
            bridge.wait(timeout=10)
            log.close()
            manager = store.read_json(root/'session-runtime/manager.json', {})
            if manager and world.process_identity(manager['pid']) == manager:
                assert Path(manager['executable']).resolve() == executable
                assert not (root/'session-runtime/active-session.json').exists()
                os.kill(manager['pid'], signal.SIGTERM)
                until = time.monotonic()+10
                while world.process_identity(manager['pid']) == manager and time.monotonic()<until:
                    time.sleep(.1)
                assert world.process_identity(manager['pid']) != manager
            # The one-file bootloader closes inherited log handles just after
            # its child exits. Wait for that close before TemporaryDirectory
            # removes this explicitly owned fixture.
            target = root/'session-runtime/supervisor.log'
            assert target.resolve().is_relative_to(root.resolve())
            until = time.monotonic()+10
            while target.exists():
                try: target.unlink()
                except PermissionError:
                    if time.monotonic() >= until: raise
                    time.sleep(.1)
        store.atomic_json(ROOT/'build/packaged-mod-presets-results.json', report)
        print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
