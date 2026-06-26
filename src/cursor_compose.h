#pragma once
#ifndef NOMINMAX
#define NOMINMAX // avoid windows.h's min/max macros clashing with std::min/max
#endif
#include <Windows.h>

// Builds a copy of a Win32 cursor with CursorRing's configured ring(s) baked
// directly into its bitmap. Compositing is real Win32/GDI, so the result is
// a genuine HCURSOR the OS can render on the hardware cursor plane - no
// per-frame draw call involved, so it can never lag behind the game's
// render rate.
namespace CursorCompose
{
    // Returns a new HCURSOR with the current Settings::Outer/Inner rings
    // drawn around aOriginal's hotspot, or aOriginal itself (unmodified) if
    // both rings are disabled or compositing fails. Caller owns the
    // returned handle when it differs from aOriginal and must DestroyIcon it.
    HCURSOR BuildComposite(HCURSOR aOriginal);
}
