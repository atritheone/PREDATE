"""Project paths and helper invocation for source and packaged launchers."""
import os
from pathlib import Path
import sys
import subprocess
import ctypes
import threading

ROOT = Path(os.environ.get("PREDATE_ROOT", Path(__file__).resolve().parents[1])).resolve()
_spawn_lock = threading.RLock()


def command(module, *arguments, windowless=False):
    if getattr(sys, "frozen", False):
        return [sys.executable, module, *map(str, arguments)]
    executable = Path(sys.executable)
    if windowless and executable.with_name("pythonw.exe").exists():
        executable = executable.with_name("pythonw.exe")
    return [str(executable), str(ROOT / "scripts" / (module + ".py")), *map(str, arguments)]


def spawn(arguments, **options):
    environment = dict(os.environ)
    if getattr(sys, "frozen", False):
        # These helpers outlive the portable UI/bridge and need their own bundle.
        environment["PYINSTALLER_RESET_ENVIRONMENT"] = "1"
    options.setdefault("env", environment)
    if not getattr(sys, "frozen", False):
        return subprocess.Popen(arguments, **options)
    # Do not let bundled Python DLLs alter the game/Steam DLL search path.
    with _spawn_lock:
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.SetDllDirectoryW.argtypes = [ctypes.c_wchar_p]
        old_path = ctypes.create_unicode_buffer(32768)
        kernel.GetDllDirectoryW(32768, old_path)
        kernel.SetDllDirectoryW(None)
        try:
            return subprocess.Popen(arguments, **options)
        finally:
            kernel.SetDllDirectoryW(old_path.value or None)
