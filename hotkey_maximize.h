#ifndef HOTKEY_MAXIMIZE_H
#define HOTKEY_MAXIMIZE_H
/* lParam identifies modifiers at registration; async key state also covers
   an extra physically held Ctrl. No cached HWND or requested window state. */
static int HotkeyMaximizeUsesAsync(int registered, int maximize,
                                  unsigned modifiers, int ctrl_down)
{
    return registered && maximize && ((modifiers & MOD_CONTROL) || ctrl_down);
}
static void HotkeyToggleMaxRestore(HWND target)
{
    if (!IsWindow(target)) return;
    /* Observe actual state on each press. Two requests before the target has
       processed the first may coalesce. Do not guess state across HWND reuse. */
    ShowWindowAsync(target, IsZoomed(target) ? SW_RESTORE : SW_MAXIMIZE);
}
#endif
