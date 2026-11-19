"""Measure native dynamic-body motion during the server's gameplay pause."""
from pathlib import Path
import hashlib
import argparse
import json
import os
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import fixture
import platform_win
import session
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hold-bodies', action='store_true', help='Test explicit world rigid-body holds')
    parser.add_argument('--seconds', type=int, default=4)
    args = parser.parse_args()
    assert 1 <= args.seconds <= 120
    assert not any(platform_win.find_processes(name) for name in
                   ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe'))
    stable = ROOT/'mods/@Predate/addons/Predate.pbo'
    digest = hashlib.sha256(stable.read_bytes()).hexdigest()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = fixture('crashlab-primitive-send-d1612')
    entry = world.MISSION/'init.c'
    injection = (ROOT/'source/validation/physics_pause.c').read_text()
    injection = injection.replace('/*PREDATE_HOLD_BODIES*/', 'HoldPhysicsBodies();' if args.hold_bodies else '')
    injection = injection.replace('/*PREDATE_RESUME_BODIES*/', 'ResumePhysicsBodies();' if args.hold_bodies else '')
    injection = injection.replace('/*PREDATE_HOLD_SECONDS*/', str(args.seconds))
    entry.write_text(entry.read_text().replace('Mission CreateCustomMission(string path)',
        injection+'\nMission CreateCustomMission(string path)').replace(
        'return new PredateValidationMission;', 'return new PredatePhysicsValidationMission;'))
    report = dict(profile=profile, candidate=True, explicit_body_hold=args.hold_bodies, passed=False)
    try:
        world.start()
        nonce = json.loads(world.STATE.read_text())['session_id']
        world.launch_client(nonce, extra_args=['-window', '-x=1920', '-y=1080'])
        until = time.monotonic()+150+args.seconds
        while time.monotonic()<until:
            assert world.running(), 'Owned test server exited'
            failure = session.read_native(world.WORLD/'client/PredateFailure.json')
            assert not failure, failure
            result = session.read_native(world.PROFILE/'PredatePhysicsPauseResult.json')
            if result: break
            time.sleep(.2)
        report['native'] = result
        assert result and result.get('Failures') == 0, result
        report['passed'] = True
    finally:
        if platform_win.find_processes('DayZ_x64.exe'):
            client = session.read_native(world.WORLD/'client/PredateClientStatus.json')
            store.atomic_json(world.WORLD/'client/PredateClientCommand.json', dict(
                Schema=1, SessionId=json.loads(world.STATE.read_text())['session_id'],
                Sequence=client.get('Acknowledged', 0)+1,
                Action='quit' if world.running() else 'server-closed'))
            until = time.monotonic()+45
            while platform_win.find_processes('DayZ_x64.exe') and time.monotonic()<until: time.sleep(.2)
        if world.running(): world.stop(make_backup=False)
        report['playable_addon_unchanged'] = hashlib.sha256(stable.read_bytes()).hexdigest() == digest
        assert report['playable_addon_unchanged']
        store.atomic_json(ROOT/'build/engine-physics-pause-results.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
