#pragma once
#include "nexus/Nexus.h"

// Hooks the Win32 cursor-setting APIs (SetCursor / SetClassLongPtrA/W) so
// every cursor the game installs gets CursorRing's ring(s) baked into it
// before reaching the OS. Because the substitution happens at the same
// point the game itself sets its cursor (driven by input messages, not the
// render loop), the ring's on-screen position is handled entirely by the
// OS/GPU cursor plane - it can't lag behind frame drops.
namespace CursorHook
{
    // Installs the hooks. Call once from AddonLoad after APIDefs is set.
    void Install(AddonAPI* aApi);

    // Removes the hooks, restores the real cursor and frees all cached
    // composite cursors. Call once from AddonUnload.
    void Uninstall();

    // Drops every cached composite so it gets rebuilt (lazily, on next use)
    // with the latest Settings values. Call after any ring setting changes.
    void InvalidateCache();
}
