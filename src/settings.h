#pragma once
#include <string>

namespace Settings
{
    // ── Per-ring configurable values ───────────────────────────────────────
    struct RingSettings
    {
        bool   Enabled       = true;
        float  Radius        = 24.0f;   // pixels from cursor hotspot to ring centre
        float  Thickness     = 2.5f;    // stroke width in pixels
        float  Color[4]      = { 1.0f, 1.0f, 1.0f, 0.85f }; // RGBA
    };

    inline RingSettings Outer;
    inline RingSettings Inner = [] { RingSettings r; r.Enabled = false; r.Radius = 12.0f; return r; }();

    // ── Persistence ───────────────────────────────────────────────────────
    void Load(const char* addonDir);
    void Save(const char* addonDir);
}
