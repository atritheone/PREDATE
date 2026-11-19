"""Load signed extra gear through the frozen launcher and retain its native state."""
import json
import os
from pathlib import Path
import shutil
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import Bridge, native_evidence
import build
import platform_win
import session
import session_store as store
import world


def pack_fixture(identifier):
    addon = ROOT/'work'/identifier/'@PredateValidationGear'
    packages = addon/'addons'
    packages.mkdir(parents=True)
    source = ROOT/'source/validation/extra_gear'
    build.run([build.TOOLS/'PboUtils/FileBank.exe', '-property', 'prefix=PredateValidationGear', '-dst', packages, source])
    package = packages/'extra_gear.pbo'
    assert package.is_file(), package
    build.run([build.TOOLS/'DsUtils/DSSignFile.exe', ROOT/'private-signing/PredateLocal.biprivatekey', package])
    assert package.with_suffix('.pbo.PredateLocal.bisign').is_file()
    (addon/'keys').mkdir()
    shutil.copy2(ROOT/'private-signing/PredateLocal.bikey', addon/'keys/PredateLocal.bikey')
    return addon


def wait_gear(filename):
    path = world.PROFILE/filename
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        result = session.read_native(path)
        if result:
            assert result['Registered'] and result['ItemCount'] == 1, result
            assert abs(result['Condition'] - .42) < .001 and result['PlayerMagazine'] > 0, result
            return result
        time.sleep(.15)
    raise TimeoutError(f'Gear report unavailable: {filename}')


def main():
    build.require_closed_runtime()
    assert not session.manager_alive(), 'Use a fresh matching supervisor for validation'
    preferences = store.preferences()
    protected = preferences['selected_world']
    before = {name: store.digest(path) for path, name in store.save_files(protected)}
    profile = 'validation-gear-' + uuid.uuid4().hex[:6]
    addon = pack_fixture(profile)
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    info = store.create_operation('Optional gear validation', profile=profile, extra_mods=[str(addon)],
                                  opponent_count=3, footprints=True)
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    mission = world.MISSION/'init.c'
    native = mission.read_text(encoding='utf-8')
    probe = (ROOT/'source/validation/optional_gear_probe.c').read_text(encoding='utf-8')
    factory = 'Mission CreateCustomMission(string path)'
    mission.write_text(native.replace(factory, probe+'\n'+factory).replace(
        'return new MissionServer;', 'return new PredateOptionalGearMission;'), encoding='utf-8')
    bridge = Bridge(candidate=True)
    report = dict(profile=profile, passed=False, **native_evidence())
    try:
        for name in ('Initial', 'Reopened'):
            bridge.wait_action(bridge.action('continue', profile=profile))
            bridge.wait_phase({'playing'})
            report[name] = wait_gear(f'OptionalGear{name}.json')
            assert bool(report[name]['Reopened']) == (name == 'Reopened')
            bridge.wait_action(bridge.action('quit'), 90)
            bridge.wait_phase({'suspended'})
        condition_error = abs(report['Initial']['Condition'] - report['Reopened']['Condition'])
        assert condition_error < .0001, condition_error
        report['condition_difference'] = condition_error
        assert report['Initial']['PlayerMagazine'] == report['Reopened']['PlayerMagazine']
        record = store.metadata(profile)['mod_lock']['mods']
        assert len(record) == 1 and record[0]['path'] == str(addon)
        mounted = [path for path in world.PROFILE.glob('*.RPT')
                   if any("PBO Load" in line and "extra_gear" in line for line in path.read_text(errors='replace').splitlines())]
        clients = [path for path in (world.WORLD/'client').glob('*.RPT')
                   if str(addon).casefold() in path.read_text(errors='replace').casefold()]
        assert len(mounted) == len(clients) == 2, (mounted, clients)
        report['server_loaded_extra_addon'] = True
        report['client_received_extra_addon_path'] = True
        report['signed_addon_retained'] = True
        report['passed'] = True
    finally:
        try:
            if world.running() or platform_win.find_processes('DayZ_x64.exe'):
                bridge.wait_action(bridge.action('quit'), 90)
        finally:
            bridge.close()
            store.save_preferences({key:value for key,value in preferences.items() if key != 'schema'})
            report['preferences_restored'] = store.preferences() == preferences
            report['protected_save_unchanged'] = before == {name:store.digest(path) for path,name in store.save_files(protected)}
            store.atomic_json(ROOT/'build/engine-optional-gear-results.json', report)
            print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
