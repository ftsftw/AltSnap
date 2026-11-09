/* SPDX-License-Identifier: GPL-3.0-or-later
   Combobox stores canonical identities separately from translated labels. */
#ifndef ALTSNAP_KB_SHORTCUTS_UI_H
#define ALTSNAP_KB_SHORTCUTS_UI_H

static int KBShortcutAddAction(HWND control, const actiondl_t *actions,
                               size_t count, const TCHAR *action)
{
    size_t i;
    for (i = 0; i < count; ++i)
        if (!lstrcmpi(action, actions[i].action)) return (int)i;
    return (int)CB_AddStringIfNotPresentAndGetIdx(control, action);
}

static int KBShortcutAction(HWND control, const actiondl_t *actions,
                            size_t count, TCHAR *out, size_t capacity)
{
    int i;
    out[0] = 0;
    i = CB_GetTextAndIdx(control, out, capacity);
    if (i < 0)
        i = (int)SendMessage(control, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)out);
    if (i >= 0 && (size_t)i < count) lstrcpy_s(out, capacity, actions[i].action);
    return i;
}

static int KBShortcutFill(HWND control, const actiondl_t *actions, size_t count,
                          const TCHAR *ini)
{
    TCHAR *section = KBShortcutSection(ini), *p;
    size_t i;
    if (!section) return 0;
    CB_ResetContent(control);
    for (i = 0; i < count; ++i) {
        TCHAR label[256];
        lstrcpy_noaccel(label, ARR_SZ(label), L10NSTR(actions[i].l10nidx));
        CB_AddString(control, label);
    }
    for (p = section; *p; p += lstrlen(p) + 1) {
        TCHAR *eq = lstrchr(p, TEXT('='));
        if (!eq) continue;
        *eq = 0;
        if (lstrcmpi(p, TEXT("UsePtWindow"))) KBShortcutAddAction(control, actions, count, p);
        *eq = TEXT('=');
    }
    free(section);
    SendMessage(control, CB_SETDROPPEDWIDTH, 360, 0);
    return 1;
}
#endif
