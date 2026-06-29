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
    // Alpha-blends one straight-alpha RGBA colour into one straight-alpha
    // BGRA pixel, scaled by coverage (0..1, e.g. from AA edge falloff).
    void BlendPixel(BYTE* px, float coverage, const float* color)
    {
        float a = coverage * color[3];
        if (a <= 0.0f) return;

        float dstB = px[0] / 255.0f, dstG = px[1] / 255.0f, dstR = px[2] / 255.0f, dstA = px[3] / 255.0f;

        float outA = a + dstA * (1.0f - a);
        float outR = 0.0f, outG = 0.0f, outB = 0.0f;
        if (outA > 0.0001f)
        {
            outR = (color[0] * a + dstR * dstA * (1.0f - a)) / outA;
            outG = (color[1] * a + dstG * dstA * (1.0f - a)) / outA;
            outB = (color[2] * a + dstB * dstA * (1.0f - a)) / outA;
        }

        px[0] = (BYTE)(std::clamp(outB, 0.0f, 1.0f) * 255.0f + 0.5f);
        px[1] = (BYTE)(std::clamp(outG, 0.0f, 1.0f) * 255.0f + 0.5f);
        px[2] = (BYTE)(std::clamp(outR, 0.0f, 1.0f) * 255.0f + 0.5f);
        px[3] = (BYTE)(std::clamp(outA, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    // Walks the square bounding box of radius aExtent around (cx, cy),
    // calling aCoverageAt(dx, dy) per pixel and blending aColor in where it
    // returns > 0. Shared by the ring (stroke) and dot (filled disc) shapes
    // below - only the coverage formula differs between them.
    template <typename CoverageFn>
    void BlendShape(BYTE* pixels, int width, int height, float cx, float cy,
                     float extent, const float* color, CoverageFn coverageAt)
    {
        int minX = std::max((int)std::floor(cx - extent), 0);
        int minY = std::max((int)std::floor(cy - extent), 0);
        int maxX = std::min((int)std::ceil (cx + extent), width - 1);
        int maxY = std::min((int)std::ceil (cy + extent), height - 1);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                float coverage = coverageAt((x + 0.5f) - cx, (y + 0.5f) - cy);
                if (coverage > 0.0f)
                    BlendPixel(pixels + (size_t)(y * width + x) * 4, coverage, color);
            }
        }
    }

    // AA feather width on each side of a stroke/edge.
    constexpr float kAAPad = 1.0f;

    void BlendRing(BYTE* pixels, int width, int height, float cx, float cy,
                    const Settings::RingSettings& ring)
    {
        if (!ring.Enabled || ring.Color[3] <= 0.0f) return;

        const float radius = ring.Radius;
        const float halfThickness = ring.Thickness * 0.5f;
        const float edge0 = halfThickness - 0.5f;
        const float edge1 = halfThickness + 0.5f;

        BlendShape(pixels, width, height, cx, cy, radius + halfThickness + kAAPad, ring.Color,
            [&](float dx, float dy)
            {
                float distFromStroke = std::fabs(std::sqrt(dx * dx + dy * dy) - radius);
                if (distFromStroke <= edge0) return 1.0f;
                if (distFromStroke >= edge1) return 0.0f;
                return (edge1 - distFromStroke) / (edge1 - edge0);
            });
    }

    void BlendDot(BYTE* pixels, int width, int height, float cx, float cy,
                   const Settings::DotSettings& dot)
    {
        if (!dot.Enabled || dot.Color[3] <= 0.0f) return;

        const float edge0 = dot.Size - 0.5f;
        const float edge1 = dot.Size + 0.5f;

        BlendShape(pixels, width, height, cx, cy, dot.Size + kAAPad, dot.Color,
            [&](float dx, float dy)
            {
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= edge0) return 1.0f;
                if (dist >= edge1) return 0.0f;
                return (edge1 - dist) / (edge1 - edge0);
            });
    }
}

HCURSOR CursorCompose::BuildComposite(HCURSOR aOriginal)
{
    if (!aOriginal) return aOriginal;
    if (!Settings::Outer.Enabled && !Settings::Inner.Enabled && !Settings::Dot.Enabled) return aOriginal;

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
    if (Settings::Dot.Enabled)
        maxExtent = std::max(maxExtent, Settings::Dot.Size);

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

    // Draw the ring(s) and centre dot first, then the original cursor on
    // top, so the cursor's own glyph stays in front wherever they overlap.
    const float cx = (float)ii.xHotspot + margin;
    const float cy = (float)ii.yHotspot + margin;
    BlendRing((BYTE*)pvBits, newW, newH, cx, cy, Settings::Outer);
    BlendRing((BYTE*)pvBits, newW, newH, cx, cy, Settings::Inner);
    BlendDot ((BYTE*)pvBits, newW, newH, cx, cy, Settings::Dot);

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
