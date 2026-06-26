#pragma once
#include <string>

namespace Settings
{
    // ── Per-ring configurable values ───────────────────────────────────────
    struct RingSettings
    {
        bool   Enabled       = true;
        float  Radius        = 24.0f;   // pixels from cursor centre to ring centre
        float  Thickness     = 2.5f;    // stroke width in pixels
        int    Segments      = 64;      // smoothness (16–128)
        float  Color[4]      = { 1.0f, 1.0f, 1.0f, 0.85f }; // RGBA

        bool   PulseEnabled  = false;
        float  PulseSpeed    = 1.5f;    // Hz
        float  PulseAmount   = 4.0f;    // ±pixels added to radius
    };

    inline RingSettings Outer;
    inline RingSettings Inner = [] { RingSettings r; r.Enabled = false; r.Radius = 12.0f; return r; }();

    // ── Global / shared values ──────────────────────────────────────────────
    inline bool   ShowInCombat  = true;
    inline bool   ShowOutOfCombat = true;
    inline bool   OnlyInGame    = true;    // hide on character select / loading
    inline bool   HideWhileLeftMouseDragging = false;
    inline bool   HideWhileRightMouseDragging = false;

    // ── Persistence ───────────────────────────────────────────────────────
    void Load(const char* addonDir);
    void Save(const char* addonDir);
}
