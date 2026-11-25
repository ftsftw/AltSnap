/* SPDX-License-Identifier: GPL-3.0-or-later
   Included by config.c. UI values are local, never debugger output. */
static ATGSelectedOwned atg_identified_pin;
static TCHAR atg_identified_class[ATG_OWNED_CLASS], atg_identified_owner_class[ATG_OWNED_CLASS];
static int atg_identified_valid;

static void AutoTileOwnedUIClear(void)
{
    atg_identified_valid=0;
    mem00(&atg_identified_pin, sizeof atg_identified_pin);
    mem00(atg_identified_class, sizeof atg_identified_class);
    mem00(atg_identified_owner_class, sizeof atg_identified_owner_class);
}

static unsigned AutoTileOwnedUISlot(HWND dialog)
{
    LRESULT slot=SendDlgItemMessage(dialog, IDC_OWNED_LIST, LB_GETCURSEL, 0, 0);
    return slot>=0 && slot<ATG_OWNED_SLOTS ? (unsigned)slot : 0;
}

static void AutoTileOwnedUIRefresh(HWND dialog, unsigned selection)
{
    ATGOwnedProfile p;
    TCHAR line[MAX_PATH+64], detail[768], section[32], exists[2];
    unsigned i;
    SendDlgItemMessage(dialog, IDC_OWNED_LIST, LB_RESETCONTENT, 0, 0);
    for (i=0; i<ATG_OWNED_SLOTS; ++i) {
        ATGOwnedProfileRead(inipath, i, &p);
        ATGOwnedSection(i, section);
        GetPrivateProfileString(section, TEXT("Path"), TEXT(""), exists, ARR_SZ(exists), inipath);
        wsprintf(line, TEXT("%u [%s] %s"), i+1,
            p.enabled ? (p.valid ? TEXT("ON") : TEXT("INVALID - blocks tiling")) : TEXT("off"),
            p.valid ? p.path+PathGetFileNameStart(p.path) : exists[0] ? TEXT("invalid record") : TEXT("empty"));
        SendDlgItemMessage(dialog, IDC_OWNED_LIST, LB_ADDSTRING, 0, (LPARAM)line);
    }
    SendDlgItemMessage(dialog, IDC_OWNED_LIST, LB_SETCURSEL, selection, 0);
    ATGOwnedProfileRead(inipath, selection, &p);
    if (p.valid) wsprintf(detail, TEXT("%s\r\nSHA-256: %s\r\nClass: %s\r\nOwner class: %s\r\nTopology: caption/app, same-process enabled tool owner, roots/popup=self"),
        p.path, p.digest, p.candidate_class, p.owner_class);
    else lstrcpy(detail, TEXT("Empty or invalid profile. Delete an invalid record before registering again. An enabled invalid record blocks automatic tiling."));
    SetDlgItemText(dialog, IDC_OWNED_DETAIL, detail);
    EnableWindow(GetDlgItem(dialog, IDC_OWNED_REGISTER), atg_identified_valid);
    EnableWindow(GetDlgItem(dialog, IDC_OWNED_TOGGLE), p.valid || p.enabled);
}

static INT_PTR CALLBACK AutoTileOwnedDialog(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    if (msg==WM_INITDIALOG) { AutoTileOwnedUIRefresh(dialog, 0); return TRUE; }
    if (msg==WM_DESTROY) { AutoTileOwnedUIClear(); return FALSE; }
    if (msg==WM_COMMAND) {
        unsigned slot=AutoTileOwnedUISlot(dialog);
        int id=LOWORD(wParam), ok=0;
        ATGOwnedProfile *p;
        if (id==IDCANCEL) { EndDialog(dialog, 0); return TRUE; }
        if (id==IDC_OWNED_LIST && HIWORD(wParam)==LBN_SELCHANGE) {
            AutoTileOwnedUIRefresh(dialog, slot); return TRUE;
        }
        if (id!=IDC_OWNED_REGISTER && id!=IDC_OWNED_TOGGLE && id!=IDC_OWNED_DELETE) return FALSE;
        p=(ATGOwnedProfile *)calloc(1, sizeof *p);
        if (!p) return TRUE;
        ATGOwnedProfileRead(inipath, slot, p);
        if (id==IDC_OWNED_REGISTER) {
            if (MessageBox(dialog, TEXT("Register the window most recently chosen with Identify window?\n\nClose all its dialogs first. This records explicit trust in this application build; an indistinguishable custom dialog remains a risk. The selected slot will be replaced and saved DISABLED."),
                           TEXT("Register owned main window"), MB_OKCANCEL|MB_ICONWARNING)!=IDOK) goto done;
            if (!atg_identified_valid || !AutoTileOwnedTopologyValid(&atg_identified_pin, 0)
                || !ATGOwnedEnroll(atg_identified_pin.hwnd, p)
                || lstrcmp(p->candidate_class, atg_identified_class)
                || lstrcmp(p->owner_class, atg_identified_owner_class)) goto failed;
            ok=ATGOwnedProfileSave(inipath, slot, p);
            AutoTileOwnedUIClear();
        } else if (id==IDC_OWNED_TOGGLE) {
            if (p->enabled) ok=ATGOwnedProfileEnable(inipath, slot, 0);
            else {
                ATGOwnedImage image;
                TCHAR digest[65];
                if (!p->valid || !ATGOwnedImageOpen(p->path, digest, &image)) goto failed;
                ok=!lstrcmp(digest, p->digest);
                ATGOwnedImageClose(&image);
                if (!ok) goto failed;
                if (MessageBox(dialog, TEXT("Enable explicit trust for this saved application build?\n\nNormal AutoTileGrid will include a matching owned window without focusing it. Unrecognized build/class/topology or visible dialogs cancel the layout. An indistinguishable custom dialog can still match. Re-register after an application update."),
                               TEXT("Enable owned-window profile"), MB_OKCANCEL|MB_ICONWARNING)!=IDOK) goto done;
                ok=ATGOwnedProfileEnable(inipath, slot, 1);
            }
        } else if (MessageBox(dialog, TEXT("Delete the selected local profile?"), TEXT("Owned-window profiles"), MB_OKCANCEL|MB_ICONQUESTION)==IDOK)
            ok=ATGOwnedProfileDelete(inipath, slot);
        else goto done;
        if (!ok) goto failed;
        PostMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
        AutoTileOwnedUIRefresh(dialog, slot);
        goto done;
failed:
        /* A failed/partial save may have disabled a previous record. Reload it. */
        PostMessage(g_hwnd, WM_UPDATESETTINGS, 0, 0);
        MessageBox(dialog, TEXT("Profile operation failed. Nothing was enabled automatically.\n\nUse Identify window again with dialogs closed, check the selected window is stable and enabled, and keep the EXE on a readable local drive (maximum 128 MiB). Check INI write access. A changed build requires registration again."),
                   TEXT("Owned-window profile refused"), MB_OK|MB_ICONWARNING);
        AutoTileOwnedUIRefresh(dialog, slot);
done:
        free(p); return TRUE;
    }
    return FALSE;
}
