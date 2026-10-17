// door_art.h - Durin Kapısı süslemeleri (yalnızca GDI; harici görsel yok)
// Elfçe yazıt şeridi, ağaçlar, yedi yıldız, taç, çekiç ve örs.
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <math.h>

// =====================================================================
//  GDI YARDIMCILARI + KAPI SÜSLEMELERİ
// =====================================================================
static const float PI_F = 3.14159265f;

struct PenScope {
    HDC dc; HPEN pen; HGDIOBJ oldPen;
    PenScope(HDC d, int w, COLORREF c) : dc(d) {
        pen = CreatePen(PS_SOLID, w < 1 ? 1 : w, c);
        oldPen = SelectObject(dc, pen);
    }
    ~PenScope() { SelectObject(dc, oldPen); DeleteObject(pen); }
};

struct BrushScope {
    HDC dc; HBRUSH br; HGDIOBJ oldBr;
    BrushScope(HDC d, COLORREF c) : dc(d) {
        br = CreateSolidBrush(c);
        oldBr = SelectObject(dc, br);
    }
    ~BrushScope() { SelectObject(dc, oldBr); DeleteObject(br); }
};

static POINT Pt(float x, float y) {
    POINT p; p.x = (LONG)floorf(x + 0.5f); p.y = (LONG)floorf(y + 0.5f); return p;
}

static void DrawCurve(HDC hdc, float x0, float y0, float cx, float cy, float x1, float y1) {
    POINT p[13];
    for (int i = 0; i <= 12; i++) {
        float t = i / 12.0f, u = 1.0f - t;
        p[i] = Pt(u*u*x0 + 2*u*t*cx + t*t*x1, u*u*y0 + 2*u*t*cy + t*t*y1);
    }
    Polyline(hdc, p, 13);
}

static void DrawStarShape(HDC hdc, float cx, float cy, float rOut, float rIn, int n, float rot) {
    POINT p[40];
    int cnt = n * 2;
    if (cnt > 40) cnt = 40;
    for (int i = 0; i < cnt; i++) {
        float a = rot + i * PI_F / n;
        float r = (i % 2 == 0) ? rOut : rIn;
        p[i] = Pt(cx + cosf(a) * r, cy + sinf(a) * r);
    }
    Polygon(hdc, p, cnt);
}

static void DrawBranch(HDC hdc, float baseX, float baseY, float dir, float lx, float ly,
                       float a, float len, int depth, float s, COLORREF col, COLORREF leafCol) {
    float ex = lx + cosf(a) * len,                 ey = ly + sinf(a) * len;
    float cx = lx + cosf(a - 0.28f) * len * 0.55f, cy = ly + sinf(a - 0.28f) * len * 0.55f;

    float X0 = baseX + dir * lx, Y0 = baseY + ly;
    float XC = baseX + dir * cx, YC = baseY + cy;
    float X1 = baseX + dir * ex, Y1 = baseY + ey;

    {
        int w = (int)((depth >= 3 ? 2 : 1) * s + 0.5f);
        PenScope pen(hdc, w, col);
        DrawCurve(hdc, X0, Y0, XC, YC, X1, Y1);
    }

    if (depth <= 0) {
        float dx = cosf(a) * dir, dy = sinf(a);
        POINT leaf[4] = {
            Pt(X1 + dx * 3.5f * s, Y1 + dy * 3.5f * s),
            Pt(X1 - dy * 1.8f * s, Y1 + dx * 1.8f * s),
            Pt(X1 - dx * 1.5f * s, Y1 - dy * 1.5f * s),
            Pt(X1 + dy * 1.8f * s, Y1 - dx * 1.8f * s)
        };
        BrushScope br(hdc, leafCol);
        PenScope pen(hdc, 1, leafCol);
        Polygon(hdc, leaf, 4);
        return;
    }

    const float da = 0.5f, pull = 0.55f, ratio = 0.72f;
    auto child = [&](float off, float f) {
        float na = a + off;
        na += (-PI_F / 2.0f - na) * pull;
        DrawBranch(hdc, baseX, baseY, dir, ex, ey, na, len * f, depth - 1, s, col, leafCol);
    };
    child(-da, ratio);
    child(da * 0.9f, ratio);
    if (depth % 2 == 0) child(0.0f, 0.62f);
}

static void DrawTree(HDC hdc, float baseX, float baseY, float dir, float trunkLen, float s,
                     COLORREF col, COLORREF leafCol) {
    for (int sg = -1; sg <= 1; sg += 2) {
        PenScope pen(hdc, (int)(2 * s + 0.5f), col);
        DrawCurve(hdc, baseX, baseY - 3 * s, baseX + sg * 4 * s, baseY, baseX + sg * 13 * s, baseY + 3 * s);
    }
    float startY = baseY - trunkLen;
    {
        PenScope pen(hdc, (int)(3 * s + 0.5f), col);
        MoveToEx(hdc, (int)(baseX + 0.5f), (int)(baseY + 0.5f), NULL);
        LineTo(hdc, (int)(baseX + 0.5f), (int)(startY + 0.5f));
    }
    DrawBranch(hdc, baseX, startY, dir, 0.0f, 0.0f, -PI_F / 2.0f, 36.0f * s, 4, s, col, leafCol);
}

static void DrawInscription(HDC hdc, float s, float x1, float x2, float yArcMid, float yBottom, COLORREF col) {
    float cx = (x1 + x2) / 2.0f;
    float rm = (x2 - x1) / 2.0f - 8.0f * s;
    float yEnd = yBottom - 10.0f * s;
    float side = yEnd - yArcMid;
    if (side < 0) side = 0;
    float arc = PI_F * rm;
    float total = 2.0f * side + arc;
    int n = (int)(total / (13.0f * s));
    if (n < 1) return;

    PenScope pen(hdc, 1, col);
    for (int i = 0; i < n; i++) {
        float d = (i + 0.5f) * total / n;
        float px, py, tx, ty, nx, ny;
        if (d < side) {
            px = cx - rm; py = yEnd - d; tx = 0; ty = -1; nx = -1; ny = 0;
        } else if (d < side + arc) {
            float ang = PI_F - (d - side) / rm;
            px = cx + rm * cosf(ang); py = yArcMid - rm * sinf(ang);
            tx = sinf(ang); ty = cosf(ang); nx = cosf(ang); ny = -sinf(ang);
        } else {
            px = cx + rm; py = yArcMid + (d - side - arc); tx = 0; ty = 1; nx = 1; ny = 0;
        }
        auto P = [&](float u, float v) {
            return Pt(px + (tx * u + nx * v) * s, py + (ty * u + ny * v) * s);
        };
        auto line = [&](float u0, float v0, float u1, float v1) {
            POINT a = P(u0, v0), b = P(u1, v1);
            MoveToEx(hdc, a.x, a.y, NULL); LineTo(hdc, b.x, b.y);
        };
        auto bow = [&](float v0, float v1, float u) {
            POINT a = P(0, v0), c = P(u, (v0 + v1) / 2), b = P(0, v1);
            DrawCurve(hdc, (float)a.x, (float)a.y, (float)c.x, (float)c.y, (float)b.x, (float)b.y);
        };

        line(0, -4, 0, 4);
        switch ((i * 5 + (i / 3) * 2) % 7) {
            case 1: bow(0, 4, 5); break;
            case 2: bow(-4, 0, -5); break;
            case 3: bow(-4, 0, 5); bow(0, 4, 5); break;
            case 4: line(2, 2, 3.5f, 2); line(2, 3.5f, 3.5f, 3.5f); break;
            case 5: line(0, 0, 4, 0); break;
            case 6: bow(-4, 0, -5); bow(0, 4, -5); break;
            default: break;
        }
    }
}

// Tepe amblemi: yedi yıldız, taç, çekiç ve örs
static void DrawEmblem(HDC hdc, float s, float cx, float cy, double timeMs, COLORREF glow, COLORREF crisp,
                       float starR, float crownY, float anvilY) {
    for (int k = 0; k < 7; k++) {
        float th = (-40.0f + 80.0f * k / 6.0f) * PI_F / 180.0f;
        float R = starR * s;
        float px = cx + R * sinf(th), py = cy - R * cosf(th);
        float tw = 0.85f + 0.15f * (float)sin(timeMs * 0.004 + k * 1.3);
        float r = ((k == 3) ? 6.5f : 4.5f) * s * tw;
        BrushScope br(hdc, glow);
        PenScope pen(hdc, 1, crisp);
        DrawStarShape(hdc, px, py, r, r * 0.42f, 7, -PI_F / 2.0f);
    }

    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    float y0 = crownY * s;
    POINT crown[7] = {
        Pt(cx - 18*s, y0 + 12*s), Pt(cx - 18*s, y0 + 3*s), Pt(cx - 9*s, y0 + 8*s), Pt(cx, y0),
        Pt(cx + 9*s, y0 + 8*s),   Pt(cx + 18*s, y0 + 3*s), Pt(cx + 18*s, y0 + 12*s)
    };

    float yb = anvilY * s;
    const float ax[10] = { -17, 13, 13, 8, 6, 10, -10, -6, -5, -11 };
    const float ay[10] = { -11, -11, -8, -7, -3, 0, 0, -3, -7, -8 };
    POINT anvil[10];
    for (int i = 0; i < 10; i++) anvil[i] = Pt(cx + ax[i] * s, yb + ay[i] * s);

    float hx0 = 0, hy0 = -11, hx1 = -9, hy1 = -22;
    float dx = hx1 - hx0, dy = hy1 - hy0;
    float L = sqrtf(dx * dx + dy * dy);
    dx /= L; dy /= L;
    float pxn = -dy, pyn = dx;
    POINT head[4] = {
        Pt(cx + (hx1 + pxn * 5 + dx * 2.5f) * s, yb + (hy1 + pyn * 5 + dy * 2.5f) * s),
        Pt(cx + (hx1 - pxn * 5 + dx * 2.5f) * s, yb + (hy1 - pyn * 5 + dy * 2.5f) * s),
        Pt(cx + (hx1 - pxn * 5 - dx * 2.5f) * s, yb + (hy1 - pyn * 5 - dy * 2.5f) * s),
        Pt(cx + (hx1 + pxn * 5 - dx * 2.5f) * s, yb + (hy1 + pyn * 5 - dy * 2.5f) * s)
    };

    for (int pass = 0; pass < 2; pass++) {
        PenScope pen(hdc, pass == 0 ? (int)(3 * s) : 1, pass == 0 ? glow : crisp);
        Polygon(hdc, crown, 7);
        Polygon(hdc, anvil, 10);
        Polygon(hdc, head, 4);
        MoveToEx(hdc, (int)(cx + hx0 * s + 0.5f), (int)(yb + hy0 * s + 0.5f), NULL);
        LineTo(hdc, (int)(cx + hx1 * s + 0.5f), (int)(yb + hy1 * s + 0.5f));
    }
    SelectObject(hdc, oldB);
}

