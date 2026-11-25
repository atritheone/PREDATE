"""Windows process ownership, startup, and single-supervisor locking."""
from contextlib import ExitStack, contextmanager
import ctypes
from ctypes import wintypes
from pathlib import Path
import hashlib
import os
import subprocess
import uuid


SYNCHRONIZE = 0x00100000
WAIT_OBJECT_0 = 0
ERROR_INVALID_PARAMETER = 87


class StartupInfo(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
        ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
        ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
        ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
        ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
        ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
        ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
        ("lpReserved2", wintypes.LPVOID), ("hStdInput", wintypes.HANDLE),
        ("hStdOutput", wintypes.HANDLE), ("hStdError", wintypes.HANDLE),
    ]


class StartupInfoEx(ctypes.Structure):
    _fields_ = [("startup", StartupInfo), ("attributes", wintypes.LPVOID)]


class ProcessInformation(ctypes.Structure):
    _fields_ = [("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
                ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD)]


@contextmanager
def private_server_desktop():
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPVOID,
                                    wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID]
    user.CreateDesktopW.restype = wintypes.HANDLE
    user.CloseDesktop.argtypes = [wintypes.HANDLE]
    name = "PREDATE-Server-" + uuid.uuid4().hex
    maximum_allowed = 0x02000000
    desktop = user.CreateDesktopW(name, None, None, 0, maximum_allowed, None)
    if not desktop:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        # Keep a handle until the server has connected to the desktop.
        yield name
    finally:
        user.CloseDesktop(desktop)


def start_on_desktop(kernel, arguments, cwd, desktop, stdout):
    """Start a process on a named desktop and return its PID."""
    import msvcrt

    extended_startup_info = 0x00080000
    handle_list_attribute = 0x00020002
    kernel.InitializeProcThreadAttributeList.argtypes = [
        wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, ctypes.POINTER(ctypes.c_size_t)]
    kernel.UpdateProcThreadAttribute.argtypes = [
        wintypes.LPVOID, wintypes.DWORD, ctypes.c_size_t, wintypes.LPVOID,
        ctypes.c_size_t, wintypes.LPVOID, wintypes.LPVOID]
    kernel.DeleteProcThreadAttributeList.argtypes = [wintypes.LPVOID]
    kernel.DeleteProcThreadAttributeList.restype = None
    kernel.CreateProcessW.argtypes = [
        wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.LPVOID, wintypes.LPVOID,
        wintypes.BOOL, wintypes.DWORD, wintypes.LPVOID, wintypes.LPCWSTR,
        ctypes.POINTER(StartupInfoEx), ctypes.POINTER(ProcessInformation)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]

    with ExitStack() as cleanup:
        discard = cleanup.enter_context(open(os.devnull, "r+b"))
        handles = (wintypes.HANDLE * 2)()
        for index, stream in enumerate((discard, stdout if stdout is not None else discard)):
            fd = os.dup(stream.fileno())
            cleanup.callback(os.close, fd)
            os.set_inheritable(fd, True)
            handles[index] = msvcrt.get_osfhandle(fd)

        size = ctypes.c_size_t()
        kernel.InitializeProcThreadAttributeList(None, 1, 0, ctypes.byref(size))
        attributes = ctypes.create_string_buffer(size.value)
        if not kernel.InitializeProcThreadAttributeList(attributes, 1, 0, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        cleanup.callback(kernel.DeleteProcThreadAttributeList, attributes)
        if not kernel.UpdateProcThreadAttribute(attributes, 0, handle_list_attribute,
                                                handles, ctypes.sizeof(handles), None, None):
            raise ctypes.WinError(ctypes.get_last_error())

        startup = StartupInfoEx()
        startup.startup.cb = ctypes.sizeof(startup)
        startup.startup.lpDesktop = desktop
        startup.startup.dwFlags = subprocess.STARTF_USESHOWWINDOW | subprocess.STARTF_USESTDHANDLES
        startup.startup.wShowWindow = subprocess.SW_HIDE
        startup.startup.hStdInput = handles[0]
        startup.startup.hStdOutput = startup.startup.hStdError = handles[1]
        startup.attributes = ctypes.cast(attributes, wintypes.LPVOID)
        process = ProcessInformation()
        command_line = ctypes.create_unicode_buffer(subprocess.list2cmdline(arguments))
        if not kernel.CreateProcessW(arguments[0], command_line, None, None, True,
                                     extended_startup_info | subprocess.CREATE_NO_WINDOW, None,
                                     str(cwd), ctypes.byref(startup), ctypes.byref(process)):
            raise ctypes.WinError(ctypes.get_last_error())
        kernel.CloseHandle(process.hThread)
        kernel.CloseHandle(process.hProcess)
        return process.dwProcessId


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
