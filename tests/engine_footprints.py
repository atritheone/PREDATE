"""Exercise opt-in world traces with real DayZ movement, visibility, and saving."""
import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_loadouts import close_client
from engine_sessions import native_evidence
import build
import session
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uhd', action='store_true', help='Measure the native clue and compass UI at 3840 by 2160')
    args = parser.parse_args()
    build.require_closed_runtime()
    prefs = store.preferences()
    playable = ROOT/'mods/@Predate/addons/Predate.pbo'
    installed_hash = store.digest(playable)
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-footprints-' + uuid.uuid4().hex[:6]
    info = store.create_operation('Footprint validation', profile=profile, extra_mods=[], opponent_count=3)
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    config_path = world.PROFILE/'PredateConfig.json'
    config = store.read_json(config_path)
    config.update(EnableFootprints=True, EnableOperatorRespawns=False, ThreatDistance=180,
                  LynxStart=[4850, 0, 2200], RookStart=[5139, 0, 2122])
    store.atomic_json(config_path, config)
    entry = world.MISSION/'init.c'
    natural = entry.read_text(encoding='utf-8')
    probe = (ROOT/'source/validation/footprint_probe.c').read_text(encoding='utf-8')
    factory = 'Mission CreateCustomMission(string path)'
    entry.write_text(natural.replace(factory, probe+'\n'+factory).replace('return new MissionServer;', 'return new PredateFootprintValidationMission;'), encoding='utf-8')
    report = dict(profile=profile, passed=False, **native_evidence())
    nonce = ''
    try:
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        client = world.WORLD/'client'
        client.mkdir(exist_ok=True)
        (client/'PredateFootprintValidation.txt').touch()
        video_args = ['-window', '-x=1280', '-y=720']
        if args.uhd:
            video_args = ['-predatePresentationValidation']
            (client/'PredatePresentationValidation.txt').touch()
            store.atomic_json(client/'PredatePresentation.json', dict(Schema=1, CompassPercent=100, HighContrast=False))
            for name in (os.environ['USERNAME'], 'Survivor'):
                video = client/'Users'/name/'DayZ.cfg'
                video.parent.mkdir(parents=True, exist_ok=True)
                video.write_text('Windowed=0;\nWindowWidth=3840;\nWindowHeight=2160;\n')
        world.launch_client(nonce, extra_args=[*video_args, '-predateFootprintValidation'])
        deadline = time.monotonic() + 150
        result = {}
        while time.monotonic() < deadline:
            assert world.running(), 'Footprint validation server exited'
            failure = session.read_native(world.WORLD/'client/PredateFailure.json')
            assert not failure, failure.get('message')
            result = session.read_native(world.PROFILE/'FootprintsTrial.json')
            if result.get('Checks'):
                break
            time.sleep(.2)
        report['fixture'] = result
        assert result.get('Checks') == 14 and result['Failures'] == 0, result
        presentation = session.read_native(client/'FootprintPresentation.json')
        assert presentation.get('Checks') == 3 and presentation['Failures'] == 0, presentation
        assert presentation.get('Projected'), 'The native client never projected the visible ground mark'
        report['client_presentation'] = presentation
        if args.uhd:
            viewport = store.read_json(client/'PresentationProbe.json')
            assert (viewport['ViewportWidth'], viewport['ViewportHeight']) == (3840, 2160), viewport
            assert viewport['Checks'] == 27 and viewport['Failures'] == 0, viewport
            report['viewport'] = viewport
        close_client(nonce)
        world.stop(make_backup=False)
        save_path = world.PROFILE/'PredateWorldState.json'
        saved = store.read_json(save_path)
        assert saved['SimulationSeconds'] == result['SimulationSeconds']
        assert len(saved['Footprints']['Marks']) >= result['NaturalMarks']
        world.install_mission()
        world.start()
        reopened = store.read_json(save_path)
        assert saved['Footprints'] == reopened['Footprints']
        assert saved['SimulationSeconds'] == reopened['SimulationSeconds']
        report['identical_footprints_after_reopen'] = True
        report['passed'] = True
    finally:
        close_client(nonce)
        if world.running():
            world.stop(make_backup=False)
        report['playable_addon_unchanged'] = store.digest(playable) == installed_hash
        report['preferences_unchanged'] = store.preferences() == prefs
        suffix = '-uhd' if args.uhd else ''
        store.atomic_json(ROOT/f'build/engine-footprints{suffix}-results.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
