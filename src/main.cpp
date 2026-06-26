/*
 * CursorRing - GW2 Nexus Addon
 * Draws a customizable ring around the mouse cursor using ImGui.
 */

#include <Windows.h>
#include "nexus/Nexus.h"   // Drop in from Nexus SDK
#include "mumble/Mumble.h"
#include <imgui/imgui.h>
#include "overlay.h"
#include "settings.h"

// ── Nexus boilerplate ──────────────────────────────────────────────────────

AddonDefinition AddonDef = {};
HMODULE hSelf             = nullptr;
AddonAPI* APIDefs         = nullptr;
NexusLinkData* NexusLink  = nullptr;
Mumble::Data* MumbleLink  = nullptr;

// Forward declarations
void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void AddonRender();
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
    AddonDef.Flags         = EAddonFlags_None;
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

    // Grab shared data pointers
    NexusLink  = (NexusLinkData*)APIDefs->DataLink.Get("DL_NEXUS_LINK");
    MumbleLink = (Mumble::Data*)APIDefs->DataLink.Get("DL_MUMBLE_LINK");

    Settings::Load(APIDefs->Paths.GetAddonDirectory("CursorRing"));

    // Register our render and options callbacks
    APIDefs->Renderer.Register(ERenderType_Render, AddonRender);
    APIDefs->Renderer.Register(ERenderType_OptionsRender, AddonOptions);
}

void AddonUnload()
{
    APIDefs->Renderer.Deregister(AddonRender);
    APIDefs->Renderer.Deregister(AddonOptions);

    Settings::Save(APIDefs->Paths.GetAddonDirectory("CursorRing"));
}

// ── Render callbacks ───────────────────────────────────────────────────────

void AddonRender()
{
    Overlay::Draw();
}

void AddonOptions()
{
    Overlay::DrawOptions();
}
