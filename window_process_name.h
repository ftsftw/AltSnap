/* Extracted from unfuck.h, under the same WTFPL license.
   Private compatibility helper. Try legacy PSAPI lookups first, then a limited
   query on Vista+; never elevate. A NULL trace performs no diagnostic queries.
   Trace arguments are fixed stage codes and Win32 errors, never identities. */
#ifndef WINDOW_PROCESS_NAME_H
#define WINDOW_PROCESS_NAME_H

typedef void (*WindowProgNameTrace)(const char *stage, DWORD error);

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

/* Counts are TCHARs, including the terminator in the capacity. Reject partial
   or malformed results before any caller strips the path or checks a list. */
static void WindowProgNamePrepare(TCHAR *name, DWORD size)
{
    DWORD i;
    for (i = 0; i < size; ++i) name[i] = TEXT('\1');
}

static DWORD WindowProgNameValidate(TCHAR *name, DWORD size, DWORD length,
                                   WindowProgNameTrace trace)
{
    DWORD i;
    if (!length) return 0;
    if (length >= size) {
        if (trace) trace("process-name-at-capacity", 0);
        return 0;
    }
    if (name[length] != TEXT('\0')) return 0;
    for (i = 0; i < length; ++i) if (!name[i]) return 0;
    if (!name[PathGetFileNameStart(name)]) return 0;
    return length;
}

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
    if (!title || !title_len) return 0;
    title[0] = TEXT('\0');
    if (title_len < 2 || title_len > (DWORD)-1) return 0;
    if (trace) SetLastError(0);
    proc = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
    if (proc) {
        WindowProgNamePrepare(title, (DWORD)title_len);
        ret = GetModuleFileNameExTraced(proc, title, title_len, trace);
        if (!ret && trace) trace("process-module-failed", GetLastError());
        ret = WindowProgNameValidate(title, (DWORD)title_len, ret, trace);
    } else {
        if (trace) {
            trace("process-open-vm-failed", GetLastError());
            SetLastError(0);
        }
        proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!proc && trace) trace("process-open-query-failed", GetLastError());
    }
    if (proc && !ret) {
        WindowProgNamePrepare(title, (DWORD)title_len);
        ret = GetProcessImageFileNameTraced(proc, title, title_len, trace);
        if (!ret && trace) trace("process-image-failed", GetLastError());
        ret = WindowProgNameValidate(title, (DWORD)title_len, ret, trace);
    }
    if (proc) CloseHandle(proc);
    if (!ret) {
        typedef BOOL (WINAPI *funk_t)(HANDLE, DWORD, LPTSTR, DWORD *);
        static funk_t funk = (funk_t)IPTR;
        if (funk == (funk_t)IPTR) {
#ifdef _UNICODE
            funk = (funk_t)LoadDLLProc("KERNEL32.DLL", "QueryFullProcessImageNameW");
#else
            funk = (funk_t)LoadDLLProc("KERNEL32.DLL", "QueryFullProcessImageNameA");
#endif
        }
        if (!funk) {
            if (trace) trace("process-limited-api-missing", ERROR_PROC_NOT_FOUND);
        } else {
            if (trace) SetLastError(0);
            proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!proc) {
                if (trace) trace("process-open-limited-failed", GetLastError());
            } else {
                DWORD size = (DWORD)title_len;
                WindowProgNamePrepare(title, size);
                if (trace) SetLastError(0);
                if (!funk(proc, 0, title, &size)) {
                    if (trace) trace("process-limited-image-failed", GetLastError());
                } else {
                    ret = WindowProgNameValidate(title, (DWORD)title_len, size, trace);
                    if (trace) trace(ret ? "process-limited-used" : "process-limited-invalid", 0);
                }
                CloseHandle(proc);
            }
        }
    }
    if (!ret) title[0] = TEXT('\0');
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
    if (!title || !title_len) return 0;
    title[0] = TEXT('\0');
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
        /* A failed child query may have overwritten the host path. Re-read it
           rather than returning partial child bytes as the host's identity. */
        if (!title[0] && !GetWindowProgNameFromPidTraced(pid, title, title_len, trace)) return 0;
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
