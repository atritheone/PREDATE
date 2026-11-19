"""Measure the real compass at a selected display size, using a disposable hunt."""
from pathlib import Path
import argparse
import json
import os
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import Bridge, native_evidence
from engine_loadouts import close_client
import platform_win
import session
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--percent',type=int,choices=(80,100,150),required=True)
    parser.add_argument('--high-contrast',action='store_true')
    parser.add_argument('--unmanaged-720p',action='store_true',help='Also exercise the source launch path at an explicit disposable 1280x720 viewport')
    args = parser.parse_args()
    assert not any(platform_win.find_processes(name) for name in
                   ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    assert not session.manager_alive(), 'Retain any existing supervisor; this proof requires a fresh frozen service'
    preference_path = ROOT/'config/launcher-settings.local.json'
    original_preferences = store.preferences()
    original_document = store.read_json(preference_path,{})
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-display-'+uuid.uuid4().hex[:6]
    info = store.create_operation('Compass display validation',profile=profile,extra_mods=[])
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json',info)
    config = store.read_json(world.PROFILE/'PredateConfig.json')
    config.update(BotStart=[5500,0,6500],ThreatDistance=150)
    store.atomic_json(world.PROFILE/'PredateConfig.json',config)
    client = world.WORLD/'client'
    client.mkdir(exist_ok=True)
    (client/'PredatePresentationValidation.txt').write_text('Disposable compass validation only\n')
    bridge = Bridge(candidate=True,additional_env={'PREDATE_PRESENTATION_TEST':'1'})
    report = dict(profile=profile,candidate=True,passed=False,packaged_launch=not args.unmanaged_720p,
                  backend_sha256=store.digest(ROOT/'launcher-app/backend/PREDATEBackend.exe'))
    report.update(native_evidence())
    nonce = ''
    try:
        bridge.call('preferences',{'receiver_scale':args.percent,'receiver_high_contrast':args.high_contrast})
        if args.unmanaged_720p:
            world.start()
            nonce = store.read_json(world.STATE)['session_id']
            world.launch_client(nonce,extra_args=['-window','-x=1280','-y=720','-predatePresentationValidation'])
        else:
            bridge.wait_action(bridge.action('continue',profile=profile))
            bridge.wait_phase({'playing'})
            owner = session.read_native(session.MANAGER)
            report['supervisor_sha256'] = store.digest(Path(owner['executable']))
            assert report['supervisor_sha256'] == report['backend_sha256']
        path = client/'PresentationProbe.json'
        deadline = time.monotonic()+100
        result = {}
        while time.monotonic()<deadline:
            assert world.running(), 'Display-validation server exited'
            failure = session.read_native(client/'PredateFailure.json')
            assert not failure, failure.get('message')
            result = session.read_native(path)
            if result.get('Checks'): break
            time.sleep(.1)
        report['native_ui'] = result
        assert result.get('Checks',0) >= 24 and result['Failures'] == 0, result
        assert result['ConfiguredPercent'] == args.percent and bool(result['HighContrast']) == args.high_contrast
        assert result['RenderedDegrees'] >= 360
        if args.unmanaged_720p:
            assert abs(result['ViewportWidth']-1280) < 1 and abs(result['ViewportHeight']-720) < 1,result
        report['passed'] = True
    finally:
        try:
            if args.unmanaged_720p:
                close_client(nonce)
                if world.running(): world.stop(make_backup=False)
            elif world.running() or platform_win.find_processes('DayZ_x64.exe'):
                bridge.wait_action(bridge.action('quit'),90)
        finally:
            bridge.close()
            store.atomic_json(preference_path,original_document)
            report['user_preferences_unchanged'] = store.preferences() == original_preferences
            store.atomic_json(ROOT/f'build/engine-presentation-{args.percent}-results.json',report)
            store.atomic_json(ROOT/f'build/engine-presentation-{profile}.json',report)
            print(json.dumps(report,indent=2),flush=True)
            assert report['user_preferences_unchanged']


if __name__ == '__main__': main()
