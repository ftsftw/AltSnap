/* SPDX-License-Identifier: GPL-3.0-or-later
   Shared INI contracts. Caller suspends its hotkeys while probing a new chord. */
#ifndef ALTSNAP_KB_SHORTCUTS_H
#define ALTSNAP_KB_SHORTCUTS_H

static TCHAR *KBShortcutSection(const TCHAR *ini)
{
    DWORD n = 4096;
    for (;;) {
        TCHAR *section = (TCHAR *)calloc(n, sizeof(TCHAR));
        DWORD read;
        if (!section) return NULL;
        read = GetPrivateProfileSection(TEXT("KBShortcuts"), section, n, ini);
        if (read < n - 2) return section;
        free(section);
        if (n == 32768) return NULL; /* Never accept a truncated assignment list. */
        n *= 2;
    }
}

static int KBShortcutValid(WORD hk)
{
    BYTE key = LOBYTE(hk);
    return !hk || (key && !(HIBYTE(hk) & ~15)
        && key != VK_SHIFT && key != VK_CONTROL && key != VK_MENU
        && key != VK_LSHIFT && key != VK_RSHIFT
        && key != VK_LCONTROL && key != VK_RCONTROL
        && key != VK_LMENU && key != VK_RMENU
        && key != VK_LWIN && key != VK_RWIN);
}

static int KBShortcutConflict(TCHAR *section, const TCHAR *action, WORD hk,
                               TCHAR *conflict, size_t capacity)
{
    TCHAR *p;
    if (!hk) return 0;
    for (p = section; *p; p += lstrlen(p) + 1) {
        TCHAR *eq = lstrchr(p, TEXT('='));
        int duplicate;
        if (!eq) continue;
        *eq = 0;
        duplicate = lstrcmpi(p, action) && lstrcmpi(p, TEXT("UsePtWindow"))
            && (WORD)strtoi(eq + 1) == hk;
        if (duplicate) lstrcpy_s(conflict, capacity, p);
        *eq = TEXT('=');
        if (duplicate) return 1;
    }
    return 0;
}

/* Returns a Win32 error, zero only after a successful write AND readback.
   A failed registration/duplicate never changes the saved value. */
static DWORD KBShortcutSave(HWND probe, const TCHAR *ini, const TCHAR *action, WORD hk,
                            TCHAR *conflict, size_t capacity)
{
    TCHAR number[16], readback[16];
    TCHAR *section;
    DWORD error;
    const TCHAR *value = NULL;
    if (!action[0] || !KBShortcutValid(hk)) return ERROR_INVALID_DATA;
    section = KBShortcutSection(ini);
    if (!section) return ERROR_INSUFFICIENT_BUFFER;
    if (KBShortcutConflict(section, action, hk, conflict, capacity)) {
        free(section); return ERROR_HOTKEY_ALREADY_REGISTERED;
    }
    free(section);
    if (hk) {
        if (!RegisterHotKey(probe, 0xBFFF, HIBYTE(hk), LOBYTE(hk))) {
            error = GetLastError(); return error ? error : ERROR_HOTKEY_ALREADY_REGISTERED;
        }
        if (!UnregisterHotKey(probe, 0xBFFF)) {
            error = GetLastError(); return error ? error : ERROR_INVALID_DATA;
        }
        value = Uint2lStr(number, hk);
    }
    if (!WritePrivateProfileString(TEXT("KBShortcuts"), action, value, ini)) {
        return ERROR_WRITE_FAULT;
    }
    GetPrivateProfileString(TEXT("KBShortcuts"), action, TEXT(""), readback, 16, ini);
    if (hk ? lstrcmp(readback, value) != 0 : readback[0] != 0) return ERROR_WRITE_FAULT;
    return 0;
}
#endif
