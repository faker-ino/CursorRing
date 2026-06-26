#include "cursor_hook.h"
#include "cursor_compose.h"
#include <Windows.h>
#include <unordered_map>
#include <deque>
#include <mutex>

namespace
{
    AddonAPI* g_api = nullptr;

    using SETCURSOR_FN        = HCURSOR(WINAPI*)(HCURSOR);
    using SETCLASSLONGPTR_FN  = ULONG_PTR(WINAPI*)(HWND, int, LONG_PTR);

    SETCURSOR_FN       fpSetCursor        = nullptr;
    SETCLASSLONGPTR_FN fpSetClassLongPtrA = nullptr;
    SETCLASSLONGPTR_FN fpSetClassLongPtrW = nullptr;

    bool g_hookedSetCursor        = false;
    bool g_hookedSetClassLongPtrA = false;
    bool g_hookedSetClassLongPtrW = false;

    // Guards every map below. The game can call SetCursor from a different
    // thread than the one CursorHook::InvalidateCache() runs on (driven by
    // the render/options UI), so the cache can't be touched lock-free.
    std::mutex g_cacheMutex;

    // original (game) HCURSOR -> composite (ring-baked) HCURSOR
    std::unordered_map<HCURSOR, HCURSOR> g_origToComposite;
    // composite HCURSOR -> original (game) HCURSOR, so SetCursor's return
    // value (the previously active cursor) can be translated back to what
    // the game actually expects to see.
    std::unordered_map<HCURSOR, HCURSOR> g_compositeToOrig;
    // Insertion order of g_origToComposite, oldest first, so the cache can
    // be capped (see kMaxCachedComposites below).
    std::deque<HCURSOR> g_insertOrder;

    // Every cached composite holds a real GDI cursor object alive until
    // explicitly destroyed, and entries are otherwise never evicted. A game
    // that hands out fresh HCURSORs over time (resolution/fullscreen
    // changes, device resets, per-session cursor reloads) would otherwise
    // leak one GDI object per occurrence for the lifetime of the process.
    // Windows caps a process at 10000 GDI objects by default; once that's
    // exhausted CreateIconIndirect starts failing and the ring silently
    // stops appearing for good. Capping the cache means the worst case is
    // an evicted-but-still-active cursor getting rebuilt on next use, which
    // is cheap, instead of the ring vanishing permanently.
    constexpr size_t kMaxCachedComposites = 128;

    bool IsOurComposite(HCURSOR aCursor)
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        return aCursor && g_compositeToOrig.find(aCursor) != g_compositeToOrig.end();
    }

    HCURSOR GetOrBuildComposite(HCURSOR aOriginal)
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);

        auto it = g_origToComposite.find(aOriginal);
        if (it != g_origToComposite.end()) return it->second;

        HCURSOR composite = CursorCompose::BuildComposite(aOriginal);
        if (composite != aOriginal)
        {
            g_origToComposite[aOriginal] = composite;
            g_compositeToOrig[composite] = aOriginal;
            g_insertOrder.push_back(aOriginal);

            if (g_insertOrder.size() > kMaxCachedComposites)
            {
                HCURSOR oldest = g_insertOrder.front();
                g_insertOrder.pop_front();

                auto oldIt = g_origToComposite.find(oldest);
                if (oldIt != g_origToComposite.end())
                {
                    DestroyIcon(oldIt->second);
                    g_compositeToOrig.erase(oldIt->second);
                    g_origToComposite.erase(oldIt);
                }
            }
        }
        return composite;
    }

    // Translates a cursor handle that may be one of our composites back to
    // the original game cursor it stands in for; passes through unknown
    // handles untouched.
    HCURSOR ToLogical(HCURSOR aCursor)
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_compositeToOrig.find(aCursor);
        return it == g_compositeToOrig.end() ? aCursor : it->second;
    }

    HCURSOR WINAPI Detour_SetCursor(HCURSOR hCursor)
    {
        if (!hCursor || IsOurComposite(hCursor))
            return fpSetCursor(hCursor);

        HCURSOR prevReal = fpSetCursor(GetOrBuildComposite(hCursor));
        return ToLogical(prevReal);
    }

    ULONG_PTR WINAPI Detour_SetClassLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
    {
        if (nIndex == GCLP_HCURSOR)
        {
            HCURSOR hCursor = (HCURSOR)dwNewLong;
            if (hCursor && !IsOurComposite(hCursor))
                dwNewLong = (LONG_PTR)GetOrBuildComposite(hCursor);
        }
        ULONG_PTR prev = fpSetClassLongPtrA(hWnd, nIndex, dwNewLong);
        return nIndex == GCLP_HCURSOR ? (ULONG_PTR)ToLogical((HCURSOR)prev) : prev;
    }

    ULONG_PTR WINAPI Detour_SetClassLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
    {
        if (nIndex == GCLP_HCURSOR)
        {
            HCURSOR hCursor = (HCURSOR)dwNewLong;
            if (hCursor && !IsOurComposite(hCursor))
                dwNewLong = (LONG_PTR)GetOrBuildComposite(hCursor);
        }
        ULONG_PTR prev = fpSetClassLongPtrW(hWnd, nIndex, dwNewLong);
        return nIndex == GCLP_HCURSOR ? (ULONG_PTR)ToLogical((HCURSOR)prev) : prev;
    }

    void DestroyCachedComposites()
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        for (auto& [original, composite] : g_origToComposite)
        {
            if (composite != original) DestroyIcon(composite);
        }
        g_origToComposite.clear();
        g_compositeToOrig.clear();
        g_insertOrder.clear();
    }
}

void CursorHook::Install(AddonAPI* aApi)
{
    g_api = aApi;

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) return;

    void* pSetCursor        = (void*)GetProcAddress(user32, "SetCursor");
    void* pSetClassLongPtrA = (void*)GetProcAddress(user32, "SetClassLongPtrA");
    void* pSetClassLongPtrW = (void*)GetProcAddress(user32, "SetClassLongPtrW");

    if (pSetCursor &&
        g_api->MinHook.Create(pSetCursor, &Detour_SetCursor, (LPVOID*)&fpSetCursor) == MH_OK &&
        g_api->MinHook.Enable(pSetCursor) == MH_OK)
    {
        g_hookedSetCursor = true;
    }

    if (pSetClassLongPtrA &&
        g_api->MinHook.Create(pSetClassLongPtrA, &Detour_SetClassLongPtrA, (LPVOID*)&fpSetClassLongPtrA) == MH_OK &&
        g_api->MinHook.Enable(pSetClassLongPtrA) == MH_OK)
    {
        g_hookedSetClassLongPtrA = true;
    }

    if (pSetClassLongPtrW &&
        g_api->MinHook.Create(pSetClassLongPtrW, &Detour_SetClassLongPtrW, (LPVOID*)&fpSetClassLongPtrW) == MH_OK &&
        g_api->MinHook.Enable(pSetClassLongPtrW) == MH_OK)
    {
        g_hookedSetClassLongPtrW = true;
    }
}

void CursorHook::Uninstall()
{
    if (!g_api) return;

    // If the cursor currently on screen is one of our composites, restore
    // the real one first - we're about to DestroyIcon every composite.
    if (g_hookedSetCursor)
    {
        HCURSOR current = GetCursor();
        if (IsOurComposite(current))
            fpSetCursor(ToLogical(current));
    }

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32)
    {
        if (g_hookedSetCursor)
        {
            void* p = (void*)GetProcAddress(user32, "SetCursor");
            g_api->MinHook.Disable(p);
            g_api->MinHook.Remove(p);
        }
        if (g_hookedSetClassLongPtrA)
        {
            void* p = (void*)GetProcAddress(user32, "SetClassLongPtrA");
            g_api->MinHook.Disable(p);
            g_api->MinHook.Remove(p);
        }
        if (g_hookedSetClassLongPtrW)
        {
            void* p = (void*)GetProcAddress(user32, "SetClassLongPtrW");
            g_api->MinHook.Disable(p);
            g_api->MinHook.Remove(p);
        }
    }

    g_hookedSetCursor = g_hookedSetClassLongPtrA = g_hookedSetClassLongPtrW = false;
    DestroyCachedComposites();
    g_api = nullptr;
}

void CursorHook::InvalidateCache()
{
    DestroyCachedComposites();
}
