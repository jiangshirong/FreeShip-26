"""Probe the first-launch flow on the unified runner's private desktop."""
import json
import os
import subprocess
import time

from private_desktop import require_private_desktop


def capture_window(hwnd, filename):
    from ctypes import WinDLL, c_void_p, wintypes
    from PIL import Image
    import win32gui
    import win32ui

    user32 = WinDLL('user32')
    set_dpi_context = user32.SetThreadDpiAwarenessContext
    set_dpi_context.argtypes = (c_void_p,)
    set_dpi_context.restype = c_void_p
    previous_dpi_context = set_dpi_context(c_void_p(-4))
    if not previous_dpi_context:
        raise RuntimeError('Could not query the window in physical pixels')
    try:
        left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    finally:
        set_dpi_context(previous_dpi_context)
    width, height = right - left, bottom - top
    source_handle = win32gui.GetWindowDC(hwnd)
    source = win32ui.CreateDCFromHandle(source_handle)
    destination = source.CreateCompatibleDC()
    bitmap = win32ui.CreateBitmap()
    bitmap.CreateCompatibleBitmap(source, width, height)
    previous = destination.SelectObject(bitmap)
    try:
        capture = user32.PrintWindow
        capture.argtypes = (wintypes.HWND, wintypes.HDC, wintypes.UINT)
        capture.restype = wintypes.BOOL
        if not capture(hwnd, destination.GetSafeHdc(), 2):
            raise RuntimeError('Could not capture the first-launch window')
        image = Image.frombytes('RGB', (width, height), bitmap.GetBitmapBits(True), 'raw', 'BGRX')
        image.save(filename)
    finally:
        destination.SelectObject(previous)
        win32gui.DeleteObject(bitmap.GetHandle())
        destination.DeleteDC()
        win32gui.ReleaseDC(hwnd, source_handle)


def exercise(report, runtime, output):
    import pywintypes
    import win32con
    import win32gui
    import win32process

    require_private_desktop()
    profile = output / 'profile'
    settings = profile / 'preferences.json'
    catalogs = {
        p.stem: json.loads(p.read_text(encoding='utf-8-sig'))['messages']
        for p in (runtime / 'languages').glob('*.json')
    }
    title_key = '选择你的语言'
    english_title = catalogs['en'][title_key]

    def windows(process):
        found = {}

        def add(hwnd, _):
            if (win32gui.IsWindowVisible(hwnd)
                    and win32process.GetWindowThreadProcessId(hwnd)[1] == process.pid):
                title = win32gui.GetWindowText(hwnd)
                if title:
                    found[hwnd] = title
            return True

        try:
            win32gui.EnumWindows(add, None)
        except pywintypes.error as error:
            if error.winerror != 0 or found:
                raise
        return found

    def wait_window(process, matches):
        stop = time.monotonic() + 12
        while time.monotonic() < stop and process.poll() is None:
            for hwnd, title in windows(process).items():
                if matches(title):
                    return hwnd, title
            time.sleep(.05)
        raise RuntimeError(f'Expected first-launch window state, saw {windows(process)}')

    def launch():
        return subprocess.Popen(
            [str(runtime / 'FreeShipQt.exe'), '--verify-first-launch', str(profile)],
            cwd=output, creationflags=subprocess.CREATE_NO_WINDOW,
            env={**os.environ, 'QT_ENABLE_HIGHDPI_SCALING': '0', 'QT_SCALE_FACTOR': '1.25'},
        )

    def close(hwnd):
        win32gui.PostMessage(hwnd, win32con.WM_SYSCOMMAND, win32con.SC_CLOSE, 0)

    def key(hwnd, code):
        win32gui.PostMessage(hwnd, win32con.WM_KEYDOWN, code, 1)
        win32gui.PostMessage(hwnd, win32con.WM_KEYUP, code, 0xc0000001)

    def observe_controls(hwnd):
        script = """
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$element=[Windows.Automation.AutomationElement]::FromHandle([IntPtr]%d)
$controls=@($element.FindAll([Windows.Automation.TreeScope]::Descendants,
    [Windows.Automation.Condition]::TrueCondition) | ForEach-Object {
    $current=$_.Current
    if (!$current.IsOffscreen -and $current.Name) {
        $bounds=$current.BoundingRectangle
        @{text=$current.Name; x=$bounds.X; y=$bounds.Y; width=$bounds.Width; height=$bounds.Height}
    }
})
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
ConvertTo-Json -InputObject $controls -Compress
""" % hwnd
        result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script],
                                capture_output=True, encoding='utf8', timeout=10,
                                creationflags=subprocess.CREATE_NO_WINDOW, check=True)
        return json.loads(result.stdout)

    def check_language(process, language, phase=None):
        messages = catalogs[language]
        hwnd, title = wait_window(process, lambda name: name.startswith(messages[title_key]))
        controls = observe_controls(hwnd)
        texts = [control['text'] for control in controls]
        expected = [messages[k] for k in (title_key, '第一语言', '第二语言',
                    '通过快捷键', '在第一语言和第二语言之间切换', '继续')]
        report.case('first-launch.' + (phase or 'captions-' + language), all(t in texts for t in expected),
                    observed={'window_title': title, 'visible_text': texts, 'expected': expected})
        return hwnd, controls

    with launch() as app:
        try:
            hwnd, controls = check_language(app, 'en')
            texts = [control['text'] for control in controls]
            report.case('first-launch.cancel-button-absent', catalogs['en']['取消'] not in texts,
                        observed=texts)
            by_text = {control['text']: control for control in controls}
            prefix = by_text[catalogs['en']['通过快捷键']]
            primary = by_text[catalogs['en']['第一语言']]
            report.case('first-launch.shortcut-starts-at-left-form-margin',
                        abs(prefix['x'] - primary['x']) <= 2,
                        observed={'shortcut': prefix, 'primary_language': primary})
            capture_window(hwnd, output / 'first-launch-english.png')
            # The primary selector starts focused and skips the secondary language.
            key(hwnd, win32con.VK_UP)
            hwnd, _ = check_language(app, 'zh')
            key(hwnd, win32con.VK_DOWN)
            hwnd, _ = check_language(app, 'en', 'back-to-english')
            for language in ('ja', 'de', 'fr', 'es'):
                key(hwnd, win32con.VK_DOWN)
                hwnd, _ = check_language(app, language)
            # Free Russian for the primary selector by moving the secondary to Japanese.
            key(hwnd, win32con.VK_TAB)
            key(hwnd, win32con.VK_DOWN)
            for _ in range(3):
                key(hwnd, win32con.VK_TAB)
            for _ in range(3):
                key(hwnd, win32con.VK_UP)
            hwnd, _ = check_language(app, 'ru')
            capture_window(hwnd, output / 'first-launch-russian.png')
            close(hwnd)
            report.case('first-launch.titlebar-close-exits-and-does-not-save',
                        app.wait(timeout=8) == 0 and not settings.exists())
        finally:
            if app.poll() is None:
                app.kill()

    with launch() as app:
        try:
            hwnd, _ = wait_window(app, lambda title: title.startswith(english_title))
            report.case('first-launch.shows-again-after-close', not settings.exists())
            key(hwnd, win32con.VK_RETURN)
            main, _ = wait_window(app, lambda title: not title.startswith(english_title))
            values = json.loads(settings.read_text(encoding='utf8'))['values']
            report.case('first-launch.confirm-saves-and-opens-main',
                        values.get('ui_language') == values.get('quick_language_first') == 'en'
                        and values.get('quick_language_second') == 'ru'
                        and values.get('quick_language_enabled') is True,
                        observed=values)
            close(main)
            app.wait(timeout=8)
        finally:
            if app.poll() is None:
                app.kill()

    committed = settings.read_bytes()
    with launch() as app:
        try:
            main, _ = wait_window(app, lambda title: not title.startswith(english_title))
            report.case('first-launch.confirmed-profile-skips-setup',
                        settings.read_bytes() == committed)
            close(main)
            app.wait(timeout=8)
        finally:
            if app.poll() is None:
                app.kill()
