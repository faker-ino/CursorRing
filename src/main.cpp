/*
 * CursorRing - GW2 Nexus Addon
 * Bakes a customizable ring into the real cursor.
 */

#include <Windows.h>
#include "nexus/Nexus.h"   // Drop in from Nexus SDK
#include <imgui/imgui.h>
#include "options.h"
#include "cursor_hook.h"
#include "settings.h"

// ── Nexus boilerplate ──────────────────────────────────────────────────────

AddonDefinition AddonDef = {};
HMODULE hSelf             = nullptr;
AddonAPI* APIDefs         = nullptr;

// Forward declarations
void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void AddonOptions();

// Called by Nexus to get addon metadata
extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
    AddonDef.Signature     = 0x43525247; // "CRRG" – pick any unique int
    AddonDef.APIVersion    = NEXUS_API_VERSION;
    AddonDef.Name          = "CursorRing";
    AddonDef.Version.Major = 1;
    AddonDef.Version.Minor = 0;
    AddonDef.Version.Build = 0;
    AddonDef.Author        = "Firu";
    AddonDef.Description   = "Draws a customizable ring around your cursor.";
    AddonDef.Load          = AddonLoad;
    AddonDef.Unload        = AddonUnload;
    AddonDef.Flags         = EAddonFlags_IsVolatile; // hooks SetCursor/SetClassLongPtr
    AddonDef.Provider      = EUpdateProvider_None;
    return &AddonDef;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { hSelf = hModule; }
    return TRUE;
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void AddonLoad(AddonAPI* aApi)
{
    APIDefs   = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree);

    Settings::Load(APIDefs->Paths.GetAddonDirectory("CursorRing"));

    CursorHook::Install(APIDefs);

    APIDefs->Renderer.Register(ERenderType_OptionsRender, AddonOptions);
}

void AddonUnload()
{
    APIDefs->Renderer.Deregister(AddonOptions);

    CursorHook::Uninstall();

    Settings::Save(APIDefs->Paths.GetAddonDirectory("CursorRing"));
}

// ── Render callbacks ───────────────────────────────────────────────────────

void AddonOptions()
{
    Options::DrawOptions();
}
