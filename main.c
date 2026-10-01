// ============================================================
//  WingetStore — 简洁的 winget 应用市场（纯 Win32，单文件）
//  功能：搜索 / 应用详情（名字、开发商、上传者、版本、简介）/ 安装
//  编译：tcc -m64 -mwindows main.c -o WingetStore.exe -luser32 -lgdi32 -lcomctl32
//  声明：本文件由 AI（WorkBuddy Agent）根据用户功能需求自动生成，未做逐行人工审校。
// ============================================================
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include "icon_data.c"

// ---------------- commctrl 最小手工声明（TinyCC 无 commctrl.h） ----------------
#define ICC_LISTVIEW_CLASSES   0x00000001
#define LVS_REPORT             0x0001
#define LVS_SINGLESEL          0x0004
#define LVS_SHOWSELALWAYS      0x0008
#define LVS_EX_FULLROWSELECT   0x00000020
#define LVS_EX_DOUBLEBUFFER    0x00010000
#define LVCF_TEXT              0x0004
#define LVCF_WIDTH             0x0002
#define LVIF_TEXT              0x0001
#define LVIF_PARAM             0x0004
#define LVIS_SELECTED          0x0002
#define EM_SETCUEBANNER        (WM_USER + 6401)
#define EM_SETBKCOLOR          (WM_USER + 19)
#define LVM_FIRST              0x1000
#define LVM_DELETEALLITEMS           (LVM_FIRST + 9)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVM_INSERTITEMW              (LVM_FIRST + 77)
#define LVM_INSERTCOLUMNW            (LVM_FIRST + 97)
#define LVM_SETCOLUMNW               (LVM_FIRST + 96)
#define LVM_SETITEMTEXTW             (LVM_FIRST + 116)
#define NM_DBLCLK                    (0U - 3U)          /* NM_FIRST-3 */
#define LVN_FIRST                    (0U - 100U)
#define LVN_ITEMCHANGED              (LVN_FIRST - 1)
#define WC_LISTVIEWW                 L"SysListView32"

typedef struct { DWORD dwSize; DWORD dwICC; } MY_INITCC;
extern BOOL WINAPI InitCommonControlsEx(const MY_INITCC *);
typedef struct { UINT mask; int fmt; int cx; LPWSTR pszText; int cchTextMax;
                 int iSubItem; int iImage; int iOrder; int cxMin; int cxDefault;
                 BOOL cxIdeal; } MY_LVCOLUMNW;
typedef struct { UINT mask; int iItem; int iSubItem; UINT state; UINT stateMask;
                 LPWSTR pszText; int cchTextMax; int iImage; LPARAM lParam;
                 int iIndent; } MY_LVITEMW;
typedef struct { HWND hwndFrom; UINT_PTR idFrom; UINT code; } MY_NMHDR;
typedef struct { MY_NMHDR hdr; int iItem; int iSubItem; UINT uNewState;
                 UINT uOldState; UINT uChanged; POINT ptAction; LPARAM lParam; } MY_NMLISTVIEW;

#define ListView_InsertColumn(w, i, p)   (int)SendMessageW(w, LVM_INSERTCOLUMNW, (WPARAM)(i), (LPARAM)(p))
#define ListView_InsertItem(w, p)        (int)SendMessageW(w, LVM_INSERTITEMW, 0, (LPARAM)(p))
#define ListView_DeleteAllItems(w)       SendMessageW(w, LVM_DELETEALLITEMS, 0, 0)
#define ListView_SetExtendedListViewStyle(w, s) \
    (DWORD)SendMessageW(w, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(s))
static void ListView_SetItemText(HWND w, int i, int sub, wchar_t *t) {
    MY_LVITEMW v; ZeroMemory(&v, sizeof(v)); v.iSubItem = sub; v.pszText = t;
    SendMessageW(w, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&v);
}

// ---------------- 控件 ID ----------------
#define IDC_EDIT_SEARCH   101
#define IDC_BTN_SEARCH    102
#define IDC_LIST          103
#define IDC_BTN_DETAILS   104
#define IDC_BTN_INSTALL   105
#define IDC_ST_STATUS     106
#define IDC_GRP_DETAIL    107
#define IDC_ST_NAME       110
#define IDC_ST_PUB        111
#define IDC_ST_AUTHOR     112
#define IDC_ST_VER        113
#define IDC_ST_HOME       114
#define IDC_EDIT_DESC     115
#define IDC_ST_DESC_LBL   116
#define IDC_BTN_HOME      117

#define WM_WORKER_DONE    (WM_APP + 2)   // lParam=RESULT*(需释放)
#define K_SEARCH  0   // 搜索
#define K_SHOW    1   // 详情
#define K_VERSION 3   // 首页推荐：刷新版本号

// TinyCC 头文件缺少的部分：手工补充声明
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x00000008
#endif
#ifndef IMAGE_ICON
#define IMAGE_ICON        1
#endif
#ifndef LR_LOADFROMFILE
#define LR_LOADFROMFILE   0x00000010
#endif
#ifndef LR_DEFAULTSIZE
#define LR_DEFAULTSIZE    0x00000040
#endif
#ifndef ICON_SMALL
#define ICON_SMALL        0
#endif
#ifndef ICON_BIG
#define ICON_BIG          1
#endif
WINBASEAPI int WINAPI MultiByteToWideChar(UINT CodePage, DWORD dwFlags,
    LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
WINUSERAPI HANDLE WINAPI LoadImageW(HINSTANCE, LPCWSTR, UINT, int, int, UINT);
WINUSERAPI HICON WINAPI CreateIconFromResourceEx(PBYTE presbits, DWORD dwResSize,
    BOOL fIcon, DWORD dwVer, int cxDesired, int cyDesired, UINT Flags);

// 标题栏直接标明运行要求（winget 自身需要 Windows 10 1809 / 内部版本 17763 及以上）
#define MIN_WIN_BUILD 17763
static const wchar_t APP_TITLE[] =
    L"Winget 应用市场  —  运行要求：Windows 10 1809（内部版本 17763）或更高";

// ---------------- 系统版本自检 ----------------
typedef struct {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    wchar_t szCSDVersion[128];
} MY_OSVI;

static int GetWinVersion(DWORD *major, DWORD *build) {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    LONG (WINAPI *pRtlGetVersion)(MY_OSVI *);
    MY_OSVI vi;
    if (!nt) return 0;
    pRtlGetVersion = (LONG (WINAPI *)(MY_OSVI *))GetProcAddress(nt, "RtlGetVersion");
    if (!pRtlGetVersion) return 0;
    ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (pRtlGetVersion(&vi) != 0) return 0;
    *major = vi.dwMajorVersion;
    *build = vi.dwBuildNumber;
    return 1;
}

// ---------------- 全局 ----------------
static HWND g_hMain, g_hSearch, g_hBtnSearch, g_hList, g_hBtnDetails, g_hBtnInstall, g_hBtnHome;
static HWND g_hStatus, g_hGrp, g_hStName, g_hStPub, g_hStAuthor, g_hStVer, g_hStHome, g_hDesc, g_hDescLbl;
static HFONT g_hFont;
static int g_dpi = 96;

typedef struct {
    wchar_t *name, *id, *ver, *src;
    wchar_t *pub, *author, *desc, *home;
} PKG;

static PKG *g_items = NULL;
static int  g_count = 0, g_cap = 0;
static int  g_sel = -1;

// 首页推荐状态
static int      g_home   = 0;   // 1 = 当前列表是首页推荐
static int      g_refIdx = 0;   // 下一个待刷新版本的下标
static unsigned g_sess   = 1;   // 会话号：搜索/回首页都会 +1，用于丢弃过期工作线程结果

// ---------------- 小工具 ----------------
static int Scale(int x) { return MulDiv(x, g_dpi, 96); }

static wchar_t *xwcsdup(const wchar_t *s) {
    size_t n = (wcslen(s) + 1) * sizeof(wchar_t);
    wchar_t *p = (wchar_t *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void trim_w(wchar_t *s) {
    wchar_t *e, *p = s;
    while (*p == L' ' || *p == L'\t') p++;
    e = p + wcslen(p);
    while (e > p && (e[-1] == L' ' || e[-1] == L'\t' || e[-1] == L'\r' || e[-1] == L'\n')) *--e = 0;
    if (e != p) memmove(s, p, (wcslen(p) + 1) * sizeof(wchar_t));
    else if (p != s) *s = 0;
}

static void SetStatus(const wchar_t *t) { SetWindowTextW(g_hStatus, t); }

static void SetDetailStatic(HWND h, const wchar_t *label, const wchar_t *val) {
    wchar_t buf[1024];
    if (!val || !val[0]) val = L"—";
    _snwprintf(buf, 1023, L"%s：%s", label, val);
    buf[1023] = 0;
    SetWindowTextW(h, buf);
}

static void ClearDetails(void) {
    SetDetailStatic(g_hStName,   L"名字", L"");
    SetDetailStatic(g_hStPub,    L"开发商", L"");
    SetDetailStatic(g_hStAuthor, L"上传者/作者", L"");
    SetDetailStatic(g_hStVer,    L"版本", L"");
    SetDetailStatic(g_hStHome,   L"主页", L"");
    SetWindowTextW(g_hDesc, L"");
}

// ---------------- UTF-8 / GBK 转宽字符（自动识别） ----------------
static wchar_t *AnsiToWide(const char *src, int srclen) {
    int n;
    wchar_t *w;
    if (srclen < 0) srclen = (int)strlen(src);
    // 先按 UTF-8 严格解码
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, srclen, NULL, 0);
    if (n > 0) {
        w = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, src, srclen, w, n);
        w[n] = 0;
        return w;
    }
    // 失败则按本地 GBK 解码
    n = MultiByteToWideChar(936, 0, src, srclen, NULL, 0);
    if (n <= 0) return xwcsdup(L"");
    w = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
    MultiByteToWideChar(936, 0, src, srclen, w, n);
    w[n] = 0;
    return w;
}

// ---------------- 执行命令并捕获输出 ----------------
// 返回堆上宽字符串；失败返回 NULL
static wchar_t *RunCapture(const wchar_t *cmdline, DWORD timeout_ms) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    char *buf = NULL;
    size_t buflen = 0, bufcap = 0;
    wchar_t *result = NULL;
    DWORD deadline = GetTickCount() + timeout_ms;

    if (!CreatePipe(&rd, &wr, &sa, 0)) return NULL;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError  = wr;

    wchar_t *cmd = xwcsdup(cmdline);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        free(cmd);
        CloseHandle(rd); CloseHandle(wr);
        return NULL;
    }
    free(cmd);
    CloseHandle(wr);

    for (;;) {
        DWORD avail = 0, got = 0;
        if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL)) break;
        if (avail > 0) {
            if (buflen + avail + 1 > bufcap) {
                bufcap = (buflen + avail + 1) * 2;
                buf = (char *)realloc(buf, bufcap);
            }
            if (ReadFile(rd, buf + buflen, avail, &got, NULL) && got > 0) {
                buflen += got;
            } else break;
        } else {
            if (WaitForSingleObject(pi.hProcess, 50) != WAIT_TIMEOUT) {
                // 进程已退出，再读一次残余
                if (PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) && avail > 0) continue;
                break;
            }
            if (GetTickCount() > deadline) {
                TerminateProcess(pi.hProcess, 1);
                break;
            }
        }
        if (GetTickCount() > deadline) { TerminateProcess(pi.hProcess, 1); break; }
    }

    if (buf) {
        buf[buflen] = 0;
        result = AnsiToWide(buf, (int)buflen);
        free(buf);
    }
    CloseHandle(rd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}

// ---------------- 工作线程 ----------------
typedef struct { HWND hwnd; int kind; int idx; unsigned sess; wchar_t cmd[1]; } WORKCTX;
typedef struct { int kind; int idx; unsigned sess; wchar_t *out; } RESULT;

static DWORD WINAPI WorkerProc(LPVOID p) {
    WORKCTX *ctx = (WORKCTX *)p;
    RESULT *r = (RESULT *)malloc(sizeof(RESULT));
    r->kind = ctx->kind;
    r->idx  = ctx->idx;
    r->sess = ctx->sess;
    r->out = RunCapture(ctx->cmd, 60000);
    PostMessageW(ctx->hwnd, WM_WORKER_DONE, 0, (LPARAM)r);
    free(ctx);
    return 0;
}

static void StartWorker(HWND hwnd, int kind, int idx, unsigned sess, const wchar_t *cmd) {
    WORKCTX *ctx = (WORKCTX *)malloc(sizeof(WORKCTX) + (wcslen(cmd) + 1) * sizeof(wchar_t));
    ctx->hwnd = hwnd;
    ctx->kind = kind;
    ctx->idx  = idx;
    ctx->sess = sess;
    wcscpy(ctx->cmd, cmd);
    CloseHandle(CreateThread(NULL, 0, WorkerProc, ctx, 0, NULL));
}


// ---------------- 解析：winget search 表格 ----------------
// 字符显示宽度：中日韩全角算 2 格
static int CharW(wchar_t c) {
    if (c < 0x1100) return 1;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0x303E) ||
        (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) ||
        (c >= 0xA000 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6)) return 2;
    return 1;
}

static void ParseSearch(const wchar_t *text) {
    // 释放旧数据
    for (int i = 0; i < g_count; i++) {
        free(g_items[i].name); free(g_items[i].id); free(g_items[i].ver); free(g_items[i].src);
    }
    g_count = 0;
    ListView_DeleteAllItems(g_hList);
    ClearDetails();
    g_sel = -1;

    wchar_t *dup = xwcsdup(text);

    // 先把所有行收集起来
    wchar_t *lines[512];
    wchar_t *strip[512];
    int nlines = 0;
    for (wchar_t *t = wcstok(dup, L"\n"); t && nlines < 512; t = wcstok(NULL, L"\n"))
        lines[nlines++] = t;

    // 找制表线行（全为 '-'、空格、\r）
    int dashIdx = -1;
    for (int i = 0; i < nlines; i++) {
        int onlyDash = wcschr(lines[i], L'-') != NULL;
        if (!onlyDash) continue;
        for (const wchar_t *p = lines[i]; *p; p++)
            if (*p != L'-' && *p != L' ' && *p != L'\r' && *p != L'\t') { onlyDash = 0; break; }
        if (onlyDash) { dashIdx = i; break; }
    }

    if (dashIdx > 0) {
        // 表头 = 制表线的上一行；列起点 = 表头各词的显示宽度位置
        const wchar_t *hdr = lines[dashIdx - 1];
        int colStart[8]; int ncols = 0;
        int dw = 0, prevSpace = 1;
        for (const wchar_t *p = hdr; *p; p++) {
            if (*p == L' ' || *p == L'\t' || *p == L'\r') { prevSpace = 1; }
            else {
                if (prevSpace && ncols < 8) colStart[ncols++] = dw;
                prevSpace = 0;
            }
            dw += CharW(*p);
        }
        // 逐数据行按显示宽度切列
        for (int i = dashIdx + 1; i < nlines; i++) {
            wchar_t *ln = lines[i];
            int hasContent = 0;
            for (const wchar_t *p = ln; *p; p++)
                if (*p != L' ' && *p != L'\r' && *p != L'\t') { hasContent = 1; break; }
            if (!hasContent) break; // 空行 = 表格结束

            wchar_t cell[8][512];
            int cellLen[8] = {0};
            int cur = 0;
            int d = 0;
            for (wchar_t *p = ln; *p; p++) {
                while (cur + 1 < ncols && d >= colStart[cur + 1]) cur++;
                int cw = CharW(*p);
                if (cur < ncols && *p != L'\r' && cellLen[cur] < 510) {
                    cell[cur][cellLen[cur]++] = *p;
                }
                d += cw;
            }
            for (int c = 0; c < ncols; c++) { cell[c][cellLen[c]] = 0; trim_w(cell[c]); }

            if (cell[1][0]) { // ID 列非空才算数据行
                if (g_count >= g_cap) {
                    g_cap = g_cap ? g_cap * 2 : 64;
                    g_items = (PKG *)realloc(g_items, g_cap * sizeof(PKG));
                }
                PKG *it = &g_items[g_count];
                it->name = xwcsdup(cell[0]);
                it->id   = xwcsdup(cell[1]);
                it->ver  = xwcsdup(cell[2]);
                it->src  = xwcsdup(ncols >= 5 ? cell[4] : cell[3]);
                it->pub = it->author = it->desc = it->home = NULL;
                g_count++;
            }
        }
    }
    free(dup);

    MY_LVITEMW lvi;
    wchar_t status[64];
    for (int i = 0; i < g_count; i++) {
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = g_items[i].name;
        lvi.lParam = (LPARAM)i;
        ListView_InsertItem(g_hList, &lvi);
        ListView_SetItemText(g_hList, i, 1, g_items[i].id);
        ListView_SetItemText(g_hList, i, 2, g_items[i].ver);
        ListView_SetItemText(g_hList, i, 3, g_items[i].src);
    }
    _snwprintf(status, 63, L"共 %d 个结果，双击条目查看详情", g_count);
    status[63] = 0;
    SetStatus(status);
    if (g_count == 0) SetStatus(L"没有找到匹配的应用");
}

// ---------------- 解析：winget show 详情 ----------------
// 取 "已找到 名称 [id]" 的名称
static wchar_t *ParseFoundName(const wchar_t *line) {
    const wchar_t *p = wcsstr(line, L"已找到 ");
    if (!p) p = wcsstr(line, L"Found ");
    if (!p) return NULL;
    p += (p[0] == L'已') ? 4 : 6;
    const wchar_t *br = wcschr(p, L'[');
    if (!br) return NULL;
    while (br > p && (br[-1] == L' ' || br[-1] == L'\t')) br--;
    int n = (int)(br - p);
    if (n <= 0) return NULL;
    wchar_t *r = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
    wcsncpy(r, p, n);
    r[n] = 0;
    return r;
}

// label后紧跟冒号才算字段头；返回冒号后内容指针
static const wchar_t *LabelValue(const wchar_t *line, const wchar_t *label) {
    size_t n = wcslen(label);
    if (wcsncmp(line, label, n) != 0) return NULL;
    const wchar_t *p = line + n;
    if (*p == L'：') return p + 1;
    if (*p == L':')  return p + 1;
    return NULL;
}

static void ParseShow(int idx, const wchar_t *text) {
    if (idx < 0 || idx >= g_count) return;
    PKG *it = &g_items[idx];
    if (it->pub) free(it->pub);
    if (it->author) free(it->author);
    if (it->desc) free(it->desc);
    if (it->home) free(it->home);
    it->pub = it->author = it->desc = it->home = NULL;

    wchar_t *dup = xwcsdup(text);
    
    wchar_t *line = wcstok(dup, L"\n");
    int any = 0;
    while (line) {
        wchar_t *cr = wcschr(line, L'\r');
        if (cr) *cr = 0;
        if (!any) {
            wchar_t *nm = ParseFoundName(line);
            if (nm) { free(it->name); it->name = nm; any = 1; }
        }
        const wchar_t *v;
        if ((v = LabelValue(line, L"发布者")) || (v = LabelValue(line, L"Publisher"))) {
            free(it->pub); it->pub = xwcsdup(v); trim_w(it->pub);
        } else if ((v = LabelValue(line, L"作者")) || (v = LabelValue(line, L"Author"))) {
            free(it->author); it->author = xwcsdup(v); trim_w(it->author);
        } else if ((v = LabelValue(line, L"描述")) || (v = LabelValue(line, L"Description"))) {
            wchar_t *d = xwcsdup(v); trim_w(d); free(it->desc); it->desc = d;
        } else if ((v = LabelValue(line, L"主页")) || (v = LabelValue(line, L"Homepage"))) {
            free(it->home); it->home = xwcsdup(v); trim_w(it->home);
        } else if ((v = LabelValue(line, L"版本")) || (v = LabelValue(line, L"Version"))) {
            wchar_t *t = xwcsdup(v); trim_w(t);
            free(it->ver); it->ver = t;
        }
        line = wcstok(NULL, L"\n");
    }
    free(dup);

    if (!it->pub    && !it->author && !it->desc && !it->home) {
        if (idx == g_sel) SetStatus(L"获取详情失败（该包可能来自 msstore 源）");
        return;
    }
    if (idx != g_sel) return;   // 用户已经切到别的条目了，只存数据不动界面
    SetDetailStatic(g_hStName,   L"名字", it->name);
    SetDetailStatic(g_hStPub,    L"开发商", it->pub);
    SetDetailStatic(g_hStAuthor, L"上传者/作者", it->author);
    SetDetailStatic(g_hStVer,    L"版本", it->ver);
    SetDetailStatic(g_hStHome,   L"主页", it->home);
    SetWindowTextW(g_hDesc, it->desc ? it->desc : L"（无简介）");
    SetStatus(L"就绪");
}

// ---------------- 首页推荐 ----------------
// 内置精选清单（ID 均已核对存在于 winget 源），启动即显示，版本号后台逐个补全
typedef struct { const wchar_t *name, *id, *cat; } RECO;

static const RECO RECOS[] = {
    // 浏览器
    { L"Google Chrome",        L"Google.Chrome",             L"浏览器"   },
    { L"Microsoft Edge",       L"Microsoft.Edge",            L"浏览器"   },
    { L"Mozilla Firefox",      L"Mozilla.Firefox",           L"浏览器"   },
    // 办公与效率
    { L"WPS Office",           L"Kingsoft.WPSOffice",        L"办公效率" },
    { L"Everything 文件搜索",  L"voidtools.Everything",      L"办公效率" },
    { L"Microsoft PowerToys",  L"Microsoft.PowerToys",       L"办公效率" },
    { L"7-Zip 压缩",           L"7zip.7zip",                 L"办公效率" },
    { L"Bandizip 压缩",        L"Bandisoft.Bandizip",        L"办公效率" },
    { L"Notepad++ 编辑器",     L"Notepad++.Notepad++",       L"办公效率" },
    { L"SumatraPDF 阅读器",    L"SumatraPDF.SumatraPDF",     L"办公效率" },
    // 影音直播
    { L"VLC 播放器",           L"VideoLAN.VLC",              L"影音直播" },
    { L"PotPlayer",            L"Daum.PotPlayer",            L"影音直播" },
    { L"OBS Studio 录屏直播",  L"OBSProject.OBSStudio",      L"影音直播" },
    { L"网易云音乐",           L"NetEase.CloudMusic",        L"影音直播" },
    // 开发工具
    { L"Visual Studio Code",   L"Microsoft.VisualStudioCode",L"开发工具" },
    { L"Git",                  L"Git.Git",                   L"开发工具" },
    { L"Windows Terminal",     L"Microsoft.WindowsTerminal", L"开发工具" },
    { L"PowerShell 7",         L"Microsoft.Power" L"Shell", L"开发工具" },
    { L"Python 3.12",          L"Python.Python.3.12",        L"开发工具" },
    { L"Node.js (LTS)",        L"OpenJS.NodeJS.LTS",         L"开发工具" },
    { L"Go 语言",              L"GoLang.Go",                 L"开发工具" },
    { L"Rust (rustup)",        L"Rustlang.Rustup",           L"开发工具" },
    // 社交通讯
    { L"微信",                 L"Tencent.WeChat",            L"社交通讯" },
    { L"QQ",                   L"Tencent.QQ",                L"社交通讯" },
    { L"钉钉",                 L"Alibaba.DingTalk.Mainland", L"社交通讯" },
    { L"飞书",                 L"ByteDance.Feishu",          L"社交通讯" },
    { L"Discord",              L"Discord.Discord",           L"社交通讯" },
    // 游戏与安全
    { L"Steam",                L"Valve.Steam",               L"游戏娱乐" },
    { L"AdGuard 广告拦截",     L"AdGuard.AdGuard",           L"安全工具" },
};
static const int RECO_N = sizeof(RECOS) / sizeof(RECOS[0]);

// 切换列表某一列的标题（首页显示“分类”，搜索结果显示“来源”）
static void SetColText(int idx, const wchar_t *t) {
    MY_LVCOLUMNW col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT;
    col.pszText = (wchar_t *)t;
    SendMessageW(g_hList, LVM_SETCOLUMNW, (WPARAM)idx, (LPARAM)&col);
}

// 从 winget search 输出里取第一条数据行的“版本”列
static int ParseFirstRowVersion(const wchar_t *text, wchar_t *out, int outsz) {
    wchar_t *dup = xwcsdup(text);
    wchar_t *lines[256];
    int n = 0, got = 0, dash = -1;
    for (wchar_t *t = wcstok(dup, L"\n"); t && n < 256; t = wcstok(NULL, L"\n"))
        lines[n++] = t;

    for (int i = 0; i < n && dash < 0; i++) {
        int ok = wcschr(lines[i], L'-') != NULL;
        if (!ok) continue;
        for (const wchar_t *p = lines[i]; *p; p++)
            if (*p != L'-' && *p != L' ' && *p != L'\r' && *p != L'\t') { ok = 0; break; }
        if (ok) dash = i;
    }
    if (dash > 0) {
        const wchar_t *hdr = lines[dash - 1];
        int colStart[8], nc = 0, dw = 0, prevSpace = 1;
        for (const wchar_t *p = hdr; *p; p++) {
            if (*p == L' ' || *p == L'\t' || *p == L'\r') prevSpace = 1;
            else { if (prevSpace && nc < 8) colStart[nc++] = dw; prevSpace = 0; }
            dw += CharW(*p);
        }
        if (nc >= 3) {
            int lo = colStart[2], hi = (nc > 3) ? colStart[3] : 0x7fffffff;
            for (int i = dash + 1; i < n && !got; i++) {
                wchar_t buf[256];
                int bl = 0, d = 0, has = 0;
                const wchar_t *p;
                for (p = lines[i]; *p; p++)
                    if (*p != L' ' && *p != L'\r' && *p != L'\t') { has = 1; break; }
                if (!has) break;
                for (p = lines[i]; *p; p++) {
                    if (*p != L'\r' && d >= lo && d < hi && bl < 254) buf[bl++] = *p;
                    d += CharW(*p);
                }
                buf[bl] = 0;
                trim_w(buf);
                if (buf[0]) {
                    int k = 0;
                    while (buf[k] && k < outsz - 1) { out[k] = buf[k]; k++; }
                    out[k] = 0;
                    got = 1;
                }
            }
        }
    }
    free(dup);
    return got;
}

// 刷新下一个推荐项的版本号（串行，避免同时开一堆 winget 抢资源）
static void StartNextRefresh(void) {
    wchar_t cmd[1024];
    if (!g_home) return;
    if (g_refIdx >= g_count) {
        SetStatus(L"首页推荐 · 双击条目查看详情，或直接搜索");
        return;
    }
    _snwprintf(cmd, 1023,
        L"cmd.exe /c winget search --id \"%s\" -e --accept-source-agreements --disable-interactivity",
        g_items[g_refIdx].id);
    cmd[1023] = 0;
    StartWorker(g_hMain, K_VERSION, g_refIdx, g_sess, cmd);
}

static void ApplyVersion(int idx, const wchar_t *out) {
    if (idx < 0 || idx >= g_count) return;
    if (out) {
        wchar_t v[128];
        if (ParseFirstRowVersion(out, v, 128)) {
            free(g_items[idx].ver);
            g_items[idx].ver = xwcsdup(v);
            ListView_SetItemText(g_hList, idx, 2, v);
        }
    }
    g_refIdx = idx + 1;
    StartNextRefresh();
}

// 显示首页推荐
static void ShowHome(void) {
    for (int i = 0; i < g_count; i++) {
        free(g_items[i].name); free(g_items[i].id);
        free(g_items[i].ver);  free(g_items[i].src);
    }
    g_count = 0;
    ListView_DeleteAllItems(g_hList);
    ClearDetails();
    g_sel  = -1;
    g_home = 1;
    g_sess++;
    g_refIdx = 0;
    SetColText(3, L"分类");

    for (int i = 0; i < RECO_N; i++) {
        if (g_count >= g_cap) {
            g_cap = g_cap ? g_cap * 2 : 64;
            g_items = (PKG *)realloc(g_items, g_cap * sizeof(PKG));
        }
        PKG *it = &g_items[g_count];
        it->name   = xwcsdup(RECOS[i].name);
        it->id     = xwcsdup(RECOS[i].id);
        it->ver    = xwcsdup(L"…");
        it->src    = xwcsdup(RECOS[i].cat);
        it->pub = it->author = it->desc = it->home = NULL;
        g_count++;
    }

    MY_LVITEMW lvi;
    for (int i = 0; i < g_count; i++) {
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.pszText = g_items[i].name;
        lvi.lParam = (LPARAM)i;
        ListView_InsertItem(g_hList, &lvi);
        ListView_SetItemText(g_hList, i, 1, g_items[i].id);
        ListView_SetItemText(g_hList, i, 2, g_items[i].ver);
        ListView_SetItemText(g_hList, i, 3, g_items[i].src);
    }
    SetStatus(L"首页推荐 · 正在获取版本信息…（双击条目查看详情）");
    StartNextRefresh();
}

// ---------------- 动作 ----------------
static void DoSearch(void) {
    wchar_t q[256];
    GetWindowTextW(g_hSearch, q, 256);
    trim_w(q);
    if (!q[0]) { ShowHome(); return; }   // 空关键词 = 回首页推荐
    // 过滤引号，防止拼接命令出错
    for (wchar_t *p = q; *p; p++)
        if (*p == L'"' || *p == L'\'' || *p == L'&' || *p == L'|' || *p == L'>') *p = L' ';
    trim_w(q);
    if (!q[0]) return;

    wchar_t cmd[1024];
    _snwprintf(cmd, 1023,
        L"cmd.exe /c winget search \"%s\" --accept-source-agreements --disable-interactivity",
        q);
    cmd[1023] = 0;
    g_home = 0;        // 离开首页，放弃后台版本刷新
    g_sess++;
    SetColText(3, L"来源");
    SetStatus(L"正在搜索…（首次运行 winget 可能较慢）");
    ListView_DeleteAllItems(g_hList);
    StartWorker(g_hMain, K_SEARCH, 0, g_sess, cmd);
}

static void DoShow(void) {
    if (g_sel < 0 || g_sel >= g_count) { SetStatus(L"请先在列表中选择一个应用"); return; }
    wchar_t cmd[1024];
    _snwprintf(cmd, 1023,
        L"cmd.exe /c winget show --id \"%s\" -e --accept-source-agreements --disable-interactivity",
        g_items[g_sel].id);
    cmd[1023] = 0;
    SetStatus(L"正在获取详情…");
    StartWorker(g_hMain, K_SHOW, g_sel, g_sess, cmd);
}

static void DoInstall(void) {
    if (g_sel < 0 || g_sel >= g_count) { SetStatus(L"请先在列表中选择一个应用"); return; }
    wchar_t msg[512];
    _snwprintf(msg, 511,
        L"确定要安装「%s」吗？\n\n会打开一个命令窗口显示安装过程，结束后窗口会停住（按任意键关闭），\n方便查看结果。若弹出管理员授权请点“是”。",
        g_items[g_sel].name);
    msg[511] = 0;
    if (MessageBoxW(g_hMain, msg, L"确认安装", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;

    // 结束后 pause 住窗口，避免出错信息一闪而过看不到
    wchar_t cmd[1400];
    _snwprintf(cmd, 1399,
        L"cmd.exe /c title winget 安装 - %s & echo [1/2] 正在执行: winget install --id %s -e & echo. "
        L"& winget install --id \"%s\" -e --accept-package-agreements --accept-source-agreements "
        L"& echo. & echo ================ 安装流程已结束（上面是完整输出） ================ "
        L"& echo 按任意键关闭本窗口 & pause > nul",
        g_items[g_sel].name, g_items[g_sel].id, g_items[g_sel].id);
    cmd[1399] = 0;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    wchar_t *c = xwcsdup(cmd);
    if (CreateProcessW(NULL, c, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        SetStatus(L"已打开安装窗口，安装结束后窗口会停住等你看结果");
    } else {
        SetStatus(L"启动安装失败");
    }
    free(c);
}

static void LoadDetailUI(void) {
    if (g_sel < 0 || g_sel >= g_count) return;
    PKG *it = &g_items[g_sel];
    SetDetailStatic(g_hStName,   L"名字", it->name);
    SetDetailStatic(g_hStPub,    L"开发商", it->pub);
    SetDetailStatic(g_hStAuthor, L"上传者/作者", it->author);
    SetDetailStatic(g_hStVer,    L"版本", it->ver);
    SetDetailStatic(g_hStHome,   L"主页", it->home);
    SetWindowTextW(g_hDesc, it->desc ? it->desc : L"（双击列表条目加载详情）");
}

// ---------------- 搜索框回车 ----------------
static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    WNDPROC old = (WNDPROC)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_CHAR && w == VK_RETURN) { DoSearch(); return 0; }
    if (m == WM_KEYDOWN && w == VK_RETURN) { DoSearch(); return 0; }
    return CallWindowProcW(old, h, m, w, l);
}

// ---------------- 布局 ----------------
static void Layout(void) {
    RECT rc; GetClientRect(g_hMain, &rc);
    int W = rc.right, H = rc.bottom, M = Scale(10);
    int y = M, r = W - M;
    // 搜索行：右侧按钮从右往左排（搜索 / 查看详情 / 安装 / 首页）
    MoveWindow(g_hBtnSearch,  r - Scale(80), y, Scale(80), Scale(28), TRUE); r -= Scale(86);
    MoveWindow(g_hBtnDetails, r - Scale(92), y, Scale(92), Scale(28), TRUE); r -= Scale(98);
    MoveWindow(g_hBtnInstall, r - Scale(80), y, Scale(80), Scale(28), TRUE); r -= Scale(86);
    MoveWindow(g_hBtnHome,    r - Scale(64), y, Scale(64), Scale(28), TRUE); r -= Scale(70);
    MoveWindow(g_hSearch,     M, y + Scale(2), r - M, Scale(24), TRUE);
    y += Scale(36);
    // 列表
    int lh = (H - y - Scale(24)) * 45 / 100;
    MoveWindow(g_hList, M, y, W - M * 2, lh, TRUE);
    y += lh + Scale(8);
    // 详情组
    MoveWindow(g_hGrp, M, y, W - M * 2, H - y - Scale(8) - Scale(22), TRUE);
    int ix = M + Scale(14), iw = W - M * 2 - Scale(28);
    int ry = y + Scale(20);
    MoveWindow(g_hStName,   ix, ry, iw, Scale(18), TRUE); ry += Scale(21);
    MoveWindow(g_hStPub,    ix, ry, iw, Scale(18), TRUE); ry += Scale(21);
    MoveWindow(g_hStAuthor, ix, ry, iw, Scale(18), TRUE); ry += Scale(21);
    MoveWindow(g_hStVer,    ix, ry, iw, Scale(18), TRUE); ry += Scale(21);
    MoveWindow(g_hStHome,   ix, ry, iw, Scale(18), TRUE); ry += Scale(24);
    MoveWindow(g_hDescLbl,  ix, ry, iw, Scale(16), TRUE); ry += Scale(18);
    int bh = y + (H - y - Scale(8) - Scale(22)) - Scale(14) - ry;
    MoveWindow(g_hDesc, ix, ry, iw, bh, TRUE);
    // 状态栏
    MoveWindow(g_hStatus, M, H - Scale(20) - Scale(4), W - M * 2, Scale(20), TRUE);
}

static HWND Mk(HWND parent, const wchar_t *cls, const wchar_t *txt, DWORD st, int id,
               int x, int y, int w, int h) {
    HWND hC = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hC, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hC;
}

// ---------------- 窗口过程 ----------------
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hMain = h;
        HDC dc = GetDC(h);
        g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(h, dc);
        g_hFont = CreateFontW(-Scale(15), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");

        g_hSearch = Mk(h, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_SEARCH, 0, 0, 0, 0);
        SendMessageW(g_hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"输入应用名，如：7zip");
        g_hBtnSearch  = Mk(h, L"BUTTON", L"搜索",   BS_PUSHBUTTON | WS_TABSTOP, IDC_BTN_SEARCH,  0, 0, 0, 0);
        g_hBtnHome    = Mk(h, L"BUTTON", L"首页",   BS_PUSHBUTTON | WS_TABSTOP, IDC_BTN_HOME,    0, 0, 0, 0);
        g_hBtnDetails = Mk(h, L"BUTTON", L"查看详情", BS_PUSHBUTTON | WS_TABSTOP, IDC_BTN_DETAILS, 0, 0, 0, 0);
        g_hBtnInstall = Mk(h, L"BUTTON", L"安装",   BS_PUSHBUTTON | WS_TABSTOP, IDC_BTN_INSTALL, 0, 0, 0, 0);

        g_hList = Mk(h, WC_LISTVIEWW, L"", WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, IDC_LIST, 0, 0, 0, 0);
        ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        MY_LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = (wchar_t *)L"名称";   col.cx = Scale(280); ListView_InsertColumn(g_hList, 0, &col);
        col.pszText = (wchar_t *)L"ID";     col.cx = Scale(230); ListView_InsertColumn(g_hList, 1, &col);
        col.pszText = (wchar_t *)L"版本";   col.cx = Scale(120); ListView_InsertColumn(g_hList, 2, &col);
        col.pszText = (wchar_t *)L"来源";   col.cx = Scale(90);  ListView_InsertColumn(g_hList, 3, &col);

        g_hGrp      = Mk(h, L"BUTTON", L"应用详情", BS_GROUPBOX, IDC_GRP_DETAIL, 0, 0, 0, 0);
        g_hStName   = Mk(h, L"STATIC", L"", 0, 0, 0, 0, 0, 0);
        g_hStPub    = Mk(h, L"STATIC", L"", 0, 0, 0, 0, 0, 0);
        g_hStAuthor = Mk(h, L"STATIC", L"", 0, 0, 0, 0, 0, 0);
        g_hStVer    = Mk(h, L"STATIC", L"", 0, 0, 0, 0, 0, 0);
        g_hStHome   = Mk(h, L"STATIC", L"", 0, 0, 0, 0, 0, 0);
        g_hDescLbl  = Mk(h, L"STATIC", L"简介：", 0, 0, 0, 0, 0, 0);
        g_hDesc     = Mk(h, L"EDIT", L"（双击列表条目加载详情）",
                         WS_BORDER | WS_VSCROLL | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL,
                         IDC_EDIT_DESC, 0, 0, 0, 0);
        SendMessageW(g_hDesc, EM_SETBKCOLOR, 0, (LPARAM)GetSysColor(COLOR_WINDOW)); // 明亮背景

        g_hStatus = Mk(h, L"STATIC", L"就绪。输入关键词开始搜索。", 0, 0, 0, 0, 0, 0);
        ClearDetails();

        // 搜索框回车
        SetWindowLongPtrW(g_hSearch, GWLP_USERDATA,
                          (LONG_PTR)SetWindowLongPtrW(g_hSearch, GWLP_WNDPROC, (LONG_PTR)EditProc));

        // 从内存加载嵌入的 ICO 图标（单文件，无需外部 icon.ico）
        HICON hIcon = CreateIconFromResourceEx((PBYTE)ICON_DATA, (DWORD)ICON_SIZE,
                                               TRUE, 0x00030000, 0, 0, LR_DEFAULTCOLOR);
        if (hIcon) {
            SendMessageW(g_hMain, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(g_hMain, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }

        // 启动即显示首页推荐
        ShowHome();
        return 0;
    }
    case WM_SIZE:
        Layout();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_BTN_SEARCH:  DoSearch(); break;
        case IDC_BTN_HOME:    ShowHome(); break;
        case IDC_BTN_DETAILS: DoShow();   break;
        case IDC_BTN_INSTALL: DoInstall(); break;
        }
        return 0;
    case WM_NOTIFY: {
        MY_NMHDR *nm = (MY_NMHDR *)l;
        if (nm->idFrom == IDC_LIST && nm->code == NM_DBLCLK) {
            MY_NMLISTVIEW *ia = (MY_NMLISTVIEW *)l;
            if (ia->iItem >= 0) {
                g_sel = ia->iItem;
                LoadDetailUI();
                DoShow();
            }
        } else if (nm->idFrom == IDC_LIST && nm->code == LVN_ITEMCHANGED) {
            MY_NMLISTVIEW *lv = (MY_NMLISTVIEW *)l;
            if (lv->uNewState & LVIS_SELECTED) {
                g_sel = lv->iItem;
                LoadDetailUI();
            }
        }
        return 0;
    }
    case WM_WORKER_DONE: {
        RESULT *r = (RESULT *)l;
        if (!r) return 0;
        if (r->kind == K_SEARCH) {
            if (r->out) ParseSearch(r->out);
            else        SetStatus(L"搜索失败：无法启动 winget");
        } else if (r->kind == K_SHOW) {
            if (r->out) ParseShow(r->idx, r->out);
            else if (r->idx == g_sel) SetStatus(L"获取详情失败");
        } else if (r->kind == K_VERSION) {
            // 会话号不匹配 = 用户已经去搜索 / 重新回首页了，直接丢弃
            if (r->sess == g_sess && g_home) ApplyVersion(r->idx, r->out);
        }
        if (r->out) free(r->out);
        free(r);
        return 0;
    }
    case WM_SETFOCUS:
        SetFocus(g_hSearch);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------- 入口 ----------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show) {
    // 高 DPI
    {
        HMODULE u32 = GetModuleHandleW(L"user32.dll");
        BOOL (WINAPI *pSetCtx)(void *) =
            (BOOL (WINAPI *)(void *))GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (pSetCtx) pSetCtx((void *)-4);   // PER_MONITOR_AWARE_V2
        else {
            BOOL (WINAPI *pSetAware)(void) =
                (BOOL (WINAPI *)(void))GetProcAddress(u32, "SetProcessDPIAware");
            if (pSetAware) pSetAware();
        }
    }
    MY_INITCC icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WingetStoreWnd";
    RegisterClassW(&wc);

    RECT rc = { 0, 0, 0, 0 };
    // 先以 96dpi 计算窗口大小，WM_CREATE 里拿到真实 dpi 后 WM_SIZE 会重排
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int W = 920 - rc.left + rc.right, H = 640 - rc.top + rc.bottom;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    if (sx < 0) sx = 0; if (sy < 0) sy = 0;

    HWND h = CreateWindowExW(0, L"WingetStoreWnd", APP_TITLE, WS_OVERLAPPEDWINDOW,
                             sx, sy, W, H, NULL, NULL, hInst, NULL);
    ShowWindow(h, show);
    UpdateWindow(h);

    // 系统版本自检：低于要求时提示（仍可打开界面，但 winget 可能不可用）
    {
        DWORD vMaj = 0, vBuild = 0;
        if (GetWinVersion(&vMaj, &vBuild) &&
            (vMaj < 10 || (vMaj == 10 && vBuild < MIN_WIN_BUILD))) {
            wchar_t m[320];
            _snwprintf(m, 319,
                L"本程序通过 winget 搜索和安装应用，\n"
                L"需要 Windows 10 1809（内部版本 %d）或更高版本。\n\n"
                L"当前系统：%lu.%lu\n\n"
                L"仍可打开界面，但搜索、查看详情和安装可能不可用。",
                MIN_WIN_BUILD, (unsigned long)vMaj, (unsigned long)vBuild);
            m[319] = 0;
            MessageBoxW(h, m, L"系统版本低于要求", MB_OK | MB_ICONWARNING);
        }
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
