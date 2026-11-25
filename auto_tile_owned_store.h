/* SPDX-License-Identifier: GPL-3.0-or-later
   Fixed-size local consent records. No HWND/PID/TID is serializable here. */
#ifndef AUTO_TILE_OWNED_STORE_H
#define AUTO_TILE_OWNED_STORE_H
#define ATG_OWNED_SLOTS 8
#define ATG_OWNED_CLASS 64
#define ATG_OWNED_STYLE_MASK (WS_CHILD|WS_POPUP|WS_CAPTION|WS_THICKFRAME)
#define ATG_OWNED_EXSTYLE_MASK (WS_EX_APPWINDOW|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_DLGMODALFRAME)
typedef struct {
    int enabled, valid;
    TCHAR path[MAX_PATH], digest[66];
    TCHAR candidate_class[ATG_OWNED_CLASS], owner_class[ATG_OWNED_CLASS];
    DWORD style, exstyle, owner_style, owner_exstyle;
} ATGOwnedProfile;

static void ATGOwnedSection(unsigned slot, TCHAR *section)
{
    wsprintf(section, TEXT("AutoTileOwned%u"), slot+1);
}

static int ATGOwnedReadText(const TCHAR *ini, const TCHAR *section,
                            const TCHAR *key, TCHAR *out, unsigned capacity)
{
    DWORD n = GetPrivateProfileString(section, key, TEXT(""), out, capacity, ini);
    return n && n < capacity-1;
}

static int ATGOwnedHex(const TCHAR *text, unsigned length, DWORD *value)
{
    unsigned i;
    DWORD n = 0;
    for (i=0; i<length; ++i) {
        unsigned c = (unsigned)text[i], v;
        if (c >= '0' && c <= '9') v=c-'0';
        else if (c >= 'a' && c <= 'f') v=c-'a'+10;
        else return 0;
        n=(n<<4)|v;
    }
    if (text[length]) return 0;
    if (value) *value=n;
    return 1;
}

static int ATGOwnedLocalPath(const TCHAR *path)
{
    return ((path[0]>='A' && path[0]<='Z') || (path[0]>='a' && path[0]<='z'))
        && path[1]==':' && path[2]=='\\' && path[3];
}

/* Native class names are application-controlled input. Do not let a newline
   become an INI key (in particular Enabled=1), or quotes change round trips. */
static int ATGOwnedSafeText(const TCHAR *text, unsigned capacity)
{
    unsigned i;
    for (i=0; i<capacity; ++i) {
        if (!text[i]) return i!=0;
        if ((text[i]>0 && text[i]<' ') || text[i]=='"') return 0;
    }
    return 0;
}

static int ATGOwnedProfileRead(const TCHAR *ini, unsigned slot, ATGOwnedProfile *p)
{
    TCHAR section[32], text[32];
    mem00(p, sizeof *p);
    ATGOwnedSection(slot, section);
    GetPrivateProfileString(section, TEXT("Enabled"), TEXT("0"), text, ARR_SZ(text), ini);
    /* Unknown nonempty enable values also block automatic tiling. */
    p->enabled = lstrcmp(text, TEXT("0")) != 0;
    if (lstrcmp(text, TEXT("0")) && lstrcmp(text, TEXT("1"))) return 0;
    if (!ATGOwnedReadText(ini, section, TEXT("Schema"), text, ARR_SZ(text))
        || lstrcmp(text, TEXT("1"))) return 0;
    if (!ATGOwnedReadText(ini, section, TEXT("Topology"), text, ARR_SZ(text))
        || lstrcmp(text, TEXT("caption-app-same-tool-self-v1"))) return 0;
    if (!ATGOwnedReadText(ini, section, TEXT("Path"), p->path, ARR_SZ(p->path))
        || !ATGOwnedLocalPath(p->path) || !ATGOwnedSafeText(p->path, ARR_SZ(p->path))
        || !ATGOwnedReadText(ini, section, TEXT("SHA256"), p->digest, ARR_SZ(p->digest))
        || !ATGOwnedHex(p->digest, 64, NULL)
        || !ATGOwnedReadText(ini, section, TEXT("Class"), p->candidate_class, ARR_SZ(p->candidate_class))
        || !ATGOwnedReadText(ini, section, TEXT("OwnerClass"), p->owner_class, ARR_SZ(p->owner_class))
        || !ATGOwnedSafeText(p->candidate_class, ARR_SZ(p->candidate_class))
        || !ATGOwnedSafeText(p->owner_class, ARR_SZ(p->owner_class))) return 0;
#define ATG_READ_STYLE(key, field) \
    if (!ATGOwnedReadText(ini, section, TEXT(key), text, ARR_SZ(text)) \
        || !ATGOwnedHex(text, 8, &p->field)) return 0
    ATG_READ_STYLE("Style", style);
    ATG_READ_STYLE("ExStyle", exstyle);
    ATG_READ_STYLE("OwnerStyle", owner_style);
    ATG_READ_STYLE("OwnerExStyle", owner_exstyle);
#undef ATG_READ_STYLE
    /* This schema permits only the already supported strict topology. */
    if (p->style != (WS_CAPTION|WS_THICKFRAME) || p->exstyle != WS_EX_APPWINDOW
        || (p->owner_style & ~((DWORD)ATG_OWNED_STYLE_MASK))
        || (p->owner_style & WS_CHILD) || p->owner_exstyle != WS_EX_TOOLWINDOW
        || !lstrcmp(p->candidate_class, TEXT("#32770"))) return 0;
    p->valid=1;
    return 1;
}

static int ATGOwnedProfileEnable(const TCHAR *ini, unsigned slot, int enabled)
{
    TCHAR section[32];
    if (enabled) {
        ATGOwnedProfile p;
        if (!ATGOwnedProfileRead(ini, slot, &p)) return 0;
    }
    ATGOwnedSection(slot, section);
    return WritePrivateProfileString(section, TEXT("Enabled"), enabled ? TEXT("1") : TEXT("0"), ini);
}

static int ATGOwnedProfileDelete(const TCHAR *ini, unsigned slot)
{
    TCHAR section[32];
    ATGOwnedSection(slot, section);
    /* Disable first, so a failed delete cannot leave an active stale record. */
    return ATGOwnedProfileEnable(ini, slot, 0)
        && WritePrivateProfileString(section, NULL, NULL, ini);
}

static int ATGOwnedProfileSave(const TCHAR *ini, unsigned slot, const ATGOwnedProfile *p)
{
    TCHAR section[32], value[16];
    ATGOwnedProfile check;
    if (!ATGOwnedSafeText(p->path, ARR_SZ(p->path))
        || !ATGOwnedSafeText(p->candidate_class, ARR_SZ(p->candidate_class))
        || !ATGOwnedSafeText(p->owner_class, ARR_SZ(p->owner_class))
        || !ATGOwnedHex(p->digest, 64, NULL)) return 0;
    ATGOwnedSection(slot, section);
    /* Interrupted/partial registration is always disabled and schema-invalid. */
    if (!ATGOwnedProfileEnable(ini, slot, 0)
        || !WritePrivateProfileString(section, TEXT("Schema"), TEXT("0"), ini)) return 0;
#define ATG_WRITE(key, val) if (!WritePrivateProfileString(section, TEXT(key), val, ini)) return 0
    ATG_WRITE("Path", p->path);
    ATG_WRITE("SHA256", p->digest);
    ATG_WRITE("Class", p->candidate_class);
    ATG_WRITE("OwnerClass", p->owner_class);
    ATG_WRITE("Topology", TEXT("caption-app-same-tool-self-v1"));
#define ATG_WRITE_STYLE(key, field) \
    wsprintf(value, TEXT("%08lx"), (unsigned long)p->field); ATG_WRITE(key, value)
    ATG_WRITE_STYLE("Style", style);
    ATG_WRITE_STYLE("ExStyle", exstyle);
    ATG_WRITE_STYLE("OwnerStyle", owner_style);
    ATG_WRITE_STYLE("OwnerExStyle", owner_exstyle);
#undef ATG_WRITE_STYLE
    ATG_WRITE("Schema", TEXT("1"));
#undef ATG_WRITE
    return ATGOwnedProfileRead(ini, slot, &check) && !check.enabled
        && !lstrcmp(check.path, p->path) && !lstrcmp(check.digest, p->digest)
        && !lstrcmp(check.candidate_class, p->candidate_class) && !lstrcmp(check.owner_class, p->owner_class)
        && check.style==p->style && check.exstyle==p->exstyle
        && check.owner_style==p->owner_style && check.owner_exstyle==p->owner_exstyle;
}
#endif
