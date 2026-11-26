"""Win32 focus handoff for a freshly launched DayZ window."""
import ctypes
from ctypes import wintypes
import subprocess
import threading
import time


class GuiThreadInfo(ctypes.Structure):
    _fields_ = [
        ("size", wintypes.DWORD),
        ("flags", wintypes.DWORD),
        ("active", wintypes.HWND),
        ("focus", wintypes.HWND),
        ("capture", wintypes.HWND),
        ("menu_owner", wintypes.HWND),
        ("move_size", wintypes.HWND),
        ("caret", wintypes.HWND),
        ("caret_rect", wintypes.RECT),
    ]


class ClientWindow:
    """Find and activate the visible window owned by a PREDATE DayZ client."""

    def __init__(self):
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.GetCurrentThreadId.restype = wintypes.DWORD
        self.user = ctypes.WinDLL('user32', use_last_error=True)
        self.callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        self.user.EnumWindows.argtypes = [self.callback_type, wintypes.LPARAM]
        self.user.EnumWindows.restype = wintypes.BOOL
        self.user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        self.user.GetWindowThreadProcessId.restype = wintypes.DWORD
        self.user.IsWindowVisible.argtypes = [wintypes.HWND]
        self.user.IsWindowVisible.restype = wintypes.BOOL
        self.user.IsIconic.argtypes = [wintypes.HWND]
        self.user.IsIconic.restype = wintypes.BOOL
        self.user.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
        self.user.GetWindow.restype = wintypes.HWND
        self.user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
        self.user.GetClientRect.restype = wintypes.BOOL
        self.user.GetForegroundWindow.argtypes = []
        self.user.GetForegroundWindow.restype = wintypes.HWND
        self.user.GetGUIThreadInfo.argtypes = [wintypes.DWORD, ctypes.POINTER(GuiThreadInfo)]
        self.user.GetGUIThreadInfo.restype = wintypes.BOOL
        self.user.IsChild.argtypes = [wintypes.HWND, wintypes.HWND]
        self.user.IsChild.restype = wintypes.BOOL
        self.user.SetFocus.argtypes = [wintypes.HWND]
        self.user.SetFocus.restype = wintypes.HWND
        self.user.ShowWindowAsync.argtypes = [wintypes.HWND, ctypes.c_int]
        self.user.ShowWindowAsync.restype = wintypes.BOOL
        self.user.SetForegroundWindow.argtypes = [wintypes.HWND]
        self.user.SetForegroundWindow.restype = wintypes.BOOL
        self.user.BringWindowToTop.argtypes = [wintypes.HWND]
        self.user.BringWindowToTop.restype = wintypes.BOOL

        self.user.AttachThreadInput.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.BOOL]
        self.user.AttachThreadInput.restype = wintypes.BOOL
        self.user.PeekMessageW.argtypes = [ctypes.POINTER(wintypes.MSG), wintypes.HWND, wintypes.UINT, wintypes.UINT, wintypes.UINT]
        self.user.PeekMessageW.restype = wintypes.BOOL

    def owner_pid(self, handle):
        process = wintypes.DWORD()
        if handle:
            self.user.GetWindowThreadProcessId(handle, ctypes.byref(process))
        return process.value

    def find(self, pid):
        windows = []

        @self.callback_type
        def collect(handle, _):
            if self.owner_pid(handle) != pid or not self.user.IsWindowVisible(handle):
                return True
            if self.user.GetWindow(handle, 4):  # GW_OWNER. Skip owned dialogs.
                return True
            rect = wintypes.RECT()
            if self.user.GetClientRect(handle, ctypes.byref(rect)):
                area = (rect.right - rect.left) * (rect.bottom - rect.top)
                # Exclusive fullscreen can start iconic with an empty client
                # rectangle. It is still the window that needs activation.
                if area > 0 or self.minimized(handle):
                    windows.append((area, handle))
            return True

        self.user.EnumWindows(collect, 0)
        return max(windows)[1] if windows else None

    def foreground(self, handle):
        return self.user.GetForegroundWindow() == handle

    def keyboard_focused(self, handle):
        info = GuiThreadInfo()
        info.size = ctypes.sizeof(info)
        if not self.foreground(handle) or not self.user.GetGUIThreadInfo(0, ctypes.byref(info)):
            return False
        return bool(info.focus and (info.focus == handle or self.user.IsChild(handle, info.focus)))

    def minimized(self, handle):
        return bool(self.user.IsIconic(handle))

    def activate(self, handle):
        if self.minimized(handle):
            self.user.ShowWindowAsync(handle, 9)  # SW_RESTORE
            if self.minimized(handle):
                return False
        if self.keyboard_focused(handle):
            return True
        if not self.foreground(handle):
            self.user.SetForegroundWindow(handle)
            if not self.foreground(handle):
                self.raise_from_foreground(handle)
        if self.keyboard_focused(handle):
            return True
        if self.foreground(handle):
            self.assign_keyboard_focus(handle)
        return self.keyboard_focused(handle)

    def input_thread(self, handle):
        if not handle:
            return None
        other = self.user.GetWindowThreadProcessId(handle, None)
        current = self.kernel.GetCurrentThreadId()
        message = wintypes.MSG()
        self.user.PeekMessageW(ctypes.byref(message), None, 0, 0, 0)
        if other and other != current and self.user.AttachThreadInput(current, other, True):
            return current, other
        return None

    def raise_from_foreground(self, handle):
        foreground = self.user.GetForegroundWindow()
        attached = self.input_thread(foreground)
        if not attached:
            return
        try:
            if self.user.GetForegroundWindow() == foreground:
                self.user.BringWindowToTop(handle)
                self.user.SetForegroundWindow(handle)
        finally:
            self.user.AttachThreadInput(*attached, False)

    def assign_keyboard_focus(self, handle):
        # The foreground window and the recipient of keystrokes are separate
        # Win32 states. SetFocus leaves the game active throughout.
        attached = self.input_thread(handle)
        if not attached:
            return
        try:
            if self.foreground(handle) and not self.keyboard_focused(handle):
                self.user.SetFocus(handle)
        finally:
            self.user.AttachThreadInput(*attached, False)


class ClientLaunchFocus:
    """Give DayZ foreground ownership while it initializes its input devices."""

    def __init__(self, launcher_pid):
        self.window = ClientWindow()
        self.launcher_pid = launcher_pid
        self.focused_handle = None
        self.ready_at = None
        self.next_attempt_at = 0

    def poll(self, pid, connected):
        """Return True when launch-time focus handling is complete."""
        now = time.monotonic()
        if not connected:
            self.ready_at = None
        elif self.ready_at is None:
            self.ready_at = now
        if self.ready_at is not None and now >= self.ready_at + 10:
            print("DayZ startup activation timed out.", flush=True)
            return True
        if now < self.next_attempt_at:
            return False
        self.next_attempt_at = now + 0.25

        handle = self.window.find(pid)
        if not handle:
            return False
        if not self.window.keyboard_focused(handle):
            owner = self.window.owner_pid(self.window.user.GetForegroundWindow())
            if owner not in (0, self.launcher_pid, pid):
                # Leave the active desktop app alone if the player switches away.
                return connected
            if not self.window.activate(handle):
                return False
        if handle != self.focused_handle:
            phase = "client startup" if not connected else "world entry"
            print(f"DayZ received foreground and keyboard focus during {phase}.", flush=True)
            self.focused_handle = handle
        return connected


def hide_diagnostic_startup_dialog(process):
    """Suppress the diagnostic client's native checklist during initialization."""
    threading.Thread(target=_watch_diagnostic_startup, args=(process,), daemon=True).start()


def _watch_diagnostic_startup(process):
    window = ClientWindow()
    user = window.user
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.GetDlgItem.argtypes = [wintypes.HWND, ctypes.c_int]
    user.GetDlgItem.restype = wintypes.HWND
    user.MsgWaitForMultipleObjects.argtypes = [wintypes.DWORD, ctypes.c_void_p, wintypes.BOOL,
                                             wintypes.DWORD, wintypes.DWORD]
    user.DispatchMessageW.argtypes = [ctypes.POINTER(wintypes.MSG)]
    compositor = ctypes.WinDLL('dwmapi')
    compositor.DwmSetWindowAttribute.argtypes = [wintypes.HWND, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
    cloak = wintypes.BOOL(True)
    main_window_seen = False

    def inspect(handle):
        nonlocal main_window_seen
        if window.owner_pid(handle) != process.pid:
            return
        class_name = ctypes.create_unicode_buffer(32)
        user.GetClassNameW(handle, class_name, len(class_name))
        if class_name.value == 'DayZ':
            main_window_seen = True
            return
        if class_name.value != '#32770':
            return
        title = ctypes.create_unicode_buffer(32)
        user.GetWindowTextW(handle, title, len(title))
        # Only the startup checklist has all ten indicator controls. Preserve error dialogs.
        if title.value == 'DayZ' and all(user.GetDlgItem(handle, control) for control in range(1002, 1012)):
            compositor.DwmSetWindowAttribute(handle, 13, ctypes.byref(cloak), ctypes.sizeof(cloak))  # DWMWA_CLOAK
            user.ShowWindowAsync(handle, subprocess.SW_HIDE)

    callback_type = ctypes.WINFUNCTYPE(None, wintypes.HANDLE, wintypes.DWORD, wintypes.HWND,
                                        wintypes.LONG, wintypes.LONG, wintypes.DWORD, wintypes.DWORD)
    user.SetWinEventHook.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.HMODULE, callback_type,
                                   wintypes.DWORD, wintypes.DWORD, wintypes.DWORD]
    user.SetWinEventHook.restype = wintypes.HANDLE
    user.UnhookWinEvent.argtypes = [wintypes.HANDLE]

    @callback_type
    def on_window_event(_hook, _event, handle, object_id, child_id, _thread, _time):
        if handle and not object_id and not child_id:
            inspect(handle)

    @window.callback_type
    def existing_window(handle, _):
        inspect(handle)
        return True

    # EVENT_OBJECT_SHOW and EVENT_OBJECT_CREATE catch both the checklist and the main window.
    hook = user.SetWinEventHook(0x8000, 0x8002, None, on_window_event, process.pid, 0, 0)
    try:
        user.EnumWindows(existing_window, 0)
        if not hook: return
        message = wintypes.MSG()
        deadline = time.monotonic() + 20
        while not main_window_seen and time.monotonic() < deadline and process.poll() is None:
            user.MsgWaitForMultipleObjects(0, None, False, 100, 0x04FF)  # QS_ALLINPUT
            while user.PeekMessageW(ctypes.byref(message), None, 0, 0, 1):
                user.DispatchMessageW(ctypes.byref(message))
    finally:
        if hook: user.UnhookWinEvent(hook)
