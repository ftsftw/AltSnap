#include <assert.h>
#include <stdio.h>
typedef unsigned HWND;
#define MOD_CONTROL 2
#define SW_MAXIMIZE 3
#define SW_RESTORE 9
static int valid = 1, zoomed, calls, requested;
static HWND destination;
static int IsWindow(HWND hwnd) { return valid && hwnd == 42; }
static int IsZoomed(HWND hwnd) { assert(hwnd == 42); return zoomed; }
static int ShowWindowAsync(HWND hwnd, int command)
{ destination = hwnd; requested = command; ++calls; return 1; }
#include "../hotkey_maximize.h"
int main(void)
{
    unsigned mods;
    for (mods = 0; mods < 16; ++mods) {
        assert(!!HotkeyMaximizeUsesAsync(1, 1, mods, 0) == !!(mods & MOD_CONTROL));
        assert(HotkeyMaximizeUsesAsync(1, 1, mods, 1));
        assert(!HotkeyMaximizeUsesAsync(0, 1, mods, 1));
        assert(!HotkeyMaximizeUsesAsync(1, 0, mods, 1));
    }
    HotkeyToggleMaxRestore(42);
    assert(destination == 42 && requested == SW_MAXIMIZE && calls == 1);
    zoomed = 1; /* Target has processed first request. */
    HotkeyToggleMaxRestore(42);
    assert(requested == SW_RESTORE && calls == 2);
    zoomed = 0;
    HotkeyToggleMaxRestore(42);
    HotkeyToggleMaxRestore(42); /* Delayed target: known coalescing limitation. */
    assert(requested == SW_MAXIMIZE && calls == 4);
    valid = 0;
    HotkeyToggleMaxRestore(42);
    assert(calls == 4);
    valid = 1; zoomed = 1; /* Reused handle: no old intent carried forward. */
    HotkeyToggleMaxRestore(42);
    assert(requested == SW_RESTORE);
    HotkeyToggleMaxRestore(99); /* Never substitute another foreground window. */
    assert(calls == 5 && destination == 42);
    puts("hotkey maximize policy passed (Win32 stubs; async coalescing documented)");
    return 0;
}
