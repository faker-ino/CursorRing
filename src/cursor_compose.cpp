#include "cursor_compose.h"
#include "settings.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// ── Pixel buffer is a top-down 32bpp BGRA DIB (straight, non-premultiplied
//    alpha) - same layout DrawIconEx leaves behind for a 32bpp color+alpha
//    source icon/cursor. ───────────────────────────────────────────────────

namespace
{
    void BlendRing(BYTE* pixels, int width, int height, float cx, float cy,
                   const Settings::RingSettings& ring)
    {
        if (!ring.Enabled || ring.Color[3] <= 0.0f) return;

        const float radius = ring.Radius;
        const float halfThickness = ring.Thickness * 0.5f;
        const float pad = 1.0f; // AA feather width on each side of the stroke

        int minX = (int)std::floor(cx - (radius + halfThickness + pad));
        int maxX = (int)std::ceil (cx + (radius + halfThickness + pad));
        int minY = (int)std::floor(cy - (radius + halfThickness + pad));
        int maxY = (int)std::ceil (cy + (radius + halfThickness + pad));

        minX = std::max(minX, 0);
        minY = std::max(minY, 0);
        maxX = std::min(maxX, width - 1);
        maxY = std::min(maxY, height - 1);

        const float srcR = ring.Color[0];
        const float srcG = ring.Color[1];
        const float srcB = ring.Color[2];
        const float srcA = ring.Color[3];

        const float edge0 = halfThickness - 0.5f;
        const float edge1 = halfThickness + 0.5f;

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                float dx = (x + 0.5f) - cx;
                float dy = (y + 0.5f) - cy;
                float distFromStroke = std::fabs(std::sqrt(dx * dx + dy * dy) - radius);

                float coverage;
                if (distFromStroke <= edge0)      coverage = 1.0f;
                else if (distFromStroke >= edge1) coverage = 0.0f;
                else                               coverage = (edge1 - distFromStroke) / (edge1 - edge0);

                float a = coverage * srcA;
                if (a <= 0.0f) continue;

                BYTE* px = pixels + (size_t)(y * width + x) * 4;
                float dstB = px[0] / 255.0f, dstG = px[1] / 255.0f, dstR = px[2] / 255.0f, dstA = px[3] / 255.0f;

                float outA = a + dstA * (1.0f - a);
                float outR = 0.0f, outG = 0.0f, outB = 0.0f;
                if (outA > 0.0001f)
                {
                    outR = (srcR * a + dstR * dstA * (1.0f - a)) / outA;
                    outG = (srcG * a + dstG * dstA * (1.0f - a)) / outA;
                    outB = (srcB * a + dstB * dstA * (1.0f - a)) / outA;
                }

                px[0] = (BYTE)(std::clamp(outB, 0.0f, 1.0f) * 255.0f + 0.5f);
                px[1] = (BYTE)(std::clamp(outG, 0.0f, 1.0f) * 255.0f + 0.5f);
                px[2] = (BYTE)(std::clamp(outR, 0.0f, 1.0f) * 255.0f + 0.5f);
                px[3] = (BYTE)(std::clamp(outA, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
    }
}

HCURSOR CursorCompose::BuildComposite(HCURSOR aOriginal)
{
    if (!aOriginal) return aOriginal;
    if (!Settings::Outer.Enabled && !Settings::Inner.Enabled) return aOriginal;

    ICONINFO ii = {};
    if (!GetIconInfo(aOriginal, &ii)) return aOriginal;

    int origW = 0, origH = 0;
    if (ii.hbmColor)
    {
        BITMAP bm = {};
        GetObject(ii.hbmColor, sizeof(bm), &bm);
        origW = bm.bmWidth;
        origH = bm.bmHeight;
    }
    else if (ii.hbmMask)
    {
        // Legacy mono cursors pack the AND and XOR masks stacked vertically.
        BITMAP bm = {};
        GetObject(ii.hbmMask, sizeof(bm), &bm);
        origW = bm.bmWidth;
        origH = bm.bmHeight / 2;
    }

    if (origW <= 0 || origH <= 0)
    {
        if (ii.hbmMask)  DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        return aOriginal;
    }

    float maxExtent = 0.0f;
    if (Settings::Outer.Enabled)
        maxExtent = std::max(maxExtent, Settings::Outer.Radius + Settings::Outer.Thickness * 0.5f);
    if (Settings::Inner.Enabled)
        maxExtent = std::max(maxExtent, Settings::Inner.Radius + Settings::Inner.Thickness * 0.5f);

    const int margin = (int)std::ceil(maxExtent) + 2; // +2px AA padding
    const int newW = origW + margin * 2;
    const int newH = origH + margin * 2;

    BITMAPV5HEADER bi   = {};
    bi.bV5Size          = sizeof(BITMAPV5HEADER);
    bi.bV5Width         = newW;
    bi.bV5Height        = -newH; // negative = top-down DIB
    bi.bV5Planes        = 1;
    bi.bV5BitCount      = 32;
    bi.bV5Compression   = BI_BITFIELDS;
    bi.bV5RedMask       = 0x00ff0000;
    bi.bV5GreenMask     = 0x0000ff00;
    bi.bV5BlueMask      = 0x000000ff;
    bi.bV5AlphaMask     = 0xff000000;

    void* pvBits = nullptr;
    HBITMAP hColorBmp = CreateDIBSection(nullptr, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    if (!hColorBmp || !pvBits)
    {
        if (hColorBmp) DeleteObject(hColorBmp);
        if (ii.hbmMask)  DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        return aOriginal;
    }
    std::memset(pvBits, 0, (size_t)newW * newH * 4);

    HDC memDC = CreateCompatibleDC(nullptr);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hColorBmp);

    // Draw the ring(s) first, then the original cursor on top, so the
    // cursor's own glyph stays in front of the ring wherever they overlap.
    const float cx = (float)ii.xHotspot + margin;
    const float cy = (float)ii.yHotspot + margin;
    BlendRing((BYTE*)pvBits, newW, newH, cx, cy, Settings::Outer);
    BlendRing((BYTE*)pvBits, newW, newH, cx, cy, Settings::Inner);

    DrawIconEx(memDC, margin, margin, aOriginal, origW, origH, 0, nullptr, DI_NORMAL);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);

    // All-zero AND mask: the color bitmap's real alpha channel drives
    // transparency on modern Windows, the mask is only legacy compat.
    HBITMAP hMaskBmp = CreateBitmap(newW, newH, 1, 1, nullptr);
    HDC maskDC = CreateCompatibleDC(nullptr);
    HBITMAP oldMask = (HBITMAP)SelectObject(maskDC, hMaskBmp);
    PatBlt(maskDC, 0, 0, newW, newH, BLACKNESS);
    SelectObject(maskDC, oldMask);
    DeleteDC(maskDC);

    ICONINFO newIcon = {};
    newIcon.fIcon    = FALSE;
    newIcon.xHotspot = (DWORD)(ii.xHotspot + margin);
    newIcon.yHotspot = (DWORD)(ii.yHotspot + margin);
    newIcon.hbmMask  = hMaskBmp;
    newIcon.hbmColor = hColorBmp;

    HCURSOR composite = CreateIconIndirect(&newIcon);

    DeleteObject(hMaskBmp);
    DeleteObject(hColorBmp);
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    return composite ? composite : aOriginal;
}
