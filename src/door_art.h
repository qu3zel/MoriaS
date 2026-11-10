// door_art.h - Peter Jackson'ın Yüzüklerin Efendisi Filmindeki Durin Kapıları'nın
// Birebir Vektörel Rekonstrüksiyonu (32-bit DIB İndirgemeli + GDI+ Çok Katmanlı Ithildin Işıması)
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <math.h>

#ifndef W_BASE
#define W_BASE 360
#endif

static const float PI_F = 3.14159265f;

// =====================================================================
//  KLASİK GDI YARDIMCILARI
// =====================================================================
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
    POINT p[9];
    for (int i = 0; i <= 8; i++) {
        float t = i / 8.0f, u = 1.0f - t;
        p[i] = Pt(u * u * x0 + 2 * u * t * cx + t * t * x1,
                  u * u * y0 + 2 * u * t * cy + t * t * y1);
    }
    Polyline(hdc, p, 9);
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

// =====================================================================
//  GDI+ YARDIMCILARI (Çok Katmanlı Sinematik Ithildin Işıması)
// =====================================================================
static void DrawIthildinPath(Gdiplus::Graphics& g, const Gdiplus::GraphicsPath& path, float s,
                             float coreW = 1.25f, float glowScale = 1.0f) {
    Gdiplus::Pen pAura(Gdiplus::Color((BYTE)(44 * glowScale), 60, 175, 235), (coreW + 6.0f) * s);
    Gdiplus::Pen pMid(Gdiplus::Color((BYTE)(100 * glowScale), 120, 222, 255), (coreW + 2.5f) * s);
    Gdiplus::Pen pCore(Gdiplus::Color((BYTE)(242 * glowScale), 232, 252, 255), coreW * s);

    pAura.SetLineJoin(Gdiplus::LineJoinRound); pAura.SetStartCap(Gdiplus::LineCapRound); pAura.SetEndCap(Gdiplus::LineCapRound);
    pMid.SetLineJoin(Gdiplus::LineJoinRound);  pMid.SetStartCap(Gdiplus::LineCapRound);  pMid.SetEndCap(Gdiplus::LineCapRound);
    pCore.SetLineJoin(Gdiplus::LineJoinRound); pCore.SetStartCap(Gdiplus::LineCapRound); pCore.SetEndCap(Gdiplus::LineCapRound);

    pAura.SetMiterLimit(2.0f);
    pMid.SetMiterLimit(2.0f);
    pCore.SetMiterLimit(2.0f);

    g.DrawPath(&pAura, &path);
    g.DrawPath(&pMid, &path);
    g.DrawPath(&pCore, &path);
}

static void AddQuadBezierToPath(Gdiplus::GraphicsPath& path, float x0, float y0, float cx, float cy, float x1, float y1) {
    const int STEPS = 18;
    Gdiplus::PointF pts[STEPS + 1];
    for (int i = 0; i <= STEPS; i++) {
        float t = (float)i / (float)STEPS;
        float u = 1.0f - t;
        pts[i].X = u * u * x0 + 2.0f * u * t * cx + t * t * x1;
        pts[i].Y = u * u * y0 + 2.0f * u * t * cy + t * t * y1;
    }
    path.StartFigure();
    path.AddLines(pts, STEPS + 1);
}

static void AddCubicBezierToPath(Gdiplus::GraphicsPath& path,
                                 float x0, float y0, float c1x, float c1y, float c2x, float c2y, float x1, float y1) {
    const int STEPS = 24;
    Gdiplus::PointF pts[STEPS + 1];
    for (int i = 0; i <= STEPS; i++) {
        float t = (float)i / (float)STEPS;
        float u = 1.0f - t;
        float b0 = u * u * u;
        float b1 = 3.0f * u * u * t;
        float b2 = 3.0f * u * t * t;
        float b3 = t * t * t;
        pts[i].X = b0 * x0 + b1 * c1x + b2 * c2x + b3 * x1;
        pts[i].Y = b0 * y0 + b1 * c1y + b2 * c2y + b3 * y1;
    }
    path.StartFigure();
    path.AddLines(pts, STEPS + 1);
}

static void DrawQuadIthildin(Gdiplus::Graphics& g, float s, float x0, float y0, float cx, float cy, float x1, float y1,
                             float coreW = 1.25f, float glowScale = 1.0f) {
    Gdiplus::GraphicsPath path;
    AddQuadBezierToPath(path, x0, y0, cx, cy, x1, y1);
    DrawIthildinPath(g, path, s, coreW, glowScale);
}

static void DrawCubicIthildin(Gdiplus::Graphics& g, float s,
                              float x0, float y0, float c1x, float c1y, float c2x, float c2y, float x1, float y1,
                              float coreW = 1.25f, float glowScale = 1.0f) {
    Gdiplus::GraphicsPath path;
    AddCubicBezierToPath(path, x0, y0, c1x, c1y, c2x, c2y, x1, y1);
    DrawIthildinPath(g, path, s, coreW, glowScale);
}

// =====================================================================
//  1. PROSEDÜREL MORIA KAYA DOKUSU VE KAPI ÇATLAĞI (Filmdeki Taş Yüzey)
// =====================================================================
static unsigned int HashU32(unsigned int x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static float ValueNoise2D(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    auto rnd = [](int a, int b) -> float {
        return (HashU32((unsigned int)(a * 374761393 + b * 668265263)) & 0xFFFF) / 65535.0f;
    };
    float n00 = rnd(ix, iy),     n10 = rnd(ix + 1, iy);
    float n01 = rnd(ix, iy + 1), n11 = rnd(ix + 1, iy + 1);
    float nx0 = n00 + (n10 - n00) * ux;
    float nx1 = n01 + (n11 - n01) * ux;
    return nx0 + (nx1 - nx0) * uy;
}

static float FbmRock(float x, float y) {
    float v = 0.0f, amp = 0.5f;
    for (int i = 0; i < 4; i++) {
        v += ValueNoise2D(x, y) * amp;
        x = x * 2.07f + 13.5f;
        y = y * 2.07f + 7.2f;
        amp *= 0.5f;
    }
    return v;
}

static void RenderRockTextureDIB(HDC hdc, int w, int h, float s) {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HBITMAP hDib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hDib || !bits) return;

    DWORD* px = (DWORD*)bits;
    float cx = w * 0.5f, cy = h * 0.36f;
    float invS = 1.0f / (18.0f * s);

    for (int y = 0; y < h; y++) {
        float dy = (y - cy) / (h * 0.58f);
        for (int x = 0; x < w; x++) {
            float dx = (x - cx) / (w * 0.55f);
            float dist = sqrtf(dx * dx + dy * dy);
            float vig = 1.0f - dist * 0.45f;
            if (vig < 0.35f) vig = 0.35f;

            float n1 = FbmRock(x * invS, y * invS);
            float n2 = ValueNoise2D(x * invS * 3.5f + y * invS * 1.2f, y * invS * 3.5f - x * invS * 0.8f);
            float ridge = 1.0f - fabsf(n2 * 2.0f - 1.0f);
            float shade = (0.55f + 0.45f * n1) * (0.82f + 0.18f * ridge * ridge) * vig;

            int r = (int)(12.0f * shade + 4.0f);
            int g = (int)(20.0f * shade + 7.0f);
            int b = (int)(28.0f * shade + 10.0f);
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            px[y * w + x] = (DWORD)((r << 16) | (g << 8) | b);
        }
    }

    HDC mem = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem, hDib);
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    DeleteObject(hDib);
}

// =====================================================================
//  2. FİLMDEKİ BİREBİR DURİN'İN KAPISI MİMARİSİ (GDI+ Statik Katman)
// =====================================================================
static void DrawMovieAccurateMoriaDoor(Gdiplus::Graphics& g, float s, int w, int h) {
    (void)w;
    float cx = (W_BASE * 0.5f) * s;

    // Pencere kemeri ile 100% eşmerkezli kapı geometrisi:
    // yArcMid = 176*s, iç kemer rInner = 144*s (x = 36..324, tam sütun başlıklarının üzerine oturur!)
    float yArcMid = 176.0f * s;
    float rInner = 144.0f * s;
    float yCapTop = 176.0f * s;
    float yColTop = 186.0f * s;
    float yBot = (float)h - 6.0f * s;

    // -----------------------------------------------------------------
    // A. MERKEZİ KAPI KANADI AYRIM ÇATLAĞI (Filmdeki koyu taş birleşim yarığı)
    // -----------------------------------------------------------------
    {
        Gdiplus::Pen darkSeam(Gdiplus::Color(175, 4, 7, 10), 2.5f * s);
        Gdiplus::Pen faintIthildinSeam(Gdiplus::Color(45, 110, 205, 245), 1.0f * s);
        for (int y = (int)(yArcMid - rInner + 14 * s); y < (int)(yBot - 10 * s); y += (int)(8 * s)) {
            float j0 = sinf(y * 0.07f) * 0.8f * s;
            float j1 = sinf((y + 8 * s) * 0.07f) * 0.8f * s;
            g.DrawLine(&darkSeam, cx + j0, (float)y, cx + j1, (float)(y + 8 * s));
            if (y > (int)(260 * s))
                g.DrawLine(&faintIthildinSeam, cx + j0 + 1.0f * s, (float)y, cx + j1 + 1.0f * s, (float)(y + 8 * s));
        }
    }

    // -----------------------------------------------------------------
    // B. İÇ KEMER YAYI (Sütun başlıklarının tam üstüne oturan yarım daire kemer)
    // -----------------------------------------------------------------
    {
        Gdiplus::GraphicsPath innerArch;
        const int ARC_STEPS = 64;
        Gdiplus::PointF arcPts[ARC_STEPS + 1];
        for (int i = 0; i <= ARC_STEPS; i++) {
            float ang = PI_F * (1.0f - (float)i / (float)ARC_STEPS);
            arcPts[i].X = cx + rInner * cosf(ang);
            arcPts[i].Y = yArcMid - rInner * sinf(ang);
        }
        innerArch.AddLines(arcPts, ARC_STEPS + 1);
        DrawIthildinPath(g, innerArch, s, 1.40f, 0.98f);
    }

    // -----------------------------------------------------------------
    // C. İKİ İYONİK SÜTUN + VOLÜT (SPİRAL) BAŞLIKLARI (Fotoğraftaki birebir yapı)
    // -----------------------------------------------------------------
    float colCenters[2] = { 50.0f * s, (W_BASE - 50.0f) * s };
    float colHalfW = 7.0f * s;

    for (int side = 0; side < 2; side++) {
        float scx = colCenters[side];
        float xL = scx - colHalfW, xR = scx + colHalfW;

        // 1. Dikey çift sütun çizgileri
        Gdiplus::GraphicsPath colLines;
        colLines.StartFigure(); colLines.AddLine(xL, yColTop, xL, yBot);
        colLines.StartFigure(); colLines.AddLine(xR, yColTop, xR, yBot);
        colLines.StartFigure(); colLines.AddLine(xL + 2.4f * s, yColTop + 4.0f * s, xL + 2.4f * s, yBot - 6.0f * s);
        colLines.StartFigure(); colLines.AddLine(xR - 2.4f * s, yColTop + 4.0f * s, xR - 2.4f * s, yBot - 6.0f * s);
        DrawIthildinPath(g, colLines, s, 1.25f, 0.95f);

        // 2. İyonik Sütun Başlığı (Abaküs + Sol ve Sağda Spiral Volütler)
        float capW = 30.0f * s;
        float capH = 9.5f * s;
        Gdiplus::GraphicsPath capPath;
        capPath.AddRectangle(Gdiplus::RectF(scx - capW * 0.5f, yCapTop, capW, capH));
        capPath.AddRectangle(Gdiplus::RectF(scx - capW * 0.40f, yCapTop + 2.2f * s, capW * 0.80f, capH - 4.4f * s));
        DrawIthildinPath(g, capPath, s, 1.35f, 1.0f);

        for (int vs = -1; vs <= 1; vs += 2) {
            float vx = scx + vs * (capW * 0.5f - 2.8f * s);
            float vy = yCapTop + capH * 0.55f;
            float vr = 4.2f * s;
            Gdiplus::GraphicsPath volute;
            volute.AddEllipse(vx - vr, vy - vr, vr * 2.0f, vr * 2.0f);
            volute.AddEllipse(vx - vr * 0.45f, vy - vr * 0.45f, vr * 0.9f, vr * 0.9f);
            DrawIthildinPath(g, volute, s, 1.15f, 0.92f);
        }
    }

    // -----------------------------------------------------------------
    // D. FİLMDEKİ TELPERION VE LAURELIN AĞAÇLARI
    //    (Sütunlara Sarılan S-Gövdeler + Yazılara/Çubuğa Değmeyen Simetrik Lir Dallar)
    // -----------------------------------------------------------------
    for (int side = 0; side < 2; side++) {
        float scx = colCenters[side];
        float inDir = (side == 0) ? 1.0f : -1.0f;
        float outDir = -inDir;

        // 1. Sütuna sarılan 3 kademeli S-boğumu ve dışa kıvrılan kanca sürgünler
        float wrapY[3] = { 228.0f * s, 286.0f * s, 344.0f * s };
        for (int wIdx = 0; wIdx < 3; wIdx++) {
            float wy = wrapY[wIdx];
            DrawCubicIthildin(g, s,
                              scx + inDir * colHalfW, wy + 22.0f * s,
                              scx + inDir * 1.5f * s, wy + 7.0f * s,
                              scx + outDir * (colHalfW + 5.0f * s), wy - 5.0f * s,
                              scx + outDir * (colHalfW + 8.5f * s), wy - 18.0f * s,
                              1.30f, 0.98f);

            // Dış kanca ucu (yukarı ve içe kıvrılan zarif Elf sürgünü)
            float tipX = scx + outDir * (colHalfW + 8.5f * s);
            float tipY = wy - 18.0f * s;
            DrawQuadIthildin(g, s,
                             tipX, tipY,
                             tipX + outDir * 2.0f * s, tipY - 8.0f * s,
                             tipX + inDir * 4.0f * s, tipY - 10.5f * s,
                             1.10f, 0.92f);

            // İç taraftan sütuna paralel yükselen ana ağaç gövdesi kolu
            DrawQuadIthildin(g, s,
                             scx + inDir * (colHalfW + 2.5f * s), wy + 26.0f * s,
                             scx + inDir * (colHalfW + 8.0f * s), wy + 4.0f * s,
                             scx + inDir * (colHalfW + 3.0f * s), wy - 20.0f * s,
                             1.25f, 0.95f);
        }

        // 2. İçeriye doğru kavislenen, hiçbir metne ("DURİN'İN...", "01:30:00", ilerleme çubuğu)
        //    değmeyen, sol ve sağda %100 ayna simetrik lir/kanopi dal çifti:
        struct CanopyBranch {
            float startY;
            float ctrl1X, ctrl1Y;
            float ctrl2X, ctrl2Y;
            float endX, endY;
            float subEndX, subEndY;
        };
        CanopyBranch cb[4] = {
            // 1. Üst kemer altı dalı (alt başlığın sol/sağ dışından zarifçe yükselir: x = 50+23 = 73)
            { 192.0f, 9.0f, 164.0f, 16.0f, 144.0f, 23.0f, 136.0f, 16.0f, 126.0f },
            // 2. Saatin üst-yanını çerçeveleyen lir dalı (x = 50+30 = 80)
            { 212.0f, 12.0f, 190.0f, 22.0f, 180.0f, 30.0f, 184.0f, 22.0f, 170.0f },
            // 3. Saatin alt-yanını çerçeveleyen lir dalı (ilerleme çubuğunun dış kenarında durur: x = 50+26 = 76)
            { 244.0f, 11.0f, 226.0f, 20.0f, 218.0f, 26.0f, 222.0f, 19.0f, 208.0f },
            // 4. Alt-iç sürgün (butonların yanından içe kıvrılan dal: x = 50+17 = 67)
            { 306.0f, 10.0f, 292.0f, 15.0f, 294.0f, 17.0f, 306.0f, 13.0f, 284.0f }
        };

        for (int b = 0; b < 4; b++) {
            float sx = scx + inDir * (colHalfW + 2.0f * s);
            float sy = cb[b].startY * s;
            float c1x = scx + inDir * cb[b].ctrl1X * s, c1y = cb[b].ctrl1Y * s;
            float c2x = scx + inDir * cb[b].ctrl2X * s, c2y = cb[b].ctrl2Y * s;
            float ex  = scx + inDir * cb[b].endX * s,   ey  = cb[b].endY * s;

            DrawCubicIthildin(g, s, sx, sy, c1x, c1y, c2x, c2y, ex, ey, 1.30f, 0.98f);

            float mx = 0.125f * sx + 0.375f * (c1x + c2x) + 0.125f * ex;
            float my = 0.125f * sy + 0.375f * (c1y + c2y) + 0.125f * ey;
            float subX = scx + inDir * cb[b].subEndX * s;
            float subY = cb[b].subEndY * s;
            DrawQuadIthildin(g, s, mx, my, (mx + subX) * 0.5f, subY - 4.0f * s, subX, subY, 1.00f, 0.90f);

            // Hilal/kanca biçimli zarif yaprak uçları
            DrawQuadIthildin(g, s,
                             ex, ey,
                             ex + inDir * 3.0f * s, ey + 2.8f * s,
                             ex + outDir * 1.8f * s, ey + 5.0f * s,
                             1.00f, 0.90f);
            DrawQuadIthildin(g, s,
                             subX, subY,
                             subX + inDir * 2.5f * s, subY - 2.8f * s,
                             subX + outDir * 1.5f * s, subY - 4.2f * s,
                             0.90f, 0.85f);
        }
    }

    // -----------------------------------------------------------------
    // E. İÇ KEMER TENGWAR YAZITI (Fotoğraftaki 2. satır: "Im Narvi hain echant...")
    // -----------------------------------------------------------------
    {
        float rInnerText = rInner - 7.5f * s;
        int nGlyphs = 36;
        for (int i = 0; i < nGlyphs; i++) {
            float t = (i + 0.5f) / (float)nGlyphs;
            float ang = PI_F * (0.93f - t * 0.86f);
            float px = cx + rInnerText * cosf(ang);
            float py = yArcMid - rInnerText * sinf(ang);
            float tx = sinf(ang), ty = cosf(ang);
            float nx = cosf(ang), ny = -sinf(ang);

            auto P = [&](float u, float v) {
                return Gdiplus::PointF(px + (tx * u + nx * v) * s, py + (ty * u + ny * v) * s);
            };

            Gdiplus::GraphicsPath gp;
            int gl = (i * 7 + 3) % 6;
            gp.StartFigure();
            gp.AddLine(P(0, -2.4f), P(0, 2.4f));
            if (gl == 0 || gl == 2) {
                AddQuadBezierToPath(gp, P(0, -1.7f).X, P(0, -1.7f).Y, P(2.8f, 0).X, P(2.8f, 0).Y, P(0, 1.7f).X, P(0, 1.7f).Y);
            } else if (gl == 1 || gl == 4) {
                AddQuadBezierToPath(gp, P(0, -1.7f).X, P(0, -1.7f).Y, P(-2.8f, 0).X, P(-2.8f, 0).Y, P(0, 1.7f).X, P(0, 1.7f).Y);
            } else {
                gp.StartFigure();
                gp.AddLine(P(-1.7f, -1.3f), P(1.7f, -1.3f));
            }
            DrawIthildinPath(g, gp, s, 0.80f, 0.72f);
        }
    }

    // -----------------------------------------------------------------
    // F. DURİN'İN AMBLEMİ: ELMAS DİLİMLİ TAÇ + ÇEKİÇ VE ÖRS (Fotoğraftaki birebir şekil)
    // -----------------------------------------------------------------
    {
        float cyCrown = 74.0f * s;
        Gdiplus::GraphicsPath crownPath;
        Gdiplus::PointF cMid[4] = {
            Gdiplus::PointF(cx, cyCrown - 12.0f * s),
            Gdiplus::PointF(cx + 6.5f * s, cyCrown),
            Gdiplus::PointF(cx, cyCrown + 9.0f * s),
            Gdiplus::PointF(cx - 6.5f * s, cyCrown)
        };
        crownPath.AddPolygon(cMid, 4);

        for (int side = -1; side <= 1; side += 2) {
            float d = (float)side;
            Gdiplus::PointF cSide1[4] = {
                Gdiplus::PointF(cx + d * 9.5f * s, cyCrown - 9.5f * s),
                Gdiplus::PointF(cx + d * 14.5f * s, cyCrown + 1.0f * s),
                Gdiplus::PointF(cx + d * 6.0f * s, cyCrown + 9.0f * s),
                Gdiplus::PointF(cx + d * 4.5f * s, cyCrown - 1.0f * s)
            };
            Gdiplus::PointF cSide2[4] = {
                Gdiplus::PointF(cx + d * 17.5f * s, cyCrown - 5.0f * s),
                Gdiplus::PointF(cx + d * 21.0f * s, cyCrown + 4.0f * s),
                Gdiplus::PointF(cx + d * 12.0f * s, cyCrown + 9.5f * s),
                Gdiplus::PointF(cx + d * 11.5f * s, cyCrown + 0.5f * s)
            };
            crownPath.AddPolygon(cSide1, 4);
            crownPath.AddPolygon(cSide2, 4);
        }
        AddQuadBezierToPath(crownPath, cx - 18.0f * s, cyCrown + 9.5f * s, cx, cyCrown + 12.5f * s, cx + 18.0f * s, cyCrown + 9.5f * s);
        DrawIthildinPath(g, crownPath, s, 1.20f, 1.0f);

        // 2. Çekiç ve Örs
        float yb = 103.0f * s;
        Gdiplus::GraphicsPath anvilPath;
        const float ax[10] = { -16, 14, 14, 8, 6, 11, -11, -6, -5, -12 };
        const float ay[10] = { -9,  -9, -6, -5, -2, 1,  1,  -2, -5, -6 };
        Gdiplus::PointF anvilPts[10];
        for (int i = 0; i < 10; i++) anvilPts[i] = Gdiplus::PointF(cx + ax[i] * s, yb + ay[i] * s);
        anvilPath.AddPolygon(anvilPts, 10);

        Gdiplus::PointF hamHead[4] = {
            Gdiplus::PointF(cx - 10.0f * s, yb - 16.5f * s),
            Gdiplus::PointF(cx + 3.0f * s,  yb - 16.5f * s),
            Gdiplus::PointF(cx + 5.0f * s,  yb - 12.0f * s),
            Gdiplus::PointF(cx - 12.0f * s, yb - 12.0f * s)
        };
        anvilPath.AddPolygon(hamHead, 4);
        anvilPath.StartFigure();
        anvilPath.AddLine(cx - 3.0f * s, yb - 16.5f * s, cx + 12.0f * s, yb - 14.0f * s);
        anvilPath.AddLine(cx + 12.0f * s, yb - 12.0f * s, cx - 3.0f * s, yb - 12.0f * s);
        DrawIthildinPath(g, anvilPath, s, 1.20f, 1.0f);
    }

    // -----------------------------------------------------------------
    // G. FËANOR'UN YILDIZI (Dakika etiketine ve alt butona değmeyen kusursuz oran)
    // -----------------------------------------------------------------
    {
        float fy = 335.0f * s;
        Gdiplus::SolidBrush auraBr(Gdiplus::Color(36, 90, 205, 255));
        g.FillEllipse(&auraBr, cx - 38.0f * s, fy - 38.0f * s, 76.0f * s, 76.0f * s);

        Gdiplus::GraphicsPath starP;
        int n = 8;
        Gdiplus::PointF pts1[16], pts2[16];
        for (int i = 0; i < 16; i++) {
            float a1 = -PI_F / 2.0f + i * PI_F / n;
            // Dikey ışınlar (i=0, 8) 26*s ile "Dakika" etiketinin altına kadar uzanır, üstüne binmez;
            // Yatay/çapraz ışınlar 36*s ile giriş kutusunun yanlarından zarifçe taşar.
            float maxR = (i % 4 == 0) ? 26.5f * s : 36.0f * s;
            float r1 = (i % 2 == 0) ? maxR : 10.5f * s;
            pts1[i] = Gdiplus::PointF(cx + cosf(a1) * r1, fy + sinf(a1) * r1);

            float a2 = -PI_F / 2.0f + PI_F / 8.0f + i * PI_F / n;
            float r2 = (i % 2 == 0) ? 25.0f * s : 8.0f * s;
            pts2[i] = Gdiplus::PointF(cx + cosf(a2) * r2, fy + sinf(a2) * r2);
        }
        starP.AddPolygon(pts1, 16);
        starP.AddPolygon(pts2, 16);
        DrawIthildinPath(g, starP, s, 1.00f, 0.75f);
    }
}

// =====================================================================
//  3. DİNAMİK KATMAN (144Hz/240Hz — Saat Yönünde Dönen Kemer Rünleri, 7 Yıldız ve Alt Mühür)
// =====================================================================
static void DrawFlowingInscriptionFastGDI(HDC hdc, float s, float x1, float x2,
                                          float yArcMid, float yBottom, double timeMs,
                                          COLORREF glow, COLORREF crisp) {
    (void)x1; (void)x2;
    float cx = (W_BASE * 0.5f) * s;
    float rRune = 160.0f * s;
    float yEnd = yBottom - 10.0f * s;
    float side = yEnd - yArcMid;
    if (side < 0) side = 0;
    float arc = PI_F * rRune;
    float total = 2.0f * side + arc;
    int n = (int)(total / (11.5f * s));
    if (n < 8) return;

    float spacing = total / (float)n;
    float speedPxPerSec = 18.0f * s;
    float scrollOffset = (float)fmod((timeMs * 0.001) * (double)speedPxPerSec, (double)total);

    for (int pass = 0; pass < 2; pass++) {
        int pw = (pass == 0) ? (int)(2.2f * s + 0.5f) : 1;
        COLORREF col = (pass == 0) ? glow : crisp;
        PenScope pen(hdc, pw, col);

        for (int i = 0; i < n; i++) {
            float d = fmodf(i * spacing + scrollOffset, total);
            if (d < 8.0f * s || total - d < 8.0f * s) continue;

            float px, py, tx, ty, nx, ny;
            if (d < side) {
                px = cx - rRune; py = yEnd - d;
                tx = 0; ty = -1; nx = -1; ny = 0;
            } else if (d < side + arc) {
                float ang = PI_F - (d - side) / rRune;
                px = cx + rRune * cosf(ang); py = yArcMid - rRune * sinf(ang);
                tx = sinf(ang); ty = cosf(ang); nx = cosf(ang); ny = -sinf(ang);
            } else {
                px = cx + rRune; py = yArcMid + (d - side - arc);
                tx = 0; ty = 1; nx = 1; ny = 0;
            }

            auto P = [&](float u, float v) {
                return Pt(px + (tx * u + nx * v) * s, py + (ty * u + ny * v) * s);
            };
            auto line = [&](float u0, float v0, float u1, float v1) {
                POINT a = P(u0, v0), b = P(u1, v1);
                MoveToEx(hdc, a.x, a.y, NULL); LineTo(hdc, b.x, b.y);
            };
            auto bow = [&](float v0, float v1, float u) {
                POINT a = P(0, v0), c = P(u, (v0 + v1) * 0.5f), b = P(0, v1);
                DrawCurve(hdc, (float)a.x, (float)a.y, (float)c.x, (float)c.y, (float)b.x, (float)b.y);
            };

            int glyph = (i * 5 + (i / 3) * 2) % 9;
            float stemTop = (glyph % 2 == 0) ? -4.2f : -2.6f;
            float stemBot = (glyph == 3 || glyph == 7) ? 4.4f : 3.0f;
            line(0, stemTop, 0, stemBot);

            switch (glyph) {
                case 0: bow(-2.4f, 2.4f, 4.4f); line(-1.5f, -4.8f, 1.5f, -3.8f); break;
                case 1: bow(0, 3.5f, 4.4f); break;
                case 2: bow(-3.5f, 0, -4.4f); line(0, -5.0f, 0.8f, -5.0f); break;
                case 3: bow(-3.5f, 0, 4.4f); bow(0, 3.5f, 4.4f); break;
                case 4: line(1.8f, 1.8f, 3.4f, 1.8f); line(1.8f, 3.4f, 3.4f, 3.4f); break;
                case 5: bow(-2.5f, 2.2f, -4.3f); line(-2.0f, 0, 3.4f, 0); break;
                case 6: bow(-3.5f, 0, -4.4f); bow(0, 3.5f, -4.4f); break;
                case 7: bow(-2.2f, 2.5f, 4.3f); line(-1.0f, -4.8f, 2.0f, -4.8f); break;
                default: bow(-1.5f, 2.8f, 3.9f); break;
            }
        }
    }
}

static void DrawSevenStarsFastGDI(HDC hdc, float s, float cx, float cy, double timeMs,
                                  COLORREF midGlow, COLORREF crisp, float starR) {
    for (int k = 0; k < 7; k++) {
        float th = (-54.0f + 108.0f * k / 6.0f) * PI_F / 180.0f;
        float R = starR * s;
        float px = cx + R * sinf(th), py = cy - R * cosf(th);
        float tw = 0.82f + 0.18f * (float)sin(timeMs * 0.0045 + k * 1.15);
        float r = ((k == 3) ? 8.2f : 6.0f) * s * tw;

        BrushScope br(hdc, midGlow);
        PenScope pen(hdc, 1, crisp);
        DrawStarShape(hdc, px, py, r, r * 0.20f, 8, -PI_F / 2.0f);
    }
}

// Kapı alt eşiğinin hemen üzerinde yer alan kaligrafik "C / J" usta mührü
// (Ayarlar açılıp kapandığında kapı alt eşiğiyle birlikte hareket eder, butonlarla asla çakışmaz)
static void DrawMakerMarkFastGDI(HDC hdc, float s, float cx, float my, COLORREF midGlow, COLORREF coreGlow) {
    for (int pass = 0; pass < 2; pass++) {
        int pw = (pass == 0) ? (int)(2.5f * s + 0.5f) : 1;
        COLORREF col = (pass == 0) ? midGlow : coreGlow;
        PenScope pen(hdc, pw, col);
        DrawCurve(hdc, cx - 10.0f * s, my + 3.0f * s, cx, my - 8.5f * s, cx + 10.0f * s, my + 3.0f * s);
        DrawCurve(hdc, cx, my - 4.0f * s, cx + 2.0f * s, my + 7.5f * s, cx - 5.0f * s, my + 9.5f * s);
    }
}
