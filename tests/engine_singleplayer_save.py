"""Save both local actors, exit, then restore in a separate official SDK process."""
from pathlib import Path
import hashlib
import json
import os
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import platform_win
import session
import session_store as store
import world


def main():
    assert not any(platform_win.find_processes(name) for name in
                   ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe'))
    stable = ROOT/'mods/@Predate/addons/Predate.pbo'
    digest = hashlib.sha256(stable.read_bytes()).hexdigest()
    folder = ROOT/'work'/('singleplayer-save-'+uuid.uuid4().hex[:6])
    mission = folder/'probe.chernarusplus'
    profile = folder/'profile'
    mission.mkdir(parents=True)
    profile.mkdir()
    (mission/'init.c').write_text((ROOT/'source/validation/singleplayer_persistence.c').read_text())
    mods = [ROOT/'mods'/name for name in
            ('@CF', '@DabsFramework', '@ExpansionCore', '@ExpansionAI', '@PredateCandidate')]
    report = dict(folder=folder.relative_to(ROOT).as_posix(), passed=False, stages=[])
    try:
        for stage in ('saved', 'restored'):
            nonce = uuid.uuid4().hex
            result_path = profile/'PredateLocalPersistence.json'
            # Only this disposable result is replaced; checkpoint files stay.
            result_path.unlink(missing_ok=True)
            with (folder/f'{stage}-process.log').open('wb') as log:
                process = subprocess.Popen([str(world.CLIENT/'DayZDiag_x64.exe'),
                    '-mission='+str(mission), '-profiles='+str(profile),
                    '-mod='+';'.join(map(str, mods)), '-predateSessionId='+nonce,
                    '-window', '-x=1920', '-y=1080', '-noSplash', '-noPause', '-doLogs'],
                    cwd=world.CLIENT, stdout=log, stderr=subprocess.STDOUT)
            identity = world.process_identity(process.pid)
            assert identity
            world.guard_process(identity, profile, profile/'PredateFailure.json')
            try:
                until = time.monotonic()+100
                while process.poll() is None and time.monotonic()<until:
                    time.sleep(.2)
                result = session.read_native(result_path)
                report['stages'].append(result)
                failure = session.read_native(profile/'PredateFailure.json')
                assert not failure, failure
                assert result.get('Phase') == stage, result
                if stage == 'saved':
                    assert result.get('HumanSaved') and result.get('BotSaved'), result
                else:
                    assert result.get('HumanRestored') and result.get('BotRestored'), result
            finally:
                if world.process_identity(identity['pid']) == identity:
                    status = session.read_native(profile/'PredateClientStatus.json')
                    store.atomic_json(profile/'PredateClientCommand.json', dict(Schema=1,
                        SessionId=nonce, Sequence=status.get('Acknowledged', 0)+1, Action='server-closed'))
                    try: process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        if world.process_identity(identity['pid']) == identity:
                            os.kill(identity['pid'], signal.SIGTERM)
                        process.wait(timeout=10)
            if stage == 'saved':
                # A deliberate real closed interval proves restoration does not
                # depend on process-local actor references or simulation uptime.
                time.sleep(5)
        before, after = report['stages']
        for actor in ('HumanState', 'BotState'):
            assert before[actor] == after[actor], (actor, before[actor], after[actor])
        assert before['HumanState']['MagazineRounds'] == 13
        assert before['BotState']['MagazineRounds'] == 17
        assert before['HumanState']['Health'] == 81 and before['BotState']['Health'] == 73
        assert all(after[key] for key in
                   ('SelectedHuman', 'HumanAlive', 'HumanHasMoveCommand'))
        assert not after['Multiplayer'] and not after['HasIdentity']
        assert after['BotResumedTravel'] > 5, after
        assert after['HumanResumedTravel'] > 3, after
        report['passed'] = True
    finally:
        report['playable_addon_unchanged'] = hashlib.sha256(stable.read_bytes()).hexdigest() == digest
        assert report['playable_addon_unchanged']
        store.atomic_json(ROOT/'build/singleplayer-save-probe.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
