"""Track native gameplay holds and recover older owned process suspensions."""
import ctypes
from ctypes import wintypes
import os
import time
from pathlib import Path

import session_store as store
import world


def lease_path(): return world.WORLD / 'server-pause.json'


def mark_expected_freeze(identity, resumed=False, client=None):
    path = lease_path().parent / 'intentional-pause.json'
    marker = store.read_json(path, {}) if resumed else {}
    marker.update(server=identity)
    if client: marker['client'] = client
    if resumed: marker['resumed_at'] = time.time()
    else: marker['paused_at'] = time.time()
    store.atomic_json(path, marker)


def _change(identity, suspend):
    expected = {k: identity[k] for k in ('pid', 'created', 'executable')}
    if Path(expected['executable']).resolve() != (world.SERVER/'DayZServer_x64.exe').resolve():
        raise RuntimeError('Only this project\'s DayZ server can be paused')
    if world.process_identity(expected['pid']) != expected:
        raise RuntimeError('The owned server changed; pause was refused')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x0800 | 0x1000, False, expected['pid'])
    if not handle: raise ctypes.WinError(ctypes.get_last_error())
    try:
        if world.process_identity(expected['pid']) != expected:
            raise RuntimeError('The server exited before its pause could change')
        native = ctypes.WinDLL('ntdll')
        function = native.NtSuspendProcess if suspend else native.NtResumeProcess
        function.argtypes = [wintypes.HANDLE]
        function.restype = ctypes.c_long
        status = function(handle)
        if status < 0: raise RuntimeError(f'Windows could not change server pause (0x{status & 0xffffffff:08x})')
    finally: kernel.CloseHandle(handle)


def suspended(session_id):
    lease = store.read_json(lease_path(), {})
    return bool(lease and lease.get('session_id') == session_id)


def pause(session_id):
    if suspended(session_id): return
    identity = store.read_json(world.STATE)
    if not identity or identity.get('session_id') != session_id:
        raise RuntimeError('Pause requires this operation\'s owned server')
    expected = {k: identity[k] for k in ('pid', 'created', 'executable')}
    if Path(expected['executable']).resolve() != (world.SERVER/'DayZServer_x64.exe').resolve() or world.process_identity(expected['pid']) != expected:
        raise RuntimeError('The owned server changed; pause was refused')
    native = world.native_status()
    if native.get('SessionId') != session_id or not native.get('Paused') or not native.get('PauseReady'):
        raise RuntimeError('The server has not acknowledged its gameplay pause')
    # The server has already held gameplay; this lease tracks ownership only.
    store.atomic_json(lease_path(), {'schema': 1, 'session_id': session_id,
        'server': identity, 'mode': 'gameplay-hold',
        'owner': world.process_identity(os.getpid())})


def resume(session_id):
    lease = store.read_json(lease_path(), {})
    if not lease: return
    if lease.get('session_id') != session_id:
        raise RuntimeError('Another session owns this pause lease')
    identity = lease['server']
    if lease.get('mode') != 'gameplay-hold': mark_expected_freeze(identity, resumed=True)
    expected = {k: identity[k] for k in ('pid', 'created', 'executable')}
    if world.process_identity(expected['pid']) == expected:
        if lease.get('mode') == 'gameplay-hold': pass
        else: _change(identity, False)  # Recover a lease written by the older build.
    lease_path().unlink(missing_ok=True)
