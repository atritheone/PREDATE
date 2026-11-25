"""Project paths and helper invocation for source and packaged launchers."""
from contextlib import contextmanager
import os
from pathlib import Path
import sys
import subprocess
import ctypes
from ctypes import wintypes
import threading

ROOT = Path(os.environ.get("PREDATE_ROOT", Path(__file__).resolve().parents[1])).resolve()
_spawn_lock = threading.RLock()


def saves_directory(root):
    """Move legacy operation folders as a single unit once their owner has stopped."""
    saves = root / "saves"
    legacy = root / "worlds"
    if not legacy.exists():
        return saves

    import platform_win

    handle = platform_win.singleton(root)
    if handle is None:
        return legacy if legacy.exists() else saves
    try:
        if not legacy.exists():
            return saves
        if saves.exists() or saves.is_symlink():
            raise RuntimeError("Both worlds and saves exist. Move the legacy operations into saves after checking for conflicts.")
        if not legacy.is_dir() or legacy.resolve() != root.resolve() / "worlds":
            raise RuntimeError("The legacy worlds folder must be stored inside the PREDATE installation.")
        if any(platform_win.find_processes(name) for name in ("DayZ_x64.exe", "DayZServer_x64.exe", "DayZDiag_x64.exe")):
            return legacy
        legacy.rename(saves)
        return saves
    finally:
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.CloseHandle(handle)


def command(module, *arguments, windowless=False):
    if getattr(sys, "frozen", False):
        return [sys.executable, module, *map(str, arguments)]
    executable = Path(sys.executable)
    if windowless and executable.with_name("pythonw.exe").exists():
        executable = executable.with_name("pythonw.exe")
    return [str(executable), str(ROOT / "scripts" / (module + ".py")), *map(str, arguments)]


@contextmanager
def _spawn_environment():
    # Do not let bundled Python DLLs alter the game/Steam DLL search path.
    with _spawn_lock:
        if not getattr(sys, "frozen", False):
            yield
            return
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.SetDllDirectoryW.argtypes = [ctypes.c_wchar_p]
        old_path = ctypes.create_unicode_buffer(32768)
        kernel.GetDllDirectoryW(32768, old_path)
        kernel.SetDllDirectoryW(None)
        try:
            yield
        finally:
            kernel.SetDllDirectoryW(old_path.value or None)


def spawn(arguments, **options):
    environment = dict(os.environ)
    if getattr(sys, "frozen", False):
        # These helpers outlive the portable UI/bridge and need their own bundle.
        environment["PYINSTALLER_RESET_ENVIRONMENT"] = "1"
    options.setdefault("env", environment)
    with _spawn_environment():
        return subprocess.Popen(arguments, **options)


def spawn_server(arguments, cwd, desktop, stdout=None):
    from platform_win import start_on_desktop

    with _spawn_environment():
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        previous = kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
        try:
            return start_on_desktop(kernel, arguments, cwd, desktop, stdout)
        finally:
            kernel.SetErrorMode(previous)
