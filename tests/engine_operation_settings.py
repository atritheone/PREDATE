"""Verify operation settings, continuation and redeployment with real DayZ."""
import os
from pathlib import Path
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import build
from engine_loadouts import close_client
from engine_map_spawns import wait_for
from engine_sessions import native_evidence
import session
import session_store as store
import world


def item(kind, placement, parent=-1, slot='', row=-1, column=-1, flipped=False, amount=-1):
    return dict(ClassName=kind, Parent=parent, Placement=placement, Slot=slot,
                Row=row, Column=column, Flipped=flipped, Amount=amount)


def check_clock(report, hour, minute, tolerance=0):
    actual = report['Hour'] * 60 + report['Minute']
    expected = hour * 60 + minute
    assert expected <= actual <= expected + tolerance, report


def main():
    build.require_closed_runtime()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    preferences = store.preferences()
    playable = ROOT/'mods/@Predate/addons/Predate.pbo'
    playable_digest = store.digest(playable)
    profile = 'validation-options-' + uuid.uuid4().hex[:6]
    kit = [
        item('AK74', 'hands'),
        item('AK74_WoodBttstck', 'attachment', 0, 'weaponButtstockAK'),
        item('AK74_Hndgrd', 'attachment', 0, 'WeaponHandguardAK'),
        item('Mag_AK74_30Rnd', 'magazine', 0, amount=17),
        item('NVGHeadstrap', 'attachment', slot='Eyewear'),
        item('NVGoggles', 'attachment', 4, 'NVG'),
        item('Battery9V', 'attachment', 5, 'BatteryD'),
        item('AssaultBag_Green', 'attachment', slot='Back'),
        item('BandageDressing', 'cargo', 7, row=1, column=2, flipped=True, amount=2),
    ]
    settings = dict(startHour=20, startMinute=35, enemyRespawns=False, respawnDelay=145, inventory=kit)
    info = store.create_operation('Native operation settings validation', profile=profile, extra_mods=[],
                                  spawn={'x': 4500.25, 'z': 6501.75}, setup=settings)
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    entry = world.MISSION/'init.c'
    probe = (ROOT/'source/validation/operation_settings_probe.c').read_text()
    factory = 'Mission CreateCustomMission(string path)'
    entry.write_text(entry.read_text().replace(factory, probe+'\n'+factory).replace(
        'return new MissionServer;', 'return new PredateOperationSettingsMission;'))
    config_path = world.PROFILE/'PredateConfig.json'
    saved_clock = world.PROFILE/'PredateWorldState.json'
    client = world.WORLD/'client'
    client.mkdir(exist_ok=True)
    report = dict(profile=profile, passed=False, **native_evidence())
    nonce = ''

    def connect():
        (client/'PredateDeploymentValidation.txt').touch()
        world.launch_client(nonce, extra_args=['-window', '-x=1280', '-y=720', '-predateDeploymentValidation'])

    def read_life(life):
        path = world.PROFILE/f'OperationSettingsLife{life}.json'
        return wait_for(lambda: session.read_native(path), bool, timeout=150)

    def check_inventory(data, rounds):
        assert sorted(data['Items']) == sorted(entry['ClassName'] for entry in kit), data
        assert data['MagazineRounds'] == rounds, data
        assert data['NightVisionChain'] == ['NVGHeadstrap', 'NVGoggles', 'Battery9V'], data
        assert (data['CargoRow'], data['CargoColumn'], data['CargoFlipped'], data['BandageAmount']) == (1, 2, True, 2), data
        assert not data['Respawns'] and data['RespawnDelay'] == 145, data

    def check_redeployment(life):
        (world.PROFILE/'EndPlayerLife.txt').touch()
        wait_for(world.native_status, lambda status: status.get('PlayerAwaitingRedeploy'))
        (client/'PredateRedeployClick.txt').write_text('4500.75 0 6501.25\n')
        wait_for(world.native_status, lambda status: status.get('PlayerLife') == life)
        return read_life(life)

    try:
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        check_clock(store.read_json(saved_clock), 20, 35)
        connect()
        report['fresh'] = read_life(1)
        check_clock(report['fresh'], 20, 35, tolerance=1)
        check_inventory(report['fresh'], 17)
        (world.PROFILE/'SpendValidationAmmo.txt').touch()
        saved = wait_for(lambda: session.read_native(saved_clock), lambda state: state.get('Hour') == 7)
        check_clock(saved, 7, 26, tolerance=1)
        close_client(nonce)
        world.stop(make_backup=False)

        config = store.read_json(config_path)
        config.update(StartHour=12, StartMinute=0)
        store.atomic_json(config_path, config)
        (world.PROFILE/'OperationSettingsLife1.json').unlink()
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        connect()
        report['continued'] = read_life(1)
        check_clock(report['continued'], 7, 26, tolerance=3)
        check_inventory(report['continued'], 7)
        report['redeployed'] = check_redeployment(2)
        check_clock(report['redeployed'], 7, 26, tolerance=4)
        check_inventory(report['redeployed'], 17)
        close_client(nonce)
        world.stop(make_backup=False)

        config['PlayerInventory'] = []
        store.atomic_json(config_path, config)
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        connect()
        wait_for(world.native_status, lambda status: status.get('Players') == 1)
        report['empty_redeployment'] = check_redeployment(3)
        assert report['empty_redeployment']['Items'] == [], report
        report['passed'] = True
    finally:
        close_client(nonce)
        if world.running():
            world.stop(make_backup=False)
        report['playable_addon_unchanged'] = store.digest(playable) == playable_digest
        report['user_preferences_unchanged'] = store.preferences() == preferences
        store.atomic_json(ROOT/'build/engine-operation-settings-results.json', report)
        print(report, flush=True)


if __name__ == '__main__':
    main()
