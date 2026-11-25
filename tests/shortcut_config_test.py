#!/usr/bin/env python3
"""Wiring/serialization contracts complement portable stubs and native User32 tests."""
from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent

def read(path):
    data = (root / path).read_bytes()
    return data.decode('utf-16') if data[:2] in (b'\xff\xfe', b'\xfe\xff') else data.decode('utf-8-sig')

names = ['AutoTileGrid', 'AutoTileExpandBoth', 'AutoTileExpandHorizontal', 'AutoTileExpandVertical']
config = read('config.c')
table = config.split('static const actiondl_t kbshortcut_actions[] = {', 1)[1].split('{NULL, 0}', 1)[0]
for name in names:
    assert re.search(r'\{TEXT\("'+name+r'"\),\s*L10NIDX\(InputAction'+name+r'\)', table), name
    assert re.search(r'ACVALUE\([^,]+,\s*'+name+r',', read('hooks.h')), name
    for language in ['Lang/_en_US baseline.txt', 'Lang/ru_RU.ini']:
        assert re.search(r'^InputAction'+name+r'=.+$', read(language), re.M), (name, language)
    assert re.search(r'^'+name+r'=0\r?$', read('AltSnap.dni'), re.M), name
assert 'KBShortcutFill(GetDlgItem(hwnd, IDC_SHORTCUTS_AC)' in config
assert 'KBShortcutAction(GetDlgItem(hwnd, IDC_SHORTCUTS_AC)' in config
assert 'KBShortcutSave(hwnd, inipath, action_string_buf' in config
# Do not silently reintroduce the Alt-system-message capture bug.
picker = config.split('static LRESULT WINAPI PickShortcutWinProc(', 1)[1].split('// Dialog procedure', 1)[0]
assert re.search(r'case WM_KEYDOWN:\s*case WM_SYSKEYDOWN:', picker)
assert re.search(r'case WM_KEYUP:\s*case WM_SYSKEYUP:', picker)
assert 'UnhookSystem()' in picker and 'HookSystem()' in picker
assert 'WM_SHORTCUTERROR' in read('altsnap.c') and 'WM_SHORTCUTERROR' in read('hooks.c')
print('shortcut configuration: action maps, unassigned GUI entries, EN/RU labels, opt-in defaults, picker and error routing passed')
