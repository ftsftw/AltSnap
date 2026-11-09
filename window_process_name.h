/* Extracted from unfuck.h, under the same WTFPL license.
   Private compatibility helper. A NULL trace preserves the legacy API sequence,
   access rights, UWP child lookup and return values; no diagnostic queries run.
   Trace arguments are fixed stage codes and Win32 errors, never identities. */
#ifndef WINDOW_PROCESS_NAME_H
#define WINDOW_PROCESS_NAME_H

typedef void (*WindowProgNameTrace)(const char *stage, DWORD error);

static DWORD GetModuleFileNameExTraced(HANDLE process, TCHAR *name, DWORD size,
                                     WindowProgNameTrace trace)
{
    typedef DWORD (WINAPI *funk_t)(HANDLE, HMODULE, LPTSTR, DWORD);
    static funk_t funk = (funk_t)IPTR;
    if (funk == (funk_t)IPTR) {
#ifdef _UNICODE
        funk = (funk_t)LoadDLLProc("PSAPI.DLL", "GetModuleFileNameExW");
#else
        funk = (funk_t)LoadDLLProc("PSAPI.DLL", "GetModuleFileNameExA");
#endif
    }
    if (funk) {
        /* DLL/export resolution can itself overwrite LastError. */
        if (trace) SetLastError(0);
        return funk(process, NULL, name, size);
    }
    if (trace) SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

static DWORD GetProcessImageFileNameTraced(HANDLE process, TCHAR *name, DWORD size,
                                         WindowProgNameTrace trace)
{
    typedef DWORD (WINAPI *funk_t)(HANDLE, TCHAR *, DWORD);
    static funk_t funk = (funk_t)IPTR;
    if (funk == (funk_t)IPTR) {
#ifdef _UNICODE
        funk = (funk_t)LoadDLLProc("PSAPI.DLL", "GetProcessImageFileNameW");
#else
        funk = (funk_t)LoadDLLProc("PSAPI.DLL", "GetProcessImageFileNameA");
#endif
    }
    if (funk) {
        if (trace) SetLastError(0);
        return funk(process, name, size);
    }
    if (trace) SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

static DWORD GetWindowProgNameFromPidTraced(DWORD pid, TCHAR *title, size_t title_len,
                                          WindowProgNameTrace trace)
{
    DWORD ret = 0;
    HANDLE proc;
    if (trace) SetLastError(0);
    proc = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
    if (proc) {
        ret = GetModuleFileNameExTraced(proc, title, title_len, trace);
        if (!ret && trace) trace("process-module-failed", GetLastError());
    } else {
        if (trace) {
            trace("process-open-vm-failed", GetLastError());
            SetLastError(0);
        }
        proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!proc && trace) trace("process-open-query-failed", GetLastError());
    }
    if (!proc) return 0;

    if (!ret) {
        ret = GetProcessImageFileNameTraced(proc, title, title_len, trace);
        if (!ret && trace) trace("process-image-failed", GetLastError());
    }
    /* Observe a possible truncation without changing the existing helper's
       contract. No title/path bytes or lengths are emitted. */
    if (trace && ret >= title_len) trace("process-name-at-capacity", 0);
    CloseHandle(proc);
    return ret;
}

struct pid_pair_struct { DWORD parent_pid; DWORD child_pid; };
static BOOL CALLBACK GetWindowProgName_subwins_EnumProc(HWND hwnd, LPARAM lp)
{
    struct pid_pair_struct *pids = (struct pid_pair_struct *)lp;
    DWORD hwnd_pid = 0;
    if (GetWindowThreadProcessId(hwnd, &hwnd_pid) && hwnd_pid && hwnd_pid != pids->parent_pid) {
        pids->child_pid = hwnd_pid;
        return FALSE;
    }
    return TRUE;
}

/* ApplicationFrameHost can host a child from another process. Preserve the
   existing first-child policy, including falling back to the host's identity. */
static DWORD GetWindowProgNameSubWindowTraced(DWORD parentpid, HWND parenthwnd,
                    TCHAR *title, size_t title_len, WindowProgNameTrace trace)
{
    struct pid_pair_struct pids = { 0, 0 };
    pids.parent_pid = parentpid;
    EnumChildWindows(parenthwnd, GetWindowProgName_subwins_EnumProc, (LPARAM)&pids);
    if (!pids.child_pid) {
        if (trace) trace("process-host-no-child", 0);
    } else if (GetWindowProgNameFromPidTraced(pids.child_pid, title, title_len, trace)) {
        if (trace) trace("process-host-child-used", 0);
        return pids.child_pid;
    } else if (trace) trace("process-host-child-failed", 0);
    return 0;
}

static DWORD GetWindowProgNameFullPathTraced(HWND hwnd, TCHAR *title, size_t title_len,
                                           WindowProgNameTrace trace)
{
    DWORD pid = 0;
    const TCHAR *p;
    if (trace) SetLastError(0);
    if (!GetWindowThreadProcessId(hwnd, &pid)) {
        if (trace) trace("process-window-thread-failed", GetLastError());
        return 0;
    }
    if (!pid) {
        if (trace) trace("process-window-id-missing", 0);
        return 0;
    }
    if (!GetWindowProgNameFromPidTraced(pid, title, title_len, trace)) return 0;
    p = title + PathGetFileNameStart(title);
    if (!lstrcmpi(p, TEXT("ApplicationFrameHost.exe"))) {
        DWORD pid2 = GetWindowProgNameSubWindowTraced(pid, hwnd, title, title_len, trace);
        if (pid2) return pid2;
    }
    return pid;
}

static DWORD GetWindowProgNameFullPath(HWND hwnd, TCHAR *title, size_t title_len)
{
    return GetWindowProgNameFullPathTraced(hwnd, title, title_len, NULL);
}

static DWORD GetWindowProgNameTraced(HWND hwnd, TCHAR *title, size_t title_len,
                                   WindowProgNameTrace trace)
{
    DWORD pid = GetWindowProgNameFullPathTraced(hwnd, title, title_len, trace);
    if (pid) PathStripPathL(title);
    return pid;
}

static DWORD GetWindowProgName(HWND hwnd, TCHAR *title, size_t title_len)
{
    return GetWindowProgNameTraced(hwnd, title, title_len, NULL);
}
#endif
