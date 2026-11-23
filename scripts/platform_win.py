"""Process ownership checks and single-supervisor locking. No window automation."""
import ctypes
from ctypes import wintypes
from pathlib import Path
import hashlib


SYNCHRONIZE = 0x00100000
WAIT_OBJECT_0 = 0
ERROR_INVALID_PARAMETER = 87


class ProcessEntry(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
                ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
                ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260)]


def _process_running(kernel, pid):
    handle = kernel.OpenProcess(SYNCHRONIZE, False, pid)
    if not handle:
        # A process can disappear after the snapshot. Keep inaccessible matches.
        return ctypes.get_last_error() != ERROR_INVALID_PARAMETER
    try:
        # A terminated process can remain in the snapshot while a handle to it is open.
        return kernel.WaitForSingleObject(handle, 0) != WAIT_OBJECT_0
    finally:
        kernel.CloseHandle(handle)


def find_processes(name):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
    kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
    if snapshot == ctypes.c_void_p(-1).value: raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = ProcessEntry()
        entry.dwSize = ctypes.sizeof(entry)
        found = []
        more = kernel.Process32FirstW(snapshot, ctypes.byref(entry))
        target = name.casefold()
        while more:
            if entry.szExeFile.casefold() == target and _process_running(kernel, entry.th32ProcessID):
                found.append(entry.th32ProcessID)
            more = kernel.Process32NextW(snapshot, ctypes.byref(entry))
        return found
    finally:
        kernel.CloseHandle(snapshot)


def singleton(root):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateMutexW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.LPCWSTR]
    kernel.CreateMutexW.restype = wintypes.HANDLE
    label = hashlib.sha256(str(Path(root).resolve()).casefold().encode()).hexdigest()[:20]
    handle = kernel.CreateMutexW(None, False, "Local\\PREDATE-" + label)
    if ctypes.get_last_error() == 183:
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.CloseHandle(handle)
        return None
    if not handle: raise ctypes.WinError(ctypes.get_last_error())
    return handle
