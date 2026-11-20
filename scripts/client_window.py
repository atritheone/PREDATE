"""Win32 focus handoff for a freshly launched DayZ window."""
import ctypes
from ctypes import wintypes


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
        self.user.ShowWindowAsync.argtypes = [wintypes.HWND, ctypes.c_int]
        self.user.ShowWindowAsync.restype = wintypes.BOOL
        self.user.SetForegroundWindow.argtypes = [wintypes.HWND]
        self.user.SetForegroundWindow.restype = wintypes.BOOL

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
                if area > 0:
                    windows.append((area, handle))
            return True

        self.user.EnumWindows(collect, 0)
        return max(windows)[1] if windows else None

    def foreground(self, handle):
        return self.user.GetForegroundWindow() == handle

    def activate(self, handle):
        if self.user.IsIconic(handle):
            self.user.ShowWindowAsync(handle, 9)  # SW_RESTORE
        self.user.SetForegroundWindow(handle)

    def handoff(self, handle, launcher_pid):
        if self.foreground(handle):
            return True
        foreground = self.user.GetForegroundWindow()
        if self.owner_pid(foreground) != launcher_pid:
            return False
        self.activate(handle)
        if self.foreground(handle):
            return True

        # The session supervisor is separate from the foreground launcher.
        # Borrow only that launcher's input queue for this single transfer.
        launcher_thread = self.user.GetWindowThreadProcessId(foreground, None)
        current_thread = self.kernel.GetCurrentThreadId()
        message = wintypes.MSG()
        self.user.PeekMessageW(ctypes.byref(message), None, 0, 0, 0)
        if self.user.GetForegroundWindow() != foreground:
            return False
        if launcher_thread and self.user.AttachThreadInput(current_thread, launcher_thread, True):
            try:
                self.user.SetForegroundWindow(handle)
            finally:
                self.user.AttachThreadInput(current_thread, launcher_thread, False)
        return self.foreground(handle)
