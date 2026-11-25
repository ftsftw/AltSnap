/* Standalone Windows diagnostic; no production AltSnap code is changed. */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void snapshot(HWND hwnd)
{
    WINDOWPLACEMENT wp;
    DWORD pid = 0;
    char cls[128] = "";
    memset(&wp, 0, sizeof(wp));
    wp.length = sizeof(wp);
    GetWindowThreadProcessId(hwnd, &pid);
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (!GetWindowPlacement(hwnd, &wp)) {
        printf("snapshot failed error=%lu\n", GetLastError());
        return;
    }
    printf("hwnd=%p pid=%lu class=%s zoomed=%d show=%u ctrl=%d normal=%ld,%ld,%ld,%ld\n",
        (void *)hwnd, pid, cls, IsZoomed(hwnd), wp.showCmd,
        !!(GetAsyncKeyState(VK_CONTROL) & 0x8000),
        wp.rcNormalPosition.left, wp.rcNormalPosition.top,
        wp.rcNormalPosition.right, wp.rcNormalPosition.bottom);
    fflush(stdout);
}

/* Native smoke test, same-thread synthetic window; not an Explorer E2E. */
static int self_test(void)
{
    HWND hwnd = CreateWindowExA(0, "STATIC", "Maximize API smoke test",
        WS_OVERLAPPEDWINDOW, 100, 100, 400, 300, NULL, NULL, NULL, NULL);
    int mode, cycle, failed = 0;
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOWNORMAL);
    for (mode = 0; mode < 4; ++mode) {
        for (cycle = 0; cycle < 4; ++cycle) {
            int command = cycle % 2 ? SW_RESTORE : SW_MAXIMIZE;
            WINDOWPLACEMENT wp;
            MSG msg;
            if (mode == 0) PostMessage(hwnd, WM_SYSCOMMAND,
                command == SW_RESTORE ? SC_RESTORE : SC_MAXIMIZE, 0);
            else if (mode == 1) ShowWindowAsync(hwnd, command);
            else if (mode == 2) ShowWindow(hwnd, command);
            else {
                memset(&wp, 0, sizeof(wp));
                wp.length = sizeof(wp);
                if (!GetWindowPlacement(hwnd, &wp)) failed = 1;
                wp.showCmd = (UINT)command;
                if (!SetWindowPlacement(hwnd, &wp)) failed = 1;
            }
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (!!IsZoomed(hwnd) != (command == SW_MAXIMIZE)) failed = 1;
        }
    }
    DestroyWindow(hwnd);
    printf("native same-thread toggle smoke test: %s (no Explorer/modifier validation)\n",
        failed ? "FAILED" : "passed");
    return failed;
}

int main(int argc, char **argv)
{
    MSG msg;
    int mode, result;
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test();
    if (argc != 2) {
        puts("Usage: maximize-diagnostic.exe syscommand|async|show|placement");
        return 2;
    }
    if (!strcmp(argv[1], "syscommand")) mode = 0;
    else if (!strcmp(argv[1], "async")) mode = 1;
    else if (!strcmp(argv[1], "show")) mode = 2;
    else if (!strcmp(argv[1], "placement")) mode = 3;
    else return 2;
    /* Register all or exit; never run a partially registered experiment. */
    if (!RegisterHotKey(NULL, 1, MOD_CONTROL|MOD_ALT|MOD_NOREPEAT, 'W') ||
        !RegisterHotKey(NULL, 2, MOD_CONTROL|MOD_ALT|MOD_NOREPEAT, 'I') ||
        !RegisterHotKey(NULL, 3, MOD_CONTROL|MOD_ALT|MOD_NOREPEAT, 'Q')) {
        printf("hotkey registration failed: %lu; close AltSnap and retry\n", GetLastError());
        UnregisterHotKey(NULL, 1);
        UnregisterHotKey(NULL, 2);
        UnregisterHotKey(NULL, 3);
        return 1;
    }
    printf("Mode=%s. Ctrl+Alt+W toggle, Ctrl+Alt+I inspect, Ctrl+Alt+Q exit.\n", argv[1]);
    fflush(stdout);
    while ((result = GetMessage(&msg, NULL, 0, 0)) > 0) {
        HWND target;
        DWORD pid = 0;
        int command;
        WINDOWPLACEMENT wp;
        if (msg.message != WM_HOTKEY) continue;
        if (msg.wParam == 3) break;
        target = GetForegroundWindow();
        if (!target || !IsWindow(target)) continue;
        GetWindowThreadProcessId(target, &pid);
        if (pid == GetCurrentProcessId() || IsIconic(target) ||
            !(GetWindowLongPtr(target, GWL_STYLE) & WS_MAXIMIZEBOX)) {
            puts("target refused (self, minimized or no maximize box)");
            continue;
        }
        snapshot(target);
        if (msg.wParam == 2) continue;
        command = IsZoomed(target) ? SW_RESTORE : SW_MAXIMIZE;
        /* Capture once: never resolve foreground again for this operation. */
        if (mode == 0) {
            printf("PostMessage accepted=%d\n", PostMessage(target, WM_SYSCOMMAND,
                command == SW_RESTORE ? SC_RESTORE : SC_MAXIMIZE, 0));
        } else if (mode == 1) {
            printf("ShowWindowAsync accepted=%d\n", ShowWindowAsync(target, command));
        } else if (mode == 2) {
            printf("ShowWindow previously-visible=%d\n", ShowWindow(target, command));
        } else {
            memset(&wp, 0, sizeof(wp));
            wp.length = sizeof(wp);
            if (!GetWindowPlacement(target, &wp)) continue;
            wp.showCmd = (UINT)command;
            printf("SetWindowPlacement success=%d\n", SetWindowPlacement(target, &wp));
        }
        /* Immediate snapshot is not completion evidence for async requests. */
        snapshot(target);
    }
    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);
    UnregisterHotKey(NULL, 3);
    return result == -1 ? 1 : 0;
}
