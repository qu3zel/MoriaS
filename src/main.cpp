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
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>
#include <time.h>
#include <math.h>
#include <cmath>

// ---------------------------------------------------------------- Yerleşim (taban birimler, DPI ile çarpılır)
static const int W_BASE = 360;
static const int H_COL = 480;        // ayarlar kapalıyken yükseklik
static const int H_EXP = 612;        // ayarlar açıkken yükseklik
static const int Y_TITLE = 112, Y_SUB = 152, Y_TIME = 174, Y_BAR = 228, Y_STATUS = 236;
static const int Y_CHIPS = 266, Y_LABEL = 296, Y_EDIT = 322, Y_ACTION = 366, Y_SETBTN = 418;
static const int Y_SEP = 450, Y_LANG = 458, Y_START = 492, Y_OPLBL = 526, Y_OPSLIDER = 556;
static const int PRESETS[5] = { 15, 30, 45, 60, 90 };

#include "door_art.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

// ---------------------------------------------------------------- Renkler
#define COLOR_CHROMA RGB(1, 2, 3)    // Donanımsal DWM şeffaf anahtar rengi (0ms gecikmeli maskeleme)
#define COLOR_BG RGB(10, 16, 22)
#define COLOR_HOVER_BG RGB(22, 38, 54)
#define COLOR_ITHILDIN RGB(135, 225, 255)
#define COLOR_RED RGB(255, 90, 80)
#define COLOR_AMBER RGB(255, 150, 70)
#define COLOR_TEXT RGB(215, 240, 255)
#define COLOR_MUTED RGB(68, 104, 136)
#define COLOR_BTN_BG RGB(14, 23, 32)

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

// ---------------------------------------------------------------- Genel durum
HWND hMain = NULL, hInput = NULL, hBtnAction = NULL, hBtnSettings = NULL, hBtnLang = NULL, hBtnStartup = NULL;
HWND hChip[5] = {};
HFONT hFontTitle = NULL, hFontTime = NULL, hFontEdit = NULL, hFontMed = NULL, hFontSmall = NULL, hFontChip = NULL;
HICON hIconIdle = NULL, hIconActive = NULL, hIconBig = NULL;
NOTIFYICONDATAW nid = {};

// ---------------------------------------------------------------- Önbellekli Statik Kapı Mimari Katmanı
static HDC g_artCacheDC = NULL;
static HBITMAP g_artCacheBmp = NULL;
static HGDIOBJ g_artCacheOld = NULL;
static int g_artCacheW = 0, g_artCacheH = 0;
static float g_artCacheS = 0.0f;

static void FreeArtCache() {
    if (g_artCacheDC) {
        if (g_artCacheOld) SelectObject(g_artCacheDC, g_artCacheOld);
        DeleteDC(g_artCacheDC);
        g_artCacheDC = NULL;
        g_artCacheOld = NULL;
    }
    if (g_artCacheBmp) {
        DeleteObject(g_artCacheBmp);
        g_artCacheBmp = NULL;
    }
    g_artCacheW = 0; g_artCacheH = 0; g_artCacheS = 0.0f;
}

int currentStatus = 0;               // 0 hazır, 1 sayıyor, 2 mellon, 3 iptal, 4 kapanıyor
std::wstring statusText;
bool isEnglish = false, isStartupEnabled = false, showSettings = false, isTimerActive = false;
bool warnedOneMinute = false;
int remainingSeconds = 0, totalSeconds = 0, lastMinutes = 60;
time_t endTime = 0;
int currentHoverId = 0;
int opacityVal = 92;
bool isDraggingOpacity = false;

float dpiScale = 1.0f, g_s = 1.0f;
float currentBaseH = (float)H_COL;   // yumuşak ayarlar açılış/kapanış yüksekliği
double expandStartMs = 0.0;
float expandFromH = (float)H_COL, expandToH = (float)H_COL;
bool isExpandingAnim = false;
double rippleStartMs = 0.0;          // "mellon" veya başlatma ışık dalgası zamanı
float btnHoverBlend[20] = {};        // buton yumuşak hover geçişi (0..1)

POINT anchorPt = { 0, 0 };
bool flyoutVisible = false;
ULONGLONG lastHideTick = 0;
double animStartMs = 0.0;
int animTx = 0, animTy = 0, animOffX = 0, animOffY = 0;
UINT g_msgTaskbarCreated = 0;
std::wstring fontPath;
bool haveRingbearer = false;
bool g_highResTimerActive = false;

// Yüksek çözünürlüklü (mikrosaniye hassasiyetli) zamanlayıcı
static double NowMs() {
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart * 1000.0 / (double)freq.QuadPart;
}

// Aktif monitörün yenileme hızına (Hz) uygun milisaniye adımı (60Hz -> 15ms, 144Hz -> 6ms, 240Hz -> 4ms)
static UINT GetMonitorFrameIntervalMs() {
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    int hz = 60;
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 30)
        hz = (int)dm.dmDisplayFrequency;
    if (hz > 240) hz = 240;
    UINT ms = (UINT)(1000 / hz);
    if (ms < 4) ms = 4;
    if (ms > 15) ms = 15;
    return ms;
}

static void SetHighResTimer(bool enable) {
    if (enable && !g_highResTimerActive) {
        timeBeginPeriod(1);
        g_highResTimerActive = true;
    } else if (!enable && g_highResTimerActive) {
        timeEndPeriod(1);
        g_highResTimerActive = false;
    }
}

static int HoverSlot(int id) {
    if (id == ID_BTN_ACTION) return 0;
    if (id == ID_BTN_SETTINGS) return 1;
    if (id == ID_BTN_LANG) return 2;
    if (id == ID_BTN_STARTUP) return 3;
    if (id >= ID_CHIP0 && id < ID_CHIP0 + 5) return 4 + (id - ID_CHIP0);
    return -1;
}

static COLORREF LerpColor(COLORREF a, COLORREF b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t + 0.5f);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t + 0.5f);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t + 0.5f);
    return RGB(r, g, bl);
}

// Buton alt-sınıfı: WM_ERASEBKGND mesajını bastırarak beyaz flicker'ı tamamen engeller
static LRESULT CALLBACK BtnNoFlickerProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_ERASEBKGND) return 1;
    return DefSubclassProc(h, m, w, l);
}

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

// Statik kapı mimarisi katmanını önbelleğe çiz (sadece ilk açılışta 1 kez çalışır)
static void EnsureArtCache(HDC refDC, int w, int h, float s) {
    if (g_artCacheDC && g_artCacheW == w && g_artCacheH == h && fabsf(g_artCacheS - s) < 0.001f)
        return;

    FreeArtCache();
    g_artCacheDC = CreateCompatibleDC(refDC);
    g_artCacheBmp = CreateCompatibleBitmap(refDC, w, h);
    g_artCacheOld = SelectObject(g_artCacheDC, g_artCacheBmp);
    g_artCacheW = w; g_artCacheH = h; g_artCacheS = s;

    // 1. Prosedürel Moria Kaya Dokusu (32-bit DIB — taş damarları ve ay ışığı vinyeti)
    RenderRockTextureDIB(g_artCacheDC, w, h, s);

    // 2. Filmdeki Durin'in Kapıları Mimari Çizimi (Çok katmanlı GDI+ Ithildin ışıması)
    Gdiplus::Graphics g(g_artCacheDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    DrawMovieAccurateMoriaDoor(g, s, w, h);
}

// =====================================================================
//  DİL / METİN
// =====================================================================
enum {
    S_SUB, S_START, S_CANCEL, S_SETTINGS, S_LANG, S_STARTUP, S_MINLABEL, S_CLOSEAT, S_OPACITY, S_UNIT,
    S_M_OPEN, S_M_CANCEL, S_M_EXIT, S_EXIT_Q, S_READY, S_MELLON, S_CANCELLED, S_CLOSING, S_TIPREADY, S_TIPLEFT,
    S_WARN_TITLE, S_WARN_MSG,
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
    { L"M.O.R.I.A. - Kalan: ", L"M.O.R.I.A. - Left: " },
    { L"M.O.R.I.A. - Durin'in Kapıları", L"M.O.R.I.A. - Doors of Durin" },
    { L"Kapıların kapanmasına 1 dakika kaldı.\nİptal etmek için simgeye tıklayın.",
      L"1 minute left until the doors close.\nClick the icon to cancel." }
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

static void RunShutdown(const std::wstring& args) {
    wchar_t sysDir[MAX_PATH] = {};
    UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring exe = (n > 0 && n < MAX_PATH) ? (std::wstring(sysDir) + L"\\shutdown.exe") : std::wstring(L"shutdown.exe");
    RunCommandSilent(L"\"" + exe + L"\" " + args);
}

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

static time_t BootStamp() { return time(NULL) - (time_t)(GetTickCount64() / 1000); }

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
    if (!ok) SavePending(false);
    return ok;
}

static void ToggleStartupStatus() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (!isStartupEnabled) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
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
    FillRect(dcM, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
    FillRect(dcC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

    float u = n / 32.0f;
    int x1 = (int)(5 * u), x2 = (int)(27 * u), yTop = (int)(3 * u), yBot = (int)(29 * u);
    int w = x2 - x1, yMid = yTop + w / 2;

    {
        HGDIOBJ ob = SelectObject(dcM, GetStockObject(BLACK_BRUSH));
        HGDIOBJ op = SelectObject(dcM, GetStockObject(BLACK_PEN));
        Ellipse(dcM, x1 - 1, yTop - 1, x2 + 1, yTop + w + 1);
        Rectangle(dcM, x1 - 1, yMid, x2 + 1, yBot + 1);
        SelectObject(dcM, op); SelectObject(dcM, ob);
    }
    {
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

static void ShowTrayBalloon(const std::wstring& title, const std::wstring& msg) {
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    wcsncpy(nid.szInfoTitle, title.c_str(), 63);
    nid.szInfoTitle[63] = 0;
    wcsncpy(nid.szInfo, msg.c_str(), 255);
    nid.szInfo[255] = 0;
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
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

static float ComputeScale() {
    RECT mr, wa;
    GetAnchorMonitor(&mr, &wa);
    float maxS = ((wa.bottom - wa.top) - 36.0f) / (float)H_EXP;
    float s = dpiScale;
    if (s > maxS) s = maxS;
    if (s < 0.6f) s = 0.6f;
    return s;
}

// Bellek DC üzerinde kapı kemeri kırpma bölgesi oluşturur (DWM SetWindowRgn çağrısı yapmadan 0.01ms'de kırpar)
static HRGN CreateDoorClipRgn(int w, int h) {
    HRGN a = CreateEllipticRgn(0, 0, w + 1, w + 1);
    HRGN b = CreateRectRgn(0, w / 2, w + 1, h + 1);
    CombineRgn(a, a, b, RGN_OR);
    DeleteObject(b);
    return a;
}

static void SetOpacityLayered(HWND hwnd) {
    SetLayeredWindowAttributes(hwnd, COLOR_CHROMA, (BYTE)((255 * opacityVal) / 100), LWA_ALPHA | LWA_COLORKEY);
}

static void UpdateWindowGeometry(HWND hwnd, bool reposition) {
    if (!hwnd) return;
    // Pencere yüzeyini her zaman tam (W_BASE x H_EXP) boyutta tutuyoruz;
    // konumlandırmayı ise aktif kapı yüksekliğine (PX(currentBaseH)) göre yapıyoruz ki
    // kapının alt eşiği her zaman görev çubuğunun (taskbar) hemen 12px üstüne otursun!
    int wpx = PX((float)W_BASE), maxH = PX((float)H_EXP), curDoorH = PX(currentBaseH);
    if (reposition && flyoutVisible) {
        Placement pl = ComputePlacement(wpx, curDoorH);
        animTx = pl.x; animTy = pl.y;
        SetWindowPos(hwnd, HWND_TOPMOST, pl.x, pl.y, wpx, maxH, SWP_NOACTIVATE | SWP_NOCOPYBITS);
    } else {
        SetWindowPos(hwnd, NULL, 0, 0, wpx, maxH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void UpdateLayout(HWND hwnd, bool reposition) {
    g_s = ComputeScale();
    float s = g_s;

    auto mk = [&](int h, int weight, const wchar_t* face) {
        return CreateFontW(-(int)(h * s + 0.5f), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, face);
    };
    HFONT nTitle = haveRingbearer ? mk(38, FW_NORMAL, L"Ringbearer") : mk(32, FW_BOLD, L"Georgia");
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

    MoveWindow(hInput, PX(144), PX((float)Y_EDIT), PX(72), PX(26), TRUE);
    for (int i = 0; i < 5; i++)
        MoveWindow(hChip[i], PX((float)(71 + i * 45)), PX((float)Y_CHIPS), PX(38), PX(26), TRUE);
    MoveWindow(hBtnAction, PX(71), PX((float)Y_ACTION), PX(218), PX(46), TRUE);
    MoveWindow(hBtnSettings, PX(125), PX((float)Y_SETBTN), PX(110), PX(24), TRUE);
    MoveWindow(hBtnLang, PX(71), PX((float)Y_LANG), PX(218), PX(28), TRUE);
    MoveWindow(hBtnStartup, PX(71), PX((float)Y_START), PX(218), PX(28), TRUE);

    UpdateWindowGeometry(hwnd, reposition);
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
    isExpandingAnim = false;
    currentBaseH = (float)(showSettings ? H_EXP : H_COL);
    UpdateLayout(hMain, false);

    int wpx = PX((float)W_BASE), maxH = PX((float)H_EXP), curDoorH = PX(currentBaseH);
    Placement pl = ComputePlacement(wpx, curDoorH);
    animTx = pl.x; animTy = pl.y; animOffX = pl.offX; animOffY = pl.offY;

    ShowWindow(hBtnLang, showSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnStartup, showSettings ? SW_SHOW : SW_HIDE);

    flyoutVisible = true;
    SetHighResTimer(true);                         // Monitör Hz hızında (144Hz/240Hz) pürüzsüz zamanlayıcı
    UINT frameMs = GetMonitorFrameIntervalMs();

    SetLayeredWindowAttributes(hMain, COLOR_CHROMA, 0, LWA_ALPHA | LWA_COLORKEY);
    SetWindowPos(hMain, HWND_TOPMOST, pl.x + pl.offX, pl.y + pl.offY, wpx, maxH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetForegroundWindow(hMain);
    if (!isTimerActive) {
        SetFocus(hInput);
        SendMessageW(hInput, EM_SETSEL, 0, -1);
    }
    animStartMs = NowMs();
    SetTimer(hMain, ID_ANIM_TIMER, frameMs, NULL);
    SetTimer(hMain, ID_HOVER_TIMER, frameMs, NULL);
    Refresh();
}

static void HideFlyout() {
    if (!flyoutVisible) return;
    flyoutVisible = false;
    isExpandingAnim = false;
    KillTimer(hMain, ID_ANIM_TIMER);
    KillTimer(hMain, ID_HOVER_TIMER);
    KillTimer(hMain, ID_AUTOHIDE);
    SetHighResTimer(false);
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
    warnedOneMinute = (remainingSeconds <= 60);
    RunShutdown(L"-s -t " + std::to_wstring(remainingSeconds));

    endTime = time(NULL) + remainingSeconds;
    isTimerActive = true;
    currentStatus = 1;
    lastMinutes = minutes;
    rippleStartMs = NowMs();
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
    rippleStartMs = NowMs();
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

// Taş oyma yarı-saydam Ithildin buton çizimi (hem WM_DRAWITEM hem de animasyon sırasındaki önizleme için)
static void DrawMoriaButtonSurface(HDC dstDC, int xInMain, int yInMain, int bw, int bh,
                                   const std::wstring& text, HFONT font, COLORREF txtCol,
                                   float hoverBlend, bool isSelected, bool isSettingsBtn) {
    RECT rc = { 0, 0, bw, bh };

    // 1. Arka plandaki Moria kaya dokusunu ve Fëanor yıldızı / ağaç ışımasını kopyala ve hafif koyulaştır
    if (g_artCacheDC && xInMain >= 0 && yInMain >= 0 && xInMain + bw <= g_artCacheW && yInMain + bh <= g_artCacheH) {
        BitBlt(dstDC, 0, 0, bw, bh, g_artCacheDC, xInMain, yInMain, SRCCOPY);
        HDC tintDC = CreateCompatibleDC(dstDC);
        HBITMAP tintBmp = CreateCompatibleBitmap(dstDC, 1, 1);
        HGDIOBJ oldTint = SelectObject(tintDC, tintBmp);
        COLORREF tintCol = LerpColor(RGB(8, 14, 20), RGB(20, 42, 62), hoverBlend);
        SetPixel(tintDC, 0, 0, tintCol);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(170 - (int)(hoverBlend * 35.0f)), 0 };
        GdiAlphaBlend(dstDC, 0, 0, bw, bh, tintDC, 0, 0, 1, 1, bf);
        SelectObject(tintDC, oldTint);
        DeleteObject(tintBmp);
        DeleteDC(tintDC);
    } else {
        COLORREF bgCol = LerpColor(COLOR_BTN_BG, COLOR_HOVER_BG, hoverBlend);
        BrushScope hb(dstDC, bgCol);
        FillRect(dstDC, &rc, hb.br);
    }

    SetBkMode(dstDC, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dstDC, font);
    SetTextColor(dstDC, txtCol);

    if (isSettingsBtn) {
        RECT calc = rc;
        DrawTextW(dstDC, text.c_str(), -1, &calc, DT_SINGLELINE | DT_CALCRECT);
        int tw = calc.right - calc.left;
        int total = tw + PX(14);
        int left = rc.left + ((rc.right - rc.left) - total) / 2;
        RECT rt = rc; rt.left = left; rt.right = left + tw;
        DrawTextW(dstDC, text.c_str(), -1, &rt, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        int tx = left + tw + PX(9), ty = (rc.top + rc.bottom) / 2, a = PX(4);
        POINT tri[3];
        if (showSettings) {
            tri[0] = Pt((float)(tx - a), (float)(ty + a / 2));
            tri[1] = Pt((float)(tx + a), (float)(ty + a / 2));
            tri[2] = Pt((float)tx, (float)(ty - a / 2 - 1));
        } else {
            tri[0] = Pt((float)(tx - a), (float)(ty - a / 2));
            tri[1] = Pt((float)(tx + a), (float)(ty - a / 2));
            tri[2] = Pt((float)tx, (float)(ty + a / 2 + 1));
        }
        BrushScope tb(dstDC, txtCol);
        HGDIOBJ op = SelectObject(dstDC, GetStockObject(NULL_PEN));
        Polygon(dstDC, tri, 3);
        SelectObject(dstDC, op);
    } else if (text.find(L'\n') != std::wstring::npos) {
        RECT calc = rc;
        DrawTextW(dstDC, text.c_str(), -1, &calc, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
        int th = calc.bottom - calc.top;
        RECT rd = rc; rd.top += (bh - th) / 2;
        DrawTextW(dstDC, text.c_str(), -1, &rd, DT_CENTER | DT_WORDBREAK);
    } else {
        DrawTextW(dstDC, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Cüce/Elf pahlı Ithildin çerçevesi ve köşe mücevher vurguları
    COLORREF border = isSelected ? txtCol : LerpColor(COLOR_MUTED, txtCol, hoverBlend);
    {
        PenScope pen(dstDC, 1, border);
        SelectObject(dstDC, GetStockObject(NULL_BRUSH));
        Rectangle(dstDC, 0, 0, bw, bh);
    }
    if (hoverBlend > 0.05f || isSelected) {
        COLORREF innerGlow = LerpColor(RGB(25, 45, 65), RGB(80, 165, 210), isSelected ? 0.8f : hoverBlend);
        PenScope penIn(dstDC, 1, innerGlow);
        SelectObject(dstDC, GetStockObject(NULL_BRUSH));
        Rectangle(dstDC, 1, 1, bw - 1, bh - 1);
    }

    SelectObject(dstDC, oldFont);
}

// =====================================================================
//  PENCERE PROSEDÜRÜ
// =====================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH hChromaBrush = CreateSolidBrush(COLOR_CHROMA);
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
            hBtnAction = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_ACTION, NULL, NULL);
            hBtnSettings = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_SETTINGS, NULL, NULL);
            hBtnLang = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_LANG, NULL, NULL);
            hBtnStartup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_BTN_STARTUP, NULL, NULL);
            for (int i = 0; i < 5; i++) {
                hChip[i] = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)(ID_CHIP0 + i), NULL, NULL);
                SetWindowSubclass(hChip[i], BtnNoFlickerProc, 0, 0);
            }
            SetWindowSubclass(hBtnAction, BtnNoFlickerProc, 0, 0);
            SetWindowSubclass(hBtnSettings, BtnNoFlickerProc, 0, 0);
            SetWindowSubclass(hBtnLang, BtnNoFlickerProc, 0, 0);
            SetWindowSubclass(hBtnStartup, BtnNoFlickerProc, 0, 0);

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

            // Statik kapı mimarisi önbelleğini başlangıçta ısıt (ilk açılışta 0 gecikme)
            {
                HDC hs = GetDC(NULL);
                EnsureArtCache(hs, PX((float)W_BASE), PX((float)H_EXP), g_s);
                ReleaseDC(NULL, hs);
            }

            if (LoadPending()) {
                isTimerActive = true;
                currentStatus = 1;
                warnedOneMinute = (remainingSeconds <= 60);
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
                        if (remainingSeconds <= 60 && !warnedOneMinute) {
                            warnedOneMinute = true;
                            ShowTrayBalloon(Tr(S_WARN_TITLE), Tr(S_WARN_MSG));
                        }
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
                double now = NowMs();

                // 1. Ayarlar açılış/kapanış animasyonu (DWM SetWindowRgn çağrısı olmadan, tam monitör Hz hızında!)
                if (isExpandingAnim) {
                    double t = (now - expandStartMs) / 190.0;
                    if (t >= 1.0) {
                        t = 1.0;
                        isExpandingAnim = false;
                        currentBaseH = expandToH;
                        int wpx = PX((float)W_BASE), curDoorH = PX(currentBaseH);
                        Placement pl = ComputePlacement(wpx, curDoorH);
                        animTx = pl.x; animTy = pl.y;
                        SetWindowPos(hwnd, NULL, pl.x, pl.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                        if (showSettings) {
                            ShowWindow(hBtnLang, SW_SHOW);
                            ShowWindow(hBtnStartup, SW_SHOW);
                            InvalidateRect(hBtnLang, NULL, FALSE);
                            InvalidateRect(hBtnStartup, NULL, FALSE);
                        }
                    } else {
                        // Smootherstep / cubic ease-out
                        float u = 1.0f - (float)t;
                        float e = 1.0f - u * u * u;
                        currentBaseH = expandFromH + (expandToH - expandFromH) * e;
                        int wpx = PX((float)W_BASE), curDoorH = PX(currentBaseH);
                        Placement pl = ComputePlacement(wpx, curDoorH);
                        animTx = pl.x; animTy = pl.y;
                        SetWindowPos(hwnd, NULL, pl.x, pl.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }

                // 2. Buton yumuşak hover geçişi
                POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
                HWND hHover = ChildWindowFromPointEx(hwnd, pt, CWP_SKIPINVISIBLE);
                int id = (hHover && hHover != hwnd) ? GetDlgCtrlID(hHover) : 0;
                currentHoverId = id;

                const int allIds[9] = {
                    ID_BTN_ACTION, ID_BTN_SETTINGS, ID_BTN_LANG, ID_BTN_STARTUP,
                    ID_CHIP0, ID_CHIP0 + 1, ID_CHIP0 + 2, ID_CHIP0 + 3, ID_CHIP0 + 4
                };
                for (int i = 0; i < 9; i++) {
                    int cid = allIds[i];
                    int slot = HoverSlot(cid);
                    if (slot < 0) continue;
                    float target = (cid == currentHoverId) ? 1.0f : 0.0f;
                    float diff = target - btnHoverBlend[slot];
                    if (fabsf(diff) > 0.005f) {
                        btnHoverBlend[slot] += diff * 0.25f;
                        if (fabsf(target - btnHoverBlend[slot]) < 0.01f) btnHoverBlend[slot] = target;
                        HWND hc = GetDlgItem(hwnd, cid);
                        if (hc && IsWindowVisible(hc)) InvalidateRect(hc, NULL, FALSE);
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                UpdateWindow(hwnd);                     // Monitör Hz hızında anında çizim
            }
            else if (wp == ID_ANIM_TIMER) {
                double t = (NowMs() - animStartMs) / 180.0;
                if (t >= 1.0) { t = 1.0; KillTimer(hwnd, ID_ANIM_TIMER); }
                float e = 1.0f - powf(1.0f - (float)t, 3.0f);
                SetLayeredWindowAttributes(hwnd, COLOR_CHROMA, (BYTE)(((255 * opacityVal) / 100) * e), LWA_ALPHA | LWA_COLORKEY);
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
            int clientW = rcClient.right, clientH = rcClient.bottom;
            HDC hdc = CreateCompatibleDC(hdcWindow);
            HBITMAP hBmp = CreateCompatibleBitmap(hdcWindow, clientW, clientH);
            HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
            HGDIOBJ origFont = GetCurrentObject(hdc, OBJ_FONT);

            // 0. Önce tüm arka tamponu DWM şeffaf anahtar rengiyle (COLOR_CHROMA) doldur
            FillRect(hdc, &rcClient, hChromaBrush);

            // Bellek DC üzerinde aktif kapı yüksekliğine (currentBaseH) göre kemer kırpması uygula
            int curDoorH = PX(currentBaseH);
            HRGN hDoorClip = CreateDoorClipRgn(clientW, curDoorH);
            SelectClipRgn(hdc, hDoorClip);

            // 1. Önbellekli statik film kapısı mimarisini 0.03ms'de kopyala
            int maxCacheH = PX((float)H_EXP);
            EnsureArtCache(hdcWindow, clientW, maxCacheH, s);
            BitBlt(hdc, 0, 0, clientW, curDoorH, g_artCacheDC, 0, 0, SRCCOPY);

            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));

            double timeMs = NowMs();
            double pulse = 0.72 + 0.28 * sin(timeMs * 0.0025);

            double rippleBoost = 0.0;
            if (rippleStartMs > 0.0) {
                double elapsed = (timeMs - rippleStartMs) / 900.0;
                if (elapsed < 1.0) rippleBoost = sin(elapsed * PI_F) * 0.35;
                else rippleStartMs = 0.0;
            }
            double pGlow = pulse + rippleBoost;
            if (pGlow > 1.15) pGlow = 1.15;

            auto cClamp = [](double v) -> BYTE { return (BYTE)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
            COLORREF auraGlow  = RGB(cClamp(15 * pGlow), cClamp(34 * pGlow), cClamp(52 * pGlow));
            COLORREF outerGlow = RGB(cClamp(42 * pGlow), cClamp(88 * pGlow), cClamp(122 * pGlow));
            COLORREF midGlow   = RGB(cClamp(84 * pGlow), cClamp(165 * pGlow), cClamp(212 * pGlow));
            COLORREF coreGlow  = RGB(cClamp(145 * pGlow), cClamp(230 * pGlow), cClamp(255 * pGlow));
            COLORREF crispGlow = RGB(220, 248, 255);

            float baseH = currentBaseH;
            float x1 = 4.0f * s, x2 = (W_BASE - 4.0f) * s;
            float yArcTop = 4.0f * s;
            float yBottom = (baseH - 4.0f) * s;
            float cxF = (W_BASE * 0.5f) * s;

            // 2. Dış taş kemer silüeti ve alt eşik Ithildin çizgisi
            auto DrawOuterRim = [&](float inset, int pw, COLORREF col) {
                PenScope p(hdc, pw, col);
                float ax1 = x1 + inset, ax2 = x2 - inset, aTop = yArcTop + inset, aBot = yBottom - inset;
                float aw = ax2 - ax1, aMid = aTop + aw / 2;
                MoveToEx(hdc, (int)ax1, (int)aBot, NULL); LineTo(hdc, (int)ax1, (int)aMid);
                MoveToEx(hdc, (int)ax2, (int)aBot, NULL); LineTo(hdc, (int)ax2, (int)aMid);
                MoveToEx(hdc, (int)ax1, (int)aBot, NULL); LineTo(hdc, (int)ax2, (int)aBot);
                Arc(hdc, (int)ax1, (int)aTop, (int)ax2, (int)(aTop + aw), (int)ax2, (int)aMid, (int)ax1, (int)aMid);
            };
            DrawOuterRim(0, (int)(8 * s), auraGlow);
            DrawOuterRim(0, (int)(4 * s), outerGlow);
            DrawOuterRim(0, (int)(2 * s), midGlow);
            DrawOuterRim(0, 1, coreGlow);

            // Alt sütun kaideleri ve eşik çizgisi (currentBaseH ile birlikte pürüzsüzce kayar)
            {
                PenScope pMid(hdc, (int)(3 * s), midGlow);
                MoveToEx(hdc, PX(30), (int)(yBottom - 3.0f * s), NULL);
                LineTo(hdc, PX(W_BASE - 30), (int)(yBottom - 3.0f * s));
                PenScope pCore(hdc, 1, crispGlow);
                MoveToEx(hdc, PX(30), (int)(yBottom - 3.0f * s), NULL);
                LineTo(hdc, PX(W_BASE - 30), (int)(yBottom - 3.0f * s));
            }

            // 3. Saat yönünde kesintisiz dönen dış Tengwar rünleri (sol koridordan yukarı -> kemerden sağa -> sağ koridordan aşağı)
            DrawFlowingInscriptionFastGDI(hdc, s, x1, x2, 176.0f * s, yBottom, timeMs, midGlow, coreGlow);

            // 4. Durin'in Tacı etrafındaki 7 Yıldızın canlı parıltısı (filmdeki yay konumunda: starR = 108.0f)
            DrawSevenStarsFastGDI(hdc, s, cxF, 162.0f * s, timeMs, midGlow, crispGlow, 108.0f);

            // 5. Alt eşik üzerindeki "C / J" usta mührü (kapı boyuyla birlikte hareket eder, butonlarla çakışmaz)
            DrawMakerMarkFastGDI(hdc, s, cxF, yBottom - 20.0f * s, midGlow, coreGlow);

            // --- Başlık (kayan parıltı ile)
            SelectObject(hdc, hFontTitle);
            RECT rTitle = { 0, PX((float)Y_TITLE), PX((float)W_BASE), PX((float)Y_TITLE + 40) };
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
            HRGN hClipTitle = PathToRegion(hdc);
            if (hClipTitle) {
                CombineRgn(hClipTitle, hClipTitle, hDoorClip, RGN_AND);
                SelectClipRgn(hdc, hClipTitle);
                double phase = fmod(timeMs, 3000.0) / 3000.0;
                int sx = (int)(-60 * s + phase * 480 * s);
                POINT pts[4] = {
                    Pt((float)(sx + 25 * s), (float)rTitle.top), Pt((float)(sx + 45 * s), (float)rTitle.top),
                    Pt((float)(sx - 15 * s), (float)rTitle.bottom), Pt((float)(sx - 35 * s), (float)rTitle.bottom)
                };
                BrushScope br(hdc, RGB(230, 252, 255));
                HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
                Polygon(hdc, pts, 4);
                SelectObject(hdc, oldPen);
                SelectClipRgn(hdc, hDoorClip);
                DeleteObject(hClipTitle);
            }

            // --- Alt başlık
            SetTextColor(hdc, RGB(135, 195, 228));
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
                if (isTimerActive) {
                    SetTextColor(hdc, outerGlow);
                    RECT r1 = r; OffsetRect(&r1, -1, 0); DrawTextW(hdc, tmStr.c_str(), -1, &r1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                    RECT r2 = r; OffsetRect(&r2, 1, 0);  DrawTextW(hdc, tmStr.c_str(), -1, &r2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
                SetTextColor(hdc, tc);
                DrawTextW(hdc, tmStr.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }

            // --- "Kapılar kapanıyor" ilerleme çubuğu
            {
                int bx1 = PX(84), bx2 = PX(276), by = PX((float)Y_BAR);
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
                RECT r = { PX(58), PX((float)Y_STATUS), PX((float)W_BASE - 58), PX((float)Y_STATUS + 24) };
                DrawTextW(hdc, statusText.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            // --- Dakika etiketi + Fëanor yıldızı üzerindeki giriş kutusunun mücevher çerçevesi
            SetTextColor(hdc, RGB(145, 205, 235));
            SelectObject(hdc, hFontSmall);
            {
                std::wstring lbl = Tr(S_MINLABEL);
                RECT r = { PX(58), PX((float)Y_LABEL), PX((float)W_BASE - 58), PX((float)Y_LABEL + 18) };
                DrawTextW(hdc, lbl.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE);
                PenScope p(hdc, 1, isTimerActive ? COLOR_MUTED : COLOR_ITHILDIN);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, PX(143), PX((float)Y_EDIT - 1), PX(217), PX((float)Y_EDIT + 27));
            }

            // --- Ayarlar bölümü (veya açılış/kapanış animasyonu sırasında pürüzsüz kırpılmış önizleme)
            if (showSettings || currentBaseH > H_COL + 1.0f) {
                {
                    PenScope p(hdc, 1, COLOR_MUTED);
                    MoveToEx(hdc, PX(100), PX((float)Y_SEP), NULL);
                    LineTo(hdc, PX(260), PX((float)Y_SEP));
                }

                // Animasyon sürerken gerçek buton HWND'leri gizliyse, birebir aynı taş oyma yüzeyle çiz
                if (!IsWindowVisible(hBtnLang)) {
                    auto drawGhostBtn = [&](int yBase, const std::wstring& btxt) {
                        int bx = PX(71), by = PX((float)yBase), bw = PX(218), bh = PX(28);
                        HDC bDC = CreateCompatibleDC(hdc);
                        HBITMAP bBmp = CreateCompatibleBitmap(hdcWindow, bw, bh);
                        HGDIOBJ oldB = SelectObject(bDC, bBmp);
                        DrawMoriaButtonSurface(bDC, bx, by, bw, bh, btxt, hFontSmall, COLOR_TEXT, 0.0f, false, false);
                        BitBlt(hdc, bx, by, bw, bh, bDC, 0, 0, SRCCOPY);
                        SelectObject(bDC, oldB);
                        DeleteObject(bBmp);
                        DeleteDC(bDC);
                    };
                    drawGhostBtn(Y_LANG, Tr(S_LANG));
                    drawGhostBtn(Y_START, Tr(S_STARTUP) + OnOff(isStartupEnabled));
                }

                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, COLOR_TEXT);
                std::wstring opTxt = Tr(S_OPACITY) + PercentText(opacityVal);
                RECT rOp = { PX(58), PX((float)Y_OPLBL), PX((float)W_BASE - 58), PX((float)Y_OPLBL + 20) };
                DrawTextW(hdc, opTxt.c_str(), -1, &rOp, DT_CENTER | DT_SINGLELINE);

                int ty = PX((float)Y_OPSLIDER);
                {
                    PenScope p(hdc, 2, COLOR_MUTED);
                    MoveToEx(hdc, PX(80), ty, NULL); LineTo(hdc, PX(280), ty);
                }
                float opP = (opacityVal - 30) / 70.0f;
                int opCx = (int)(PX(80) + opP * PX(200));
                {
                    PenScope pGlowLine(hdc, 2, coreGlow);
                    MoveToEx(hdc, PX(80), ty, NULL); LineTo(hdc, opCx, ty);
                }
                {
                    PenScope p(hdc, 1, crispGlow);
                    BrushScope b(hdc, COLOR_ITHILDIN);
                    Ellipse(hdc, opCx - PX(7), ty - PX(7), opCx + PX(7), ty + PX(7));
                }
            }

            SelectClipRgn(hdc, NULL);
            DeleteObject(hDoorClip);

            SelectObject(hdc, origFont);
            BitBlt(hdcWindow, 0, 0, clientW, clientH, hdc, 0, 0, SRCCOPY);
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
            int slot = HoverSlot(id);
            float blend = (slot >= 0 && !chipOff) ? btnHoverBlend[slot] : 0.0f;
            bool chipSel = false;
            if (chip) {
                bool mel = false;
                int curMin = ReadMinutesFromEdit(&mel);
                chipSel = (!mel && curMin == PRESETS[id - ID_CHIP0]);
            }

            int bw = d->rcItem.right - d->rcItem.left;
            int bh = d->rcItem.bottom - d->rcItem.top;
            POINT ptMain = { 0, 0 };
            MapWindowPoints(d->hwndItem, hwnd, &ptMain, 1);

            HDC memDC = CreateCompatibleDC(d->hDC);
            HBITMAP memBmp = CreateCompatibleBitmap(d->hDC, bw, bh);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            COLORREF txt = COLOR_TEXT;
            std::wstring text;
            if (id == ID_BTN_ACTION) { text = Tr(isTimerActive ? S_CANCEL : S_START); txt = isTimerActive ? COLOR_RED : COLOR_ITHILDIN; }
            else if (id == ID_BTN_SETTINGS) { text = Tr(S_SETTINGS); txt = COLOR_ITHILDIN; }
            else if (id == ID_BTN_LANG) { text = Tr(S_LANG); }
            else if (id == ID_BTN_STARTUP) { text = Tr(S_STARTUP) + OnOff(isStartupEnabled); }
            else if (chip) {
                text = std::to_wstring(PRESETS[id - ID_CHIP0]) + L" " + Tr(S_UNIT);
                txt = chipOff ? COLOR_MUTED : (chipSel ? COLOR_ITHILDIN : LerpColor(COLOR_TEXT, COLOR_ITHILDIN, blend));
            }

            DrawMoriaButtonSurface(memDC, ptMain.x, ptMain.y, bw, bh,
                                   text, chip ? hFontChip : hFontSmall, txt,
                                   blend, chipSel, id == ID_BTN_SETTINGS);

            BitBlt(d->hDC, d->rcItem.left, d->rcItem.top, bw, bh, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
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
                        SetTimer(hwnd, ID_AUTOHIDE, 1800, NULL);
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
                expandFromH = currentBaseH;
                expandToH = (float)(showSettings ? H_EXP : H_COL);
                expandStartMs = NowMs();
                isExpandingAnim = true;

                ShowWindow(hBtnLang, SW_HIDE);
                ShowWindow(hBtnStartup, SW_HIDE);
                InvalidateRect(hBtnSettings, NULL, FALSE);
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
                else if (GetTickCount64() - lastHideTick > 400) ShowFlyout(true);
            } else if (lp == WM_RBUTTONUP) {
                ShowTrayMenu();
            } else if (lp == NIN_BALLOONUSERCLICK) {
                ShowFlyout(false);
            }
            break;
        }

        case WM_DESTROY: {
            SetHighResTimer(false);
            RemoveFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
            KillTimer(hwnd, ID_TIMER);
            KillTimer(hwnd, ID_HOVER_TIMER);
            KillTimer(hwnd, ID_ANIM_TIMER);
            FreeArtCache();
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
            DeleteObject(hChromaBrush); DeleteObject(hInputBrush);
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
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
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

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

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
    if (!RegisterClassExW(&wc)) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
                                L"MoriaFlyoutClass", L"M.O.R.I.A. - Doors of Durin",
                                WS_POPUP | WS_CLIPCHILDREN,
                                0, 0, W_BASE, H_EXP, NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    if (!trayOnly) ShowFlyout(false);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && flyoutVisible) {
            if (msg.wParam == VK_ESCAPE) { HideFlyout(); continue; }
            if (msg.wParam == VK_RETURN) { SendMessageW(hwnd, WM_COMMAND, ID_BTN_ACTION, 0); continue; }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}
