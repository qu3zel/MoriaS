// M.O.R.I.A. - Doors of Durin  |  Sleep timer that lives in the system tray
// Kaynak dosyalar UTF-8'dir (MSVC için /utf-8 gerekir; CMakeLists.txt bunu ayarlar).
// Lisans: MIT. Ayrıntılar için README.md.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601          // Windows 7 ve üzeri
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>
#include <time.h>
#include <math.h>
#include <cmath>

#include "door_art.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

// ---------------------------------------------------------------- Renkler
#define COLOR_BG RGB(12, 18, 24)
#define COLOR_HOVER_BG RGB(24, 38, 52)
#define COLOR_ITHILDIN RGB(135, 225, 255)
#define COLOR_RED RGB(255, 90, 80)
#define COLOR_AMBER RGB(255, 150, 70)
#define COLOR_TEXT RGB(210, 235, 255)
#define COLOR_MUTED RGB(60, 90, 120)
#define COLOR_BTN_BG RGB(16, 26, 36)

// ---------------------------------------------------------------- Kimlikler
#define ID_INPUT 101
#define ID_BTN_ACTION 102
#define ID_BTN_SETTINGS 104
#define ID_BTN_LANG 105
#define ID_BTN_STARTUP 106
#define ID_CHIP0 110                 // 110..114
#define ID_TIMER 1001
#define ID_HOVER_TIMER 1002
#define ID_ANIM_TIMER 1003
#define ID_AUTOHIDE 1004

#define WM_TRAYICON (WM_USER + 1)
#define WM_SHOWFLYOUT (WM_APP + 2)

#define IDM_OPEN 201
#define IDM_Q30 202
#define IDM_Q60 203
#define IDM_Q90 204
#define IDM_CANCEL 205
#define IDM_EXIT 206

#define MAX_MINUTES 43200            // 30 gün

// ---------------------------------------------------------------- Yerleşim (taban birimler, DPI ile çarpılır)
static const int W_BASE = 360;
static const int H_COL = 480;        // ayarlar kapalıyken yükseklik
static const int H_EXP = 612;        // ayarlar açıkken yükseklik
static const int Y_TITLE = 104, Y_SUB = 150, Y_TIME = 174, Y_BAR = 230, Y_STATUS = 238;
static const int Y_CHIPS = 268, Y_LABEL = 304, Y_EDIT = 322, Y_ACTION = 364, Y_SETBTN = 420;
static const int Y_SEP = 452, Y_LANG = 460, Y_START = 494, Y_OPLBL = 530, Y_OPSLIDER = 562;
static const int PRESETS[5] = { 15, 30, 45, 60, 90 };

// ---------------------------------------------------------------- Genel durum
HWND hMain = NULL, hInput = NULL, hBtnAction = NULL, hBtnSettings = NULL, hBtnLang = NULL, hBtnStartup = NULL;
HWND hChip[5] = {};
HFONT hFontTitle = NULL, hFontTime = NULL, hFontEdit = NULL, hFontMed = NULL, hFontSmall = NULL, hFontChip = NULL;
HICON hIconIdle = NULL, hIconActive = NULL, hIconBig = NULL;
NOTIFYICONDATAW nid = {};

int currentStatus = 0;               // 0 hazır, 1 sayıyor, 2 mellon, 3 iptal, 4 kapanıyor
std::wstring statusText;
bool isEnglish = false, isStartupEnabled = false, showSettings = false, isTimerActive = false;
int remainingSeconds = 0, totalSeconds = 0, lastMinutes = 60;
time_t endTime = 0;
int currentHoverId = 0;
int opacityVal = 90;
bool isDraggingOpacity = false;

float dpiScale = 1.0f, g_s = 1.0f;
POINT anchorPt = { 0, 0 };
bool flyoutVisible = false;
ULONGLONG lastHideTick = 0, animStart = 0;
int animTx = 0, animTy = 0, animOffX = 0, animOffY = 0;
UINT g_msgTaskbarCreated = 0;
std::wstring fontPath;
bool haveRingbearer = false;

static bool FontAvailable(const wchar_t* face) {
    HDC dc = GetDC(NULL);
    HFONT f = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, face);
    HGDIOBJ old = SelectObject(dc, f);
    wchar_t got[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, got);
    SelectObject(dc, old);
    DeleteObject(f);
    ReleaseDC(NULL, dc);
    return _wcsicmp(got, face) == 0;
}

static inline int PX(float v) { return (int)(v * g_s + 0.5f); }

// =====================================================================
//  DİL / METİN
// =====================================================================
enum {
    S_SUB, S_START, S_CANCEL, S_SETTINGS, S_LANG, S_STARTUP, S_MINLABEL, S_CLOSEAT, S_OPACITY, S_UNIT,
    S_M_OPEN, S_M_CANCEL, S_M_EXIT, S_EXIT_Q, S_READY, S_MELLON, S_CANCELLED, S_CLOSING, S_TIPREADY, S_TIPLEFT,
    S_COUNT
};

static const wchar_t* STR[S_COUNT][2] = {
    { L"DURİN'İN KAPILARI - UYKU PROTOKOLÜ", L"DOORS OF DURIN - SLEEP PROTOCOL" },
    { L"Söyle dost ve öyle gir.\n(Uykuyu başlat)", L"Speak friend, and enter.\n(Start Sleep)" },
    { L"Protokolü İptal Et", L"Cancel Protocol" },
    { L"Ayarlar", L"Settings" },
    { L"Dil: Türkçe", L"Language: English" },
    { L"Windows ile Başla: ", L"Run at Startup: " },
    { L"Dakika (veya 'mellon')", L"Minutes (or 'mellon')" },
    { L"Kapılar şu saatte kapanır: ", L"The doors close at " },
    { L"Ithildin Opaklığı: ", L"Ithildin Opacity: " },
    { L"dk", L"min" },
    { L"Kapıyı Aç", L"Open the Doors" },
    { L"Protokolü İptal Et", L"Cancel Protocol" },
    { L"Çıkış", L"Exit" },
    { L"Zamanlayıcı hâlâ çalışıyor.\n\nEvet: Kapatmayı iptal et ve çık\nHayır: Zamanlayıcı kalsın, sadece çık\nİptal: Vazgeç",
      L"The timer is still running.\n\nYes: Cancel the shutdown and exit\nNo: Keep the timer, just exit\nCancel: Stay" },
    { L"Sistem hazır. Kapılar kapalı.", L"System ready. Doors closed." },
    { L"Dost de ve gir. (Protokoller iptal edildi)", L"Speak friend, and enter. (Doors unlocked)" },
    { L"Protokol iptal edildi. Kapılar açık.", L"Protocol cancelled. Doors remain open." },
    { L"Kapılar kapanıyor...", L"The doors are closing..." },
    { L"M.O.R.I.A. - Hazır", L"M.O.R.I.A. - Ready" },
    { L"M.O.R.I.A. - Kalan: ", L"M.O.R.I.A. - Left: " }
};

static std::wstring Tr(int id) { return STR[id][isEnglish ? 1 : 0]; }
static std::wstring OnOff(bool on) { return on ? (isEnglish ? L"ON" : L"AÇIK") : (isEnglish ? L"OFF" : L"KAPALI"); }
static std::wstring PercentText(int v) { return isEnglish ? (std::to_wstring(v) + L"%") : (L"%" + std::to_wstring(v)); }

static std::wstring FormatHMS(int totalSec) {
    if (totalSec < 0) totalSec = 0;
    wchar_t buf[64];
    _snwprintf(buf, 64, L"%02d:%02d:%02d", totalSec / 3600, (totalSec % 3600) / 60, totalSec % 60);
    return buf;
}

static std::wstring FormatShort(int totalSec) {
    if (totalSec < 0) totalSec = 0;
    wchar_t buf[64];
    if (totalSec >= 3600) _snwprintf(buf, 64, L"%d:%02d:%02d", totalSec / 3600, (totalSec % 3600) / 60, totalSec % 60);
    else _snwprintf(buf, 64, L"%02d:%02d", totalSec / 60, totalSec % 60);
    return buf;
}

static std::wstring FormatClock(time_t t) {
    struct tm lt = {};
#if defined(_MSC_VER)
    localtime_s(&lt, &t);
#else
    { struct tm* p = localtime(&t); if (p) lt = *p; }
#endif
    wchar_t buf[16];
    _snwprintf(buf, 16, L"%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

static void UpdateStatusText() {
    switch (currentStatus) {
        case 0: statusText = Tr(S_READY); break;
        case 1: statusText = Tr(S_CLOSEAT) + FormatClock(endTime); break;
        case 2: statusText = Tr(S_MELLON); break;
        case 3: statusText = Tr(S_CANCELLED); break;
        case 4: statusText = Tr(S_CLOSING); break;
    }
}

// Giriş kutusundan dakika oku (geçersizse 60). 'mellon' ise isMellon=true.
static int ReadMinutesFromEdit(bool* isMellon) {
    wchar_t buf[32] = {};
    if (hInput) GetWindowTextW(hInput, buf, 32);
    std::wstring t = buf;
    size_t b = t.find_first_not_of(L" \t");
    size_t e = t.find_last_not_of(L" \t");
    t = (b == std::wstring::npos) ? L"" : t.substr(b, e - b + 1);
    for (auto& c : t) c = (wchar_t)towlower(c);
    if (isMellon) *isMellon = (t == L"mellon");
    wchar_t* endp = NULL;
    long m = wcstol(t.c_str(), &endp, 10);
    if (t.empty() || *endp != L'\0' || m <= 0) m = 60;
    if (m > MAX_MINUTES) m = MAX_MINUTES;
    return (int)m;
}

// =====================================================================
//  SİSTEM / KAYIT DEFTERİ
// =====================================================================
static void RunCommandSilent(const std::wstring& cmd) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::wstring buf = cmd;
    if (CreateProcessW(NULL, &buf[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// shutdown.exe'yi System32'den tam yolla çağır (PATH/çalışma dizini hilelerine karşı)
static void RunShutdown(const std::wstring& args) {
    wchar_t sysDir[MAX_PATH] = {};
    UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring exe = (n > 0 && n < MAX_PATH) ? (std::wstring(sysDir) + L"\\shutdown.exe") : std::wstring(L"shutdown.exe");
    RunCommandSilent(L"\"" + exe + L"\" " + args);
}

// Eski sürümlerden (FridayUyku) ayarları ve Windows ile başlatma kaydını taşır
static void MigrateLegacySettings() {
    HKEY hNew;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MORIA", 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hNew, &disp) == ERROR_SUCCESS) {
        if (disp == REG_CREATED_NEW_KEY) {
            HKEY hOld;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\FridayUyku", 0, KEY_READ, &hOld) == ERROR_SUCCESS) {
                const wchar_t* names[2] = { L"Opacity", L"Lang" };
                for (int i = 0; i < 2; i++) {
                    DWORD v = 0, sz = sizeof(v);
                    if (RegQueryValueExW(hOld, names[i], NULL, NULL, (BYTE*)&v, &sz) == ERROR_SUCCESS)
                        RegSetValueExW(hNew, names[i], 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
                }
                RegCloseKey(hOld);
            }
        }
        RegCloseKey(hNew);
    }
    HKEY hRun;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_SET_VALUE, &hRun) == ERROR_SUCCESS) {
        DWORD type = 0, size = 0;
        if (RegQueryValueExW(hRun, L"FridayUykuProtokolu", NULL, &type, NULL, &size) == ERROR_SUCCESS) {
            RegDeleteValueW(hRun, L"FridayUykuProtokolu");
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(path) + L"\" --tray";
            RegSetValueExW(hRun, L"MORIA", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(hRun);
    }
}

static void LoadSettings() {
    MigrateLegacySettings();
    // İlk çalıştırmada sistem diline göre varsayılan dil
    isEnglish = (PRIMARYLANGID(GetUserDefaultUILanguage()) != LANG_TURKISH);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MORIA", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        auto rd = [&](const wchar_t* name, int& dst) {
            DWORD val = 0, size = sizeof(DWORD);
            if (RegQueryValueExW(hKey, name, NULL, NULL, (BYTE*)&val, &size) == ERROR_SUCCESS) dst = (int)val;
        };
        int lang = isEnglish ? 1 : 0;
        rd(L"Opacity", opacityVal);
        rd(L"Lang", lang);
        rd(L"LastMinutes", lastMinutes);
        isEnglish = (lang != 0);
        RegCloseKey(hKey);
    }
    if (opacityVal < 30) opacityVal = 30;
    if (opacityVal > 100) opacityVal = 100;
    if (lastMinutes < 1 || lastMinutes > MAX_MINUTES) lastMinutes = 60;

    isStartupEnabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = 0;
        if (RegQueryValueExW(hKey, L"MORIA", NULL, &type, NULL, &size) == ERROR_SUCCESS) isStartupEnabled = true;
        RegCloseKey(hKey);
    }
}

static void SaveSettings() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MORIA", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val;
        val = opacityVal; RegSetValueExW(hKey, L"Opacity", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = isEnglish ? 1 : 0; RegSetValueExW(hKey, L"Lang", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = lastMinutes; RegSetValueExW(hKey, L"LastMinutes", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

// Çalışan geri sayımı kaydet: uygulama kapanıp açılsa bile kaldığı yerden devam eder
// Sistemin açılış zamanı damgası: bilgisayar yeniden başlatıldıysa eski geri sayım sürdürülmez
static time_t BootStamp() { return time(NULL) - (time_t)(GetTickCount64() / 1000); }

// Çalışan geri sayımı kaydet: uygulama kapanıp açılsa bile kaldığı yerden devam eder
static void SavePending(bool active) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MORIA", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        ULONGLONG end = active ? (ULONGLONG)endTime : 0;
        ULONGLONG boot = active ? (ULONGLONG)BootStamp() : 0;
        DWORD total = active ? (DWORD)totalSeconds : 0;
        RegSetValueExW(hKey, L"PendingEnd", 0, REG_QWORD, (const BYTE*)&end, sizeof(end));
        RegSetValueExW(hKey, L"PendingBoot", 0, REG_QWORD, (const BYTE*)&boot, sizeof(boot));
        RegSetValueExW(hKey, L"PendingTotal", 0, REG_DWORD, (const BYTE*)&total, sizeof(total));
        RegCloseKey(hKey);
    }
}

static bool LoadPending() {
    bool ok = false;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MORIA", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        ULONGLONG end = 0, boot = 0;
        DWORD total = 0;
        DWORD s1 = sizeof(end), s2 = sizeof(total), s3 = sizeof(boot);
        if (RegQueryValueExW(hKey, L"PendingEnd", NULL, NULL, (BYTE*)&end, &s1) == ERROR_SUCCESS &&
            RegQueryValueExW(hKey, L"PendingTotal", NULL, NULL, (BYTE*)&total, &s2) == ERROR_SUCCESS &&
            RegQueryValueExW(hKey, L"PendingBoot", NULL, NULL, (BYTE*)&boot, &s3) == ERROR_SUCCESS) {
            time_t now = time(NULL);
            long long drift = (long long)boot - (long long)BootStamp();
            if (drift < 0) drift = -drift;
            if (end > (ULONGLONG)now + 1 && drift <= 120) {
                endTime = (time_t)end;
                remainingSeconds = (int)(end - (ULONGLONG)now);
                totalSeconds = total > 0 ? (int)total : remainingSeconds;
                ok = true;
            }
        }
        RegCloseKey(hKey);
    }
    if (!ok) SavePending(false);     // bayat kaydı temizle
    return ok;
}

static void ToggleStartupStatus() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (!isStartupEnabled) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            // --tray: Windows ile açılınca panel görünmeden doğrudan tepsiye yerleşir
            std::wstring cmd = L"\"" + std::wstring(path) + L"\" --tray";
            if (RegSetValueExW(hKey, L"MORIA", 0, REG_SZ, (const BYTE*)cmd.c_str(),
                               (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS) isStartupEnabled = true;
        } else {
            RegDeleteValueW(hKey, L"MORIA");
            isStartupEnabled = false;
        }
        RegCloseKey(hKey);
    }
}

// =====================================================================
//  PROGRAMATİK TEPSİ SİMGESİ (kapı kemeri)
// =====================================================================
static HICON MakeDoorIcon(int n, COLORREF glow) {
    HDC hs = GetDC(NULL);
    HDC dcC = CreateCompatibleDC(hs), dcM = CreateCompatibleDC(hs);
    HBITMAP bmC = CreateCompatibleBitmap(hs, n, n);
    HBITMAP bmM = CreateBitmap(n, n, 1, 1, NULL);
    HGDIOBJ oC = SelectObject(dcC, bmC), oM = SelectObject(dcM, bmM);

    RECT r = { 0, 0, n, n };
    FillRect(dcM, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));   // beyaz = saydam
    FillRect(dcC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

    float u = n / 32.0f;
    int x1 = (int)(5 * u), x2 = (int)(27 * u), yTop = (int)(3 * u), yBot = (int)(29 * u);
    int w = x2 - x1, yMid = yTop + w / 2;

    {   // maske: kapı şekli opak
        HGDIOBJ ob = SelectObject(dcM, GetStockObject(BLACK_BRUSH));
        HGDIOBJ op = SelectObject(dcM, GetStockObject(BLACK_PEN));
        Ellipse(dcM, x1 - 1, yTop - 1, x2 + 1, yTop + w + 1);
        Rectangle(dcM, x1 - 1, yMid, x2 + 1, yBot + 1);
        SelectObject(dcM, op); SelectObject(dcM, ob);
    }
    {   // renk: koyu dolgu + parlak çerçeve + küçük yıldız
        HBRUSH fill = CreateSolidBrush(COLOR_BG);
        HGDIOBJ ob = SelectObject(dcC, fill);
        HGDIOBJ op = SelectObject(dcC, GetStockObject(NULL_PEN));
        Ellipse(dcC, x1, yTop, x2 + 1, yTop + w + 1);
        Rectangle(dcC, x1, yMid, x2 + 1, yBot + 1);
        SelectObject(dcC, op); SelectObject(dcC, ob); DeleteObject(fill);

        SelectObject(dcC, GetStockObject(NULL_BRUSH));
        {
            PenScope pen(dcC, (int)(2.2f * u + 0.5f), glow);
            MoveToEx(dcC, x1, yBot, NULL); LineTo(dcC, x1, yMid);
            MoveToEx(dcC, x2, yBot, NULL); LineTo(dcC, x2, yMid);
            MoveToEx(dcC, x1, yBot, NULL); LineTo(dcC, x2, yBot);
            Arc(dcC, x1, yTop, x2, yTop + w, x2, yMid, x1, yMid);
        }
        {
            BrushScope br(dcC, glow);
            PenScope pen(dcC, 1, glow);
            DrawStarShape(dcC, (x1 + x2) / 2.0f, yMid + (yBot - yMid) * 0.2f, 5.5f * u, 2.2f * u, 4, -PI_F / 2.0f);
        }
    }

    SelectObject(dcC, oC); SelectObject(dcM, oM);
    ICONINFO ii = {};
    ii.fIcon = TRUE; ii.hbmMask = bmM; ii.hbmColor = bmC;
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(bmC); DeleteObject(bmM);
    DeleteDC(dcC); DeleteDC(dcM);
    ReleaseDC(NULL, hs);
    return ic;
}

// =====================================================================
//  TEPSİ
// =====================================================================
static void UpdateTray() {
    nid.hIcon = isTimerActive ? hIconActive : hIconIdle;
    std::wstring tip = isTimerActive ? (Tr(S_TIPLEFT) + FormatShort(remainingSeconds)) : Tr(S_TIPREADY);
    wcsncpy(nid.szTip, tip.c_str(), 127);
    nid.szTip[127] = 0;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void Refresh() {
    if (hMain && flyoutVisible) RedrawWindow(hMain, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// =====================================================================
//  YERLEŞİM / ÖLÇEK / ŞEKİL
// =====================================================================
struct Placement { int x, y, offX, offY; };

static int ClampI(int v, int lo, int hi) { if (hi < lo) return lo; return v < lo ? lo : (v > hi ? hi : v); }

static void GetAnchorMonitor(RECT* mr, RECT* wa) {
    HMONITOR mon = MonitorFromPoint(anchorPt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    *mr = mi.rcMonitor; *wa = mi.rcWork;
}

static Placement ComputePlacement(int w, int h) {
    RECT mr, wa;
    GetAnchorMonitor(&mr, &wa);
    int edge = 0;                                  // 0 alt, 1 üst, 2 sol, 3 sağ
    if (wa.bottom < mr.bottom) edge = 0;
    else if (wa.top > mr.top) edge = 1;
    else if (wa.left > mr.left) edge = 2;
    else if (wa.right < mr.right) edge = 3;
    const int mg = 12;
    Placement p = {};
    switch (edge) {
        case 0: p.x = ClampI(anchorPt.x - w / 2, wa.left + mg, wa.right - w - mg); p.y = wa.bottom - h - mg; p.offY = 18; break;
        case 1: p.x = ClampI(anchorPt.x - w / 2, wa.left + mg, wa.right - w - mg); p.y = wa.top + mg; p.offY = -18; break;
        case 2: p.x = wa.left + mg; p.y = ClampI(anchorPt.y - h / 2, wa.top + mg, wa.bottom - h - mg); p.offX = -18; break;
        default: p.x = wa.right - w - mg; p.y = ClampI(anchorPt.y - h / 2, wa.top + mg, wa.bottom - h - mg); p.offX = 18; break;
    }
    return p;
}

// Küçük ekranlarda panel çalışma alanına sığacak şekilde otomatik küçülür
static float ComputeScale() {
    RECT mr, wa;
    GetAnchorMonitor(&mr, &wa);
    float baseH = (float)(showSettings ? H_EXP : H_COL);
    float maxS = ((wa.bottom - wa.top) - 36.0f) / baseH;
    float s = dpiScale;
    if (s > maxS) s = maxS;
    if (s < 0.6f) s = 0.6f;
    return s;
}

static void ApplyRegion(HWND hwnd, int w, int h) {
    HRGN a = CreateEllipticRgn(0, 0, w + 1, w + 1);
    HRGN b = CreateRoundRectRgn(0, w / 2 - 30, w + 1, h + 1, 24, 24);
    CombineRgn(a, a, b, RGN_OR);
    DeleteObject(b);
    SetWindowRgn(hwnd, a, TRUE);     // bölgenin sahipliği sisteme geçer
}

static void UpdateLayout(HWND hwnd, bool reposition) {
    g_s = ComputeScale();
    float s = g_s;

    auto mk = [&](int h, int weight, const wchar_t* face) {
        return CreateFontW(-(int)(h * s + 0.5f), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, face);
    };
    HFONT nTitle = haveRingbearer ? mk(40, FW_NORMAL, L"Ringbearer") : mk(34, FW_BOLD, L"Georgia");
    HFONT nTime = mk(44, FW_LIGHT, L"Segoe UI Light");
    HFONT nEdit = mk(20, FW_SEMIBOLD, L"Segoe UI");
    HFONT nMed = mk(15, FW_NORMAL, L"Georgia");
    HFONT nSmall = mk(12, FW_NORMAL, L"Georgia");
    HFONT nChip = mk(12, FW_SEMIBOLD, L"Segoe UI");

    SendMessageW(hInput, WM_SETFONT, (WPARAM)nEdit, TRUE);
    if (hFontTitle) DeleteObject(hFontTitle);
    if (hFontTime) DeleteObject(hFontTime);
    if (hFontEdit) DeleteObject(hFontEdit);
    if (hFontMed) DeleteObject(hFontMed);
    if (hFontSmall) DeleteObject(hFontSmall);
    if (hFontChip) DeleteObject(hFontChip);
    hFontTitle = nTitle; hFontTime = nTime; hFontEdit = nEdit; hFontMed = nMed; hFontSmall = nSmall; hFontChip = nChip;

    MoveWindow(hInput, PX(130), PX((float)Y_EDIT), PX(100), PX(28), TRUE);
    for (int i = 0; i < 5; i++)
        MoveWindow(hChip[i], PX((float)(71 + i * 45)), PX((float)Y_CHIPS), PX(38), PX(26), TRUE);
    MoveWindow(hBtnAction, PX(71), PX((float)Y_ACTION), PX(218), PX(48), TRUE);
    MoveWindow(hBtnSettings, PX(125), PX((float)Y_SETBTN), PX(110), PX(24), TRUE);
    MoveWindow(hBtnLang, PX(71), PX((float)Y_LANG), PX(218), PX(28), TRUE);
    MoveWindow(hBtnStartup, PX(71), PX((float)Y_START), PX(218), PX(28), TRUE);

    if (hwnd) {
        int wpx = PX((float)W_BASE), hpx = PX((float)(showSettings ? H_EXP : H_COL));
        if (reposition && flyoutVisible) {
            Placement pl = ComputePlacement(wpx, hpx);
            animTx = pl.x; animTy = pl.y;
            SetWindowPos(hwnd, HWND_TOPMOST, pl.x, pl.y, wpx, hpx, SWP_NOACTIVATE);
        } else {
            SetWindowPos(hwnd, NULL, 0, 0, wpx, hpx, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        ApplyRegion(hwnd, wpx, hpx);
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

// =====================================================================
//  PANELİ GÖSTER / GİZLE
// =====================================================================
static void ShowFlyout(bool atCursor) {
    if (atCursor) {
        GetCursorPos(&anchorPt);
    } else {
        POINT z = { 0, 0 };
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromPoint(z, MONITOR_DEFAULTTOPRIMARY), &mi);
        anchorPt.x = mi.rcWork.right - 1;
        anchorPt.y = mi.rcWork.bottom - 1;
    }
    UpdateLayout(hMain, false);

    RECT wr;
    GetWindowRect(hMain, &wr);
    Placement pl = ComputePlacement(wr.right - wr.left, wr.bottom - wr.top);
    animTx = pl.x; animTy = pl.y; animOffX = pl.offX; animOffY = pl.offY;

    flyoutVisible = true;
    SetLayeredWindowAttributes(hMain, 0, 0, LWA_ALPHA);
    SetWindowPos(hMain, HWND_TOPMOST, pl.x + pl.offX, pl.y + pl.offY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetForegroundWindow(hMain);
    if (!isTimerActive) {
        SetFocus(hInput);
        SendMessageW(hInput, EM_SETSEL, 0, -1);
    }
    animStart = GetTickCount64();
    SetTimer(hMain, ID_ANIM_TIMER, 15, NULL);
    SetTimer(hMain, ID_HOVER_TIMER, 33, NULL);
    Refresh();
}

static void HideFlyout() {
    if (!flyoutVisible) return;
    flyoutVisible = false;
    KillTimer(hMain, ID_ANIM_TIMER);
    KillTimer(hMain, ID_HOVER_TIMER);
    KillTimer(hMain, ID_AUTOHIDE);
    if (isDraggingOpacity) { isDraggingOpacity = false; ReleaseCapture(); }
    ShowWindow(hMain, SW_HIDE);
    currentHoverId = 0;
    lastHideTick = GetTickCount64();
}

// =====================================================================
//  PROTOKOL
// =====================================================================
static void StartProtocol(int minutes) {
    if (minutes <= 0) minutes = 60;
    if (minutes > MAX_MINUTES) minutes = MAX_MINUTES;

    RunShutdown(L"-a");
    remainingSeconds = minutes * 60;
    totalSeconds = remainingSeconds;
    RunShutdown(L"-s -t " + std::to_wstring(remainingSeconds));

    endTime = time(NULL) + remainingSeconds;
    isTimerActive = true;
    currentStatus = 1;
    lastMinutes = minutes;
    SetTimer(hMain, ID_TIMER, 500, NULL);
    SavePending(true);
    SaveSettings();
    SetWindowTextW(hInput, std::to_wstring(minutes).c_str());
    SendMessageW(hInput, EM_SETREADONLY, TRUE, 0);
    UpdateStatusText();
    UpdateTray();
    Refresh();
}

static void CancelProtocol() {
    RunShutdown(L"-a");
    KillTimer(hMain, ID_TIMER);
    KillTimer(hMain, ID_AUTOHIDE);
    isTimerActive = false;
    currentStatus = 3;
    SavePending(false);
    SendMessageW(hInput, EM_SETREADONLY, FALSE, 0);
    UpdateStatusText();
    UpdateTray();
    Refresh();
}

static void StopMellon() {
    RunShutdown(L"-a");
    KillTimer(hMain, ID_TIMER);
    KillTimer(hMain, ID_AUTOHIDE);
    isTimerActive = false;
    currentStatus = 2;
    SavePending(false);
    SendMessageW(hInput, EM_SETREADONLY, FALSE, 0);
    SetWindowTextW(hInput, std::to_wstring(lastMinutes).c_str());
    UpdateStatusText();
    UpdateTray();
    Refresh();
}

static void ExitApp() {
    if (isTimerActive) {
        HideFlyout();
        int r = MessageBoxW(NULL, Tr(S_EXIT_Q).c_str(), L"M.O.R.I.A.", MB_YESNOCANCEL | MB_ICONQUESTION | MB_TOPMOST);
        if (r == IDCANCEL) return;
        if (r == IDYES) { RunShutdown(L"-a"); SavePending(false); }
    }
    DestroyWindow(hMain);
}

static void ShowTrayMenu() {
    HideFlyout();
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_OPEN, Tr(S_M_OPEN).c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    if (isTimerActive) {
        AppendMenuW(m, MF_STRING, IDM_CANCEL, Tr(S_M_CANCEL).c_str());
    } else {
        const int q[3] = { 30, 60, 90 };
        const int ids[3] = { IDM_Q30, IDM_Q60, IDM_Q90 };
        for (int i = 0; i < 3; i++) {
            std::wstring t = isEnglish ? (L"Sleep in " + std::to_wstring(q[i]) + L" min")
                                       : (std::to_wstring(q[i]) + L" dk sonra uykuya geç");
            AppendMenuW(m, MF_STRING, ids[i], t.c_str());
        }
    }
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, Tr(S_M_EXIT).c_str());

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hMain);
    int cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hMain, NULL);
    PostMessageW(hMain, WM_NULL, 0, 0);
    DestroyMenu(m);

    switch (cmd) {
        case IDM_OPEN: ShowFlyout(true); break;
        case IDM_Q30: StartProtocol(30); break;
        case IDM_Q60: StartProtocol(60); break;
        case IDM_Q90: StartProtocol(90); break;
        case IDM_CANCEL: CancelProtocol(); break;
        case IDM_EXIT: ExitApp(); break;
    }
}

static void UpdateButtonTexts() {
    SetWindowTextW(hBtnAction, Tr(S_START).c_str());
    SetWindowTextW(hBtnSettings, Tr(S_SETTINGS).c_str());
    SetWindowTextW(hBtnLang, Tr(S_LANG).c_str());
    SetWindowTextW(hBtnStartup, (Tr(S_STARTUP) + OnOff(isStartupEnabled)).c_str());
}

static void SetOpacityLayered(HWND hwnd) {
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)((255 * opacityVal) / 100), LWA_ALPHA);
}

// =====================================================================
//  PENCERE PROSEDÜRÜ
// =====================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
    static HBRUSH hInputBrush = CreateSolidBrush(COLOR_BTN_BG);

    if (g_msgTaskbarCreated && msg == g_msgTaskbarCreated) {   // Explorer yeniden başladı
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        Shell_NotifyIconW(NIM_ADD, &nid);
        return 0;
    }

    switch (msg) {
        case WM_CREATE: {
            hMain = hwnd;
            AddFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
            haveRingbearer = FontAvailable(L"Ringbearer");
            LoadSettings();

            SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetOpacityLayered(hwnd);

            hInput = CreateWindowExW(0, L"EDIT", std::to_wstring(lastMinutes).c_str(), WS_VISIBLE | WS_CHILD | ES_CENTER | ES_AUTOHSCROLL,
                                     0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_INPUT, NULL, NULL);
            SendMessageW(hInput, EM_SETLIMITTEXT, 12, 0);
            hBtnAction = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_ACTION, NULL, NULL);
            hBtnSettings = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_SETTINGS, NULL, NULL);
            hBtnLang = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_LANG, NULL, NULL);
            hBtnStartup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_STARTUP, NULL, NULL);
            for (int i = 0; i < 5; i++)
                hChip[i] = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)(ID_CHIP0 + i), NULL, NULL);

            // Tepsi simgesi (her zaman orada)
            int sm = GetSystemMetrics(SM_CXSMICON);
            if (sm < 16) sm = 16;
            hIconIdle = MakeDoorIcon(sm, COLOR_ITHILDIN);
            hIconActive = MakeDoorIcon(sm, COLOR_AMBER);
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = hwnd; nid.uID = 2001;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAYICON;
            nid.hIcon = hIconIdle;

            UpdateLayout(hwnd, false);
            UpdateButtonTexts();

            // Önceki oturumdan kalan geri sayım varsa devam et
            if (LoadPending()) {
                isTimerActive = true;
                currentStatus = 1;
                RunShutdown(L"-a");
                RunShutdown(L"-s -t " + std::to_wstring(remainingSeconds));
                SendMessageW(hInput, EM_SETREADONLY, TRUE, 0);
                SetTimer(hwnd, ID_TIMER, 500, NULL);
            }
            UpdateStatusText();
            wcsncpy(nid.szTip, (isTimerActive ? (Tr(S_TIPLEFT) + FormatShort(remainingSeconds)) : Tr(S_TIPREADY)).c_str(), 127);
            nid.hIcon = isTimerActive ? hIconActive : hIconIdle;
            Shell_NotifyIconW(NIM_ADD, &nid);
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_ACTIVATE: {
            if (LOWORD(wp) == WA_INACTIVE && flyoutVisible && !isDraggingOpacity) HideFlyout();
            break;
        }

        case WM_CLOSE:
            HideFlyout();
            return 0;

        case WM_SHOWFLYOUT:
            ShowFlyout(false);
            return 0;

        case WM_MOUSEWHEEL: {
            if (flyoutVisible && !isTimerActive) {
                int steps = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
                bool mel = false;
                int m = ReadMinutesFromEdit(&mel) + steps * 5;
                if (m < 1) m = 1;
                if (m > MAX_MINUTES) m = MAX_MINUTES;
                SetWindowTextW(hInput, std::to_wstring(m).c_str());
            }
            return 0;
        }

        case WM_TIMER: {
            if (wp == ID_TIMER && isTimerActive) {
                int left = (int)difftime(endTime, time(NULL));
                if (left > 0) {
                    if (left != remainingSeconds) {
                        remainingSeconds = left;
                        UpdateStatusText();
                        UpdateTray();
                    }
                } else {
                    KillTimer(hwnd, ID_TIMER);
                    isTimerActive = false;
                    remainingSeconds = 0;
                    currentStatus = 4;
                    SavePending(false);
                    SendMessageW(hInput, EM_SETREADONLY, FALSE, 0);
                    UpdateStatusText();
                    UpdateTray();
                    Refresh();
                }
            }
            else if (wp == ID_HOVER_TIMER) {
                if (!flyoutVisible) break;
                POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
                HWND hHover = ChildWindowFromPointEx(hwnd, pt, CWP_SKIPINVISIBLE);
                int id = (hHover && hHover != hwnd) ? GetDlgCtrlID(hHover) : 0;
                if (id != currentHoverId) {
                    HWND hOld = currentHoverId ? GetDlgItem(hwnd, currentHoverId) : NULL;
                    if (hOld) InvalidateRect(hOld, NULL, FALSE);
                    currentHoverId = id;
                    if (hHover && hHover != hwnd) InvalidateRect(hHover, NULL, FALSE);
                }
                InvalidateRect(hwnd, NULL, FALSE);      // ithildin animasyonu
            }
            else if (wp == ID_ANIM_TIMER) {
                double t = (double)(GetTickCount64() - animStart) / 180.0;
                if (t >= 1.0) { t = 1.0; KillTimer(hwnd, ID_ANIM_TIMER); }
                float e = 1.0f - powf(1.0f - (float)t, 3.0f);
                SetLayeredWindowAttributes(hwnd, 0, (BYTE)(((255 * opacityVal) / 100) * e), LWA_ALPHA);
                SetWindowPos(hwnd, NULL, animTx + (int)((1.0f - e) * animOffX), animTy + (int)((1.0f - e) * animOffY),
                             0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            else if (wp == ID_AUTOHIDE) {
                KillTimer(hwnd, ID_AUTOHIDE);
                HideFlyout();
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            if (showSettings) {
                int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                RECT rOp = { PX(70), PX((float)Y_OPSLIDER - 14), PX(290), PX((float)Y_OPSLIDER + 14) };
                POINT pt = { x, y };
                if (PtInRect(&rOp, pt)) {
                    isDraggingOpacity = true;
                    SetCapture(hwnd);
                    SendMessageW(hwnd, WM_MOUSEMOVE, 0, lp);
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (isDraggingOpacity) {
                int x = GET_X_LPARAM(lp);
                float p = (x - PX(80)) / (float)PX(200);
                if (p < 0.0f) p = 0.0f;
                if (p > 1.0f) p = 1.0f;
                int v = 30 + (int)(p * 70.0f + 0.5f);
                if (v != opacityVal) {
                    opacityVal = v;
                    SetOpacityLayered(hwnd);
                }
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (isDraggingOpacity) {
                isDraggingOpacity = false;
                ReleaseCapture();
                SaveSettings();
            }
            break;
        }

        case WM_CAPTURECHANGED:
            isDraggingOpacity = false;
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdcWindow = BeginPaint(hwnd, &ps);
            float s = g_s;

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            HDC hdc = CreateCompatibleDC(hdcWindow);
            HBITMAP hBmp = CreateCompatibleBitmap(hdcWindow, rcClient.right, rcClient.bottom);
            HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
            HGDIOBJ origFont = GetCurrentObject(hdc, OBJ_FONT);

            FillRect(hdc, &rcClient, hBgBrush);
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));

            double timeMs = (double)GetTickCount64();
            double pulse = 0.65 + 0.35 * sin(timeMs * 0.0025);
            COLORREF auraGlow  = RGB((int)(15 * pulse), (int)(30 * pulse), (int)(45 * pulse));
            COLORREF outerGlow = RGB((int)(40 * pulse), (int)(80 * pulse), (int)(110 * pulse));
            COLORREF midGlow   = RGB((int)(80 * pulse), (int)(150 * pulse), (int)(190 * pulse));
            COLORREF coreGlow  = RGB((int)(135 * pulse), (int)(225 * pulse), (int)(255 * pulse));
            COLORREF crispGlow = RGB(200, 245, 255);

            float baseH = (float)(showSettings ? H_EXP : H_COL);
            float x1 = 6 * s, x2 = (W_BASE - 6) * s;
            float yArcTop = 6 * s;
            float arcW = x2 - x1;
            float yArcMid = yArcTop + arcW / 2;
            float yBottom = (baseH - 6) * s;
            float cxF = (x1 + x2) / 2.0f;

            // --- Arkada çok soluk büyük sekiz köşeli yıldız
            {
                COLORREF dim = RGB((int)(20 * pulse), (int)(40 * pulse), (int)(56 * pulse));
                PenScope p(hdc, 1, dim);
                DrawStarShape(hdc, cxF, 255 * s, 92 * s, 38 * s, 8, -1.5707963f);
                Ellipse(hdc, (int)(cxF - 62 * s), (int)(255 * s - 62 * s), (int)(cxF + 62 * s), (int)(255 * s + 62 * s));
            }

            // --- Kapı çerçevesi (çok katmanlı ithildin parıltısı)
            auto DrawDoor = [&](float inset, int pw, COLORREF col) {
                PenScope p(hdc, pw, col);
                float ax1 = x1 + inset, ax2 = x2 - inset, aTop = yArcTop + inset, aBot = yBottom - inset;
                float aw = ax2 - ax1, aMid = aTop + aw / 2;
                MoveToEx(hdc, (int)ax1, (int)aBot, NULL); LineTo(hdc, (int)ax1, (int)aMid);
                MoveToEx(hdc, (int)ax2, (int)aBot, NULL); LineTo(hdc, (int)ax2, (int)aMid);
                MoveToEx(hdc, (int)ax1, (int)aBot, NULL); LineTo(hdc, (int)ax2, (int)aBot);
                Arc(hdc, (int)ax1, (int)aTop, (int)ax2, (int)(aTop + aw), (int)ax2, (int)aMid, (int)ax1, (int)aMid);
            };
            DrawDoor(0, (int)(12 * s), auraGlow);
            DrawDoor(0, (int)(7 * s), outerGlow);
            DrawDoor(0, (int)(4 * s), midGlow);
            DrawDoor(0, (int)(2 * s), coreGlow);
            DrawDoor(0, 1, crispGlow);
            DrawDoor(16 * s, 1, COLOR_MUTED);

            // --- Elfçe yazıt, ağaçlar, amblem
            DrawInscription(hdc, s, x1, x2, yArcMid, yBottom, midGlow);
            float treeBaseY = yBottom - 6 * s;
            float trunk = treeBaseY - 300 * s;
            DrawTree(hdc, 46 * s, treeBaseY, 1.0f, trunk, s, midGlow, coreGlow);
            DrawTree(hdc, (W_BASE - 46) * s, treeBaseY, -1.0f, trunk, s, midGlow, coreGlow);
            DrawEmblem(hdc, s, cxF, yArcMid, timeMs, outerGlow, crispGlow, 134.0f, 56.0f, 100.0f);

            // --- Başlık (kayan parıltı ile)
            SelectObject(hdc, hFontTitle);
            RECT rTitle = { 0, PX((float)Y_TITLE), PX((float)W_BASE), PX((float)Y_TITLE + 44) };
            std::wstring titleStr = L"M.O.R.I.A.";
            SIZE sz;
            GetTextExtentPoint32W(hdc, titleStr.c_str(), (int)titleStr.length(), &sz);
            int textX = (int)((PX((float)W_BASE) - sz.cx) / 2);
            int textY = rTitle.top + ((rTitle.bottom - rTitle.top) - sz.cy) / 2;
            SetTextColor(hdc, COLOR_ITHILDIN);
            TextOutW(hdc, textX, textY, titleStr.c_str(), (int)titleStr.length());

            BeginPath(hdc);
            TextOutW(hdc, textX, textY, titleStr.c_str(), (int)titleStr.length());
            EndPath(hdc);
            HRGN hClip = PathToRegion(hdc);
            if (hClip) {
                SelectClipRgn(hdc, hClip);
                double phase = fmod(timeMs, 3000.0) / 3000.0;
                int sx = (int)(-60 * s + phase * 480 * s);
                POINT pts[4] = {
                    Pt((float)(sx + 25 * s), (float)rTitle.top), Pt((float)(sx + 45 * s), (float)rTitle.top),
                    Pt((float)(sx - 15 * s), (float)rTitle.bottom), Pt((float)(sx - 35 * s), (float)rTitle.bottom)
                };
                BrushScope br(hdc, RGB(220, 250, 255));
                HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
                Polygon(hdc, pts, 4);
                SelectObject(hdc, oldPen);
                SelectClipRgn(hdc, NULL);
                DeleteObject(hClip);
            }

            // --- Alt başlık
            SetTextColor(hdc, COLOR_MUTED);
            SelectObject(hdc, hFontSmall);
            {
                std::wstring sub = Tr(S_SUB);
                RECT r = { 0, PX((float)Y_SUB), PX((float)W_BASE), PX((float)Y_SUB + 18) };
                DrawTextW(hdc, sub.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE);
            }

            // --- Büyük saat: boştayken seçilen süre, çalışırken kalan süre
            {
                bool mel = false;
                int showSec = isTimerActive ? remainingSeconds : ReadMinutesFromEdit(&mel) * 60;
                std::wstring tmStr = FormatHMS(showSec);
                COLORREF tc = isTimerActive ? ((remainingSeconds <= 60) ? COLOR_RED : COLOR_ITHILDIN) : COLOR_TEXT;
                SelectObject(hdc, hFontTime);
                RECT r = { 0, PX((float)Y_TIME), PX((float)W_BASE), PX((float)Y_TIME + 52) };
                if (isTimerActive) {            // hafif parıltı
                    SetTextColor(hdc, outerGlow);
                    RECT r1 = r; OffsetRect(&r1, -1, 0); DrawTextW(hdc, tmStr.c_str(), -1, &r1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                    RECT r2 = r; OffsetRect(&r2, 1, 0);  DrawTextW(hdc, tmStr.c_str(), -1, &r2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
                SetTextColor(hdc, tc);
                DrawTextW(hdc, tmStr.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }

            // --- "Kapılar kapanıyor" ilerleme çubuğu
            {
                int bx1 = PX(71), bx2 = PX(289), by = PX((float)Y_BAR);
                {
                    PenScope p(hdc, (int)(3 * s), COLOR_BTN_BG);
                    MoveToEx(hdc, bx1, by, NULL); LineTo(hdc, bx2, by);
                }
                {
                    PenScope p(hdc, 1, COLOR_MUTED);
                    MoveToEx(hdc, bx1, by + PX(3), NULL); LineTo(hdc, bx2, by + PX(3));
                }
                if (isTimerActive && totalSeconds > 0) {
                    float prog = 1.0f - (float)remainingSeconds / (float)totalSeconds;
                    if (prog < 0) prog = 0;
                    if (prog > 1) prog = 1;
                    int fx = bx1 + (int)((bx2 - bx1) * prog);
                    {
                        PenScope p(hdc, (int)(5 * s), outerGlow);
                        MoveToEx(hdc, bx1, by, NULL); LineTo(hdc, fx, by);
                    }
                    {
                        PenScope p(hdc, (int)(2 * s), coreGlow);
                        MoveToEx(hdc, bx1, by, NULL); LineTo(hdc, fx, by);
                    }
                }
            }

            // --- Durum yazısı
            if (currentStatus == 1 || currentStatus == 2) SetTextColor(hdc, COLOR_ITHILDIN);
            else if (currentStatus == 3 || currentStatus == 4) SetTextColor(hdc, COLOR_RED);
            else SetTextColor(hdc, COLOR_TEXT);
            SelectObject(hdc, hFontMed);
            {
                RECT r = { PX(20), PX((float)Y_STATUS), PX((float)W_BASE - 20), PX((float)Y_STATUS + 24) };
                DrawTextW(hdc, statusText.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            // --- Dakika etiketi + giriş kutusunun altındaki çizgi
            SetTextColor(hdc, COLOR_MUTED);
            SelectObject(hdc, hFontSmall);
            {
                std::wstring lbl = Tr(S_MINLABEL);
                RECT r = { 0, PX((float)Y_LABEL), PX((float)W_BASE), PX((float)Y_LABEL + 18) };
                DrawTextW(hdc, lbl.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE);
                PenScope p(hdc, 1, isTimerActive ? COLOR_MUTED : COLOR_ITHILDIN);
                MoveToEx(hdc, PX(130), PX((float)Y_EDIT + 31), NULL);
                LineTo(hdc, PX(230), PX((float)Y_EDIT + 31));
            }

            // --- Ayarlar bölümü
            if (showSettings) {
                {
                    PenScope p(hdc, 1, COLOR_MUTED);
                    MoveToEx(hdc, PX(100), PX((float)Y_SEP), NULL);
                    LineTo(hdc, PX(260), PX((float)Y_SEP));
                }
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, COLOR_TEXT);
                std::wstring opTxt = Tr(S_OPACITY) + PercentText(opacityVal);
                RECT rOp = { 0, PX((float)Y_OPLBL), PX((float)W_BASE), PX((float)Y_OPLBL + 20) };
                DrawTextW(hdc, opTxt.c_str(), -1, &rOp, DT_CENTER | DT_SINGLELINE);

                int ty = PX((float)Y_OPSLIDER);
                {
                    PenScope p(hdc, 2, COLOR_MUTED);
                    MoveToEx(hdc, PX(80), ty, NULL); LineTo(hdc, PX(280), ty);
                }
                float opP = (opacityVal - 30) / 70.0f;
                int opCx = (int)(PX(80) + opP * PX(200));
                {
                    PenScope p(hdc, 1, crispGlow);
                    BrushScope b(hdc, COLOR_ITHILDIN);
                    Ellipse(hdc, opCx - PX(7), ty - PX(7), opCx + PX(7), ty + PX(7));
                }
            }

            SelectObject(hdc, origFont);
            BitBlt(hdcWindow, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);
            SelectObject(hdc, oldBmp);
            DeleteObject(hBmp);
            DeleteDC(hdc);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT d = (LPDRAWITEMSTRUCT)lp;
            int id = (int)d->CtlID;
            bool chip = (id >= ID_CHIP0 && id < ID_CHIP0 + 5);
            bool chipOff = chip && isTimerActive;
            bool hov = (id == currentHoverId) && !chipOff;
            bool chipSel = false;
            if (chip) {
                bool mel = false;
                int curMin = ReadMinutesFromEdit(&mel);
                chipSel = (!mel && curMin == PRESETS[id - ID_CHIP0]);
            }

            HBRUSH hb = CreateSolidBrush(hov ? COLOR_HOVER_BG : COLOR_BTN_BG);
            FillRect(d->hDC, &d->rcItem, hb);
            DeleteObject(hb);
            SetBkMode(d->hDC, TRANSPARENT);
            HGDIOBJ oldFont = SelectObject(d->hDC, chip ? hFontChip : hFontSmall);

            COLORREF txt = COLOR_TEXT;
            std::wstring text;
            if (id == ID_BTN_ACTION) { text = Tr(isTimerActive ? S_CANCEL : S_START); txt = isTimerActive ? COLOR_RED : COLOR_ITHILDIN; }
            else if (id == ID_BTN_SETTINGS) { text = Tr(S_SETTINGS); txt = COLOR_ITHILDIN; }
            else if (id == ID_BTN_LANG) { text = Tr(S_LANG); }
            else if (id == ID_BTN_STARTUP) { text = Tr(S_STARTUP) + OnOff(isStartupEnabled); }
            else if (chip) {
                text = std::to_wstring(PRESETS[id - ID_CHIP0]) + L" " + Tr(S_UNIT);
                txt = chipOff ? COLOR_MUTED : (chipSel ? COLOR_ITHILDIN : COLOR_TEXT);
            }
            SetTextColor(d->hDC, txt);

            RECT rc = d->rcItem;
            if (id == ID_BTN_SETTINGS) {
                RECT calc = rc;
                DrawTextW(d->hDC, text.c_str(), -1, &calc, DT_SINGLELINE | DT_CALCRECT);
                int tw = calc.right - calc.left;
                int total = tw + PX(14);
                int left = rc.left + ((rc.right - rc.left) - total) / 2;
                RECT rt = rc; rt.left = left; rt.right = left + tw;
                DrawTextW(d->hDC, text.c_str(), -1, &rt, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                int tx = left + tw + PX(9), ty = (rc.top + rc.bottom) / 2, a = PX(4);
                POINT tri[3];
                if (showSettings) { tri[0] = Pt((float)(tx - a), (float)(ty + a / 2)); tri[1] = Pt((float)(tx + a), (float)(ty + a / 2)); tri[2] = Pt((float)tx, (float)(ty - a / 2 - 1)); }
                else { tri[0] = Pt((float)(tx - a), (float)(ty - a / 2)); tri[1] = Pt((float)(tx + a), (float)(ty - a / 2)); tri[2] = Pt((float)tx, (float)(ty + a / 2 + 1)); }
                HBRUSH tb = CreateSolidBrush(txt);
                HGDIOBJ ob = SelectObject(d->hDC, tb);
                HGDIOBJ op = SelectObject(d->hDC, GetStockObject(NULL_PEN));
                Polygon(d->hDC, tri, 3);
                SelectObject(d->hDC, op); SelectObject(d->hDC, ob); DeleteObject(tb);
            } else if (text.find(L'\n') != std::wstring::npos) {
                RECT calc = rc;
                DrawTextW(d->hDC, text.c_str(), -1, &calc, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
                int th = calc.bottom - calc.top, bh = rc.bottom - rc.top;
                RECT rd = rc; rd.top += (bh - th) / 2;
                DrawTextW(d->hDC, text.c_str(), -1, &rd, DT_CENTER | DT_WORDBREAK);
            } else {
                DrawTextW(d->hDC, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            COLORREF border = (hov || chipSel) ? txt : COLOR_MUTED;
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ op = SelectObject(d->hDC, pen);
            HGDIOBJ ob = SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom);
            SelectObject(d->hDC, ob); SelectObject(d->hDC, op); DeleteObject(pen);
            SelectObject(d->hDC, oldFont);
            return TRUE;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, isTimerActive ? COLOR_MUTED : COLOR_ITHILDIN);
            SetBkColor(hdc, COLOR_BTN_BG);
            return (LRESULT)hInputBrush;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            if (id == ID_INPUT && code == EN_CHANGE) {
                InvalidateRect(hwnd, NULL, FALSE);
                for (int i = 0; i < 5; i++) if (hChip[i]) InvalidateRect(hChip[i], NULL, FALSE);
                break;
            }
            if (id == ID_BTN_ACTION) {
                if (!isTimerActive) {
                    bool mellon = false;
                    int m = ReadMinutesFromEdit(&mellon);
                    if (mellon) {
                        StopMellon();
                    } else {
                        StartProtocol(m);
                        SetTimer(hwnd, ID_AUTOHIDE, 1800, NULL);    // onayı göster, sonra paneli kapat
                    }
                } else {
                    CancelProtocol();
                }
            }
            else if (id >= ID_CHIP0 && id < ID_CHIP0 + 5) {
                if (!isTimerActive) {
                    SetWindowTextW(hInput, std::to_wstring(PRESETS[id - ID_CHIP0]).c_str());
                    SetFocus(hInput);
                    SendMessageW(hInput, EM_SETSEL, 0, -1);
                }
            }
            else if (id == ID_BTN_SETTINGS) {
                showSettings = !showSettings;
                ShowWindow(hBtnLang, showSettings ? SW_SHOW : SW_HIDE);
                ShowWindow(hBtnStartup, showSettings ? SW_SHOW : SW_HIDE);
                UpdateLayout(hwnd, true);
                Refresh();
            }
            else if (id == ID_BTN_LANG) {
                isEnglish = !isEnglish; SaveSettings();
                UpdateStatusText(); UpdateButtonTexts(); UpdateTray(); Refresh();
            }
            else if (id == ID_BTN_STARTUP) {
                ToggleStartupStatus(); UpdateButtonTexts(); Refresh();
            }
            break;
        }

        case WM_TRAYICON: {
            if (lp == WM_LBUTTONUP) {
                if (flyoutVisible) HideFlyout();
                else if (GetTickCount64() - lastHideTick > 400) ShowFlyout(true);   // tıklama paneli zaten kapattıysa tekrar açma
            } else if (lp == WM_RBUTTONUP) {
                ShowTrayMenu();
            }
            break;
        }

        case WM_DESTROY: {
            RemoveFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
            KillTimer(hwnd, ID_TIMER);
            KillTimer(hwnd, ID_HOVER_TIMER);
            KillTimer(hwnd, ID_ANIM_TIMER);
            Shell_NotifyIconW(NIM_DELETE, &nid);
            if (hIconIdle) DestroyIcon(hIconIdle);
            if (hIconActive) DestroyIcon(hIconActive);
            if (hIconBig) DestroyIcon(hIconBig);
            if (hFontTitle) DeleteObject(hFontTitle);
            if (hFontTime) DeleteObject(hFontTime);
            if (hFontEdit) DeleteObject(hFontEdit);
            if (hFontMed) DeleteObject(hFontMed);
            if (hFontSmall) DeleteObject(hFontSmall);
            if (hFontChip) DeleteObject(hFontChip);
            DeleteObject(hBgBrush); DeleteObject(hInputBrush);
            PostQuitMessage(0);
            break;
        }
        default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// =====================================================================
//  GİRİŞ NOKTASI
// =====================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Tek örnek: ikinci çalıştırma mevcut paneli açar
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\MORIA.SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hOther = FindWindowW(L"MoriaFlyoutClass", NULL);
        if (hOther) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(hOther, WM_SHOWFLYOUT, 0, 0);
        }
        CloseHandle(hMutex);
        return 0;
    }

    SetProcessDPIAware();
    HDC hScreen = GetDC(NULL);
    dpiScale = GetDeviceCaps(hScreen, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, hScreen);
    if (dpiScale < 1.0f) dpiScale = 1.0f;
    g_s = dpiScale;

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir = exePath;
    size_t slash = dir.find_last_of(L"\\/");
    dir = (slash == std::wstring::npos) ? L"" : dir.substr(0, slash + 1);
    fontPath = dir + L"ringbearer.ttf";

    bool trayOnly = (wcsstr(GetCommandLineW(), L"--tray") != NULL);
    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    hIconBig = MakeDoorIcon(GetSystemMetrics(SM_CXICON), COLOR_ITHILDIN);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"MoriaFlyoutClass";
    wc.hbrBackground = NULL;
    wc.hIcon = hIconBig;
    wc.hIconSm = hIconBig;
    if (!RegisterClassExW(&wc)) return 0;

    // Görev çubuğu/Alt+Tab'da görünmeyen, üstte duran, kenarlıksız panel
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
                                L"MoriaFlyoutClass", L"M.O.R.I.A. - Doors of Durin",
                                WS_POPUP | WS_CLIPCHILDREN,
                                0, 0, W_BASE, H_COL, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    if (!trayOnly) ShowFlyout(false);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN && msg.hwnd == hInput && !isTimerActive) {      // Enter: protokolü başlat
                SendMessageW(hMain, WM_COMMAND, MAKEWPARAM(ID_BTN_ACTION, BN_CLICKED), (LPARAM)hBtnAction);
                continue;
            }
            if (msg.wParam == VK_ESCAPE && flyoutVisible) {            // Esc: paneli kapat
                HideFlyout();
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}
