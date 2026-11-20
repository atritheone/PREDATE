"""Launch a fresh take of the PREDATE recording scenario."""

from datetime import datetime
from pathlib import Path
import re
import sys
import time
import uuid

from app_runtime import ROOT
import build
from client_window import ClientWindow
import platform_win
import session
import session_store as store
import world


RECORDING_MISSION = """
class PredateRecordingMission : MissionServer
{
    override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
    {
        super.StartingEquipSetup(player, clothesChosen);
        Weapon_Base rifle = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
        if (!rifle) return;
        Magazine magazine = rifle.GetMagazine(rifle.GetCurrentMuzzle());
        if (magazine) magazine.ServerSetAmmoCount(Math.Round(magazine.GetAmmoMax() * 0.5));
    }
}
"""


def create_take():
    previous = store.preferences()['selected_world']
    stamp = datetime.now().strftime('%m%d-%H%M%S')
    profile = f'op-recording-{stamp}-{uuid.uuid4().hex[:4]}'
    info = store.create_operation('Recording â€” 500 m hunt (live start)', profile=profile, extra_mods=[])
    info.update(recording=True, previous_operation=previous)

    config_file = world.PROFILE / 'PredateConfig.json'
    config = store.read_json(config_file)
    config.update(PlayerStart=[4500, 0, 6500], BotStart=[4500, 0, 7000], PatrolSize=30)
    store.atomic_json(config_file, config)

    video = {'Windowed': 0, 'WindowWidth': 3840, 'WindowHeight': 2160, 'WinX': 0, 'WinY': 0}
    previous_users = store.world_path(previous) / 'client/Users'
    default_settings = Path.home() / 'Documents/DayZ/DayZ.cfg'
    sources = {'Survivor': default_settings}
    sources.update((config.parent.name, config) for config in previous_users.glob('*/DayZ.cfg') if config.is_file())
    for name, source in sources.items():
        settings = source.read_text() if source.exists() else ''
        for key, value in video.items():
            settings = re.sub(rf'(?m)^{key}=[^\r\n]*\r?\n?', '', settings)
            settings += f'\n{key}={value};\n'
        target = world.WORLD / 'client/Users' / name / 'DayZ.cfg'
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(settings)

    entry = world.MISSION / 'init.c'
    mission = entry.read_text(encoding='utf-8')
    factory = 'Mission CreateCustomMission(string path)'
    default = 'return new MissionServer;'
    if mission.count(factory) != 1 or mission.count(default) != 1:
        raise RuntimeError('The mission template has changed. The recording take could not be prepared.')
    mission = mission.replace(factory, RECORDING_MISSION + '\n' + factory)
    entry.write_text(mission.replace(default, 'return new PredateRecordingMission;'), encoding='utf-8')
    store.atomic_json(world.WORLD / 'operation.json', info)
    return profile, previous


def finish_previous_take():
    status = session.state()
    if status['phase'] in {'idle', 'suspended', 'error'}:
        return
    profile = status.get('profile')
    if not profile or not store.metadata(profile).get('recording'):
        raise RuntimeError('Save and quit your current operation before opening the recording scenario.')
    print('Saving the previous recording take...', flush=True)
    session.wait_result(session.request('quit'))


def wait_for_gameplay(profile):
    deadline = time.monotonic() + 185
    window = ClientWindow()
    client_path = store.world_path(profile) / 'client/PredateClientStatus.json'
    activated_while_loading = False
    ready_at = None
    foreground_before = None
    while time.monotonic() < deadline:
        status = session.state()
        if status.get('profile') != profile:
            raise RuntimeError('PREDATE switched to a different operation during launch.')
        if status['phase'] in {'error', 'suspended', 'idle'}:
            raise RuntimeError(status['message'])

        active = session.read_native(session.RUNTIME / 'active-session.json')
        client = active.get('client', {})
        session_id = status.get('session_id')
        if active.get('profile') != profile or active.get('session_id') != session_id or not client:
            time.sleep(0.1)
            continue
        if world.process_identity(client['pid']) != client:
            time.sleep(0.1)
            continue
        handle = window.find(client['pid'])
        if not handle:
            time.sleep(0.1)
            continue

        if foreground_before is None:
            foreground_before = window.foreground(handle)
            if not foreground_before:
                window.activate(handle)
                activated_while_loading = True

        native = session.read_native(client_path)
        if native.get('SessionId') == session_id and native.get('Phase') == 'connected':
            if ready_at is None:
                ready_at = time.monotonic()
                if not window.foreground(handle):
                    window.activate(handle)
            focused = window.foreground(handle)
            if focused or time.monotonic() - ready_at >= 2:
                store.atomic_json(ROOT / 'build/recording-focus.json', {
                    'profile': profile, 'initial_foreground': foreground_before,
                    'requested_during_loading': activated_while_loading,
                    'foreground_at_gameplay': focused,
                })
                if not focused:
                    raise RuntimeError('Windows did not give the fullscreen DayZ window focus. The recording take is open.')
                return
        time.sleep(0.1)
    raise TimeoutError('DayZ is still connecting. Check the PREDATE launcher for its current status.')


def main():
    if platform_win.singleton(ROOT / 'recording-launch') is None:
        raise RuntimeError('A recording take is already launching.')
    finish_previous_take()
    build.require_closed_runtime()
    profile, previous = create_take()
    print('Opening the 4K recording scenario...', flush=True)
    result = session.wait_result(session.request('continue', profile=profile))
    if result.get('cancelled'):
        raise RuntimeError('Recording launch was cancelled.')
    store.atomic_json(ROOT / 'build/recording-operation.json', {'profile': profile, 'previous_operation': previous})
    wait_for_gameplay(profile)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f'PREDATE: {error}', file=sys.stderr)
        sys.exit(1)
