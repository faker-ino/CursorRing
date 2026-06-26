#include "overlay.h"
#include "settings.h"
#include "nexus/Nexus.h"    // for NexusLink / MumbleLink access
#include "mumble/Mumble.h"
#include <Windows.h>
#include <imgui/imgui.h>
#include <cmath>

// Declared in main.cpp
extern NexusLinkData*  NexusLink;
extern Mumble::Data*   MumbleLink;

// ── Helpers ────────────────────────────────────────────────────────────────

static void DrawRing(ImDrawList* dl, ImVec2 cursor, const Settings::RingSettings& ring)
{
    if (!ring.Enabled) return;

    float radius = ring.Radius;
    if (ring.PulseEnabled)
    {
        constexpr float kTwoPi = 6.28318530717958647692f;
        float t = (float)ImGui::GetTime();
        radius += std::sin(t * ring.PulseSpeed * kTwoPi) * ring.PulseAmount;
    }

    ImU32 col = ImGui::ColorConvertFloat4ToU32(
        ImVec4(ring.Color[0], ring.Color[1], ring.Color[2], ring.Color[3]));
    dl->AddCircle(cursor, radius, col, ring.Segments, ring.Thickness);
}

static bool ShouldDraw()
{
    static POINT s_lastCursorPos = { 0, 0 };
    static bool s_hasLastCursorPos = false;
    static bool s_leftHiding = false;
    static bool s_rightHiding = false;

    // Older Nexus SDK revisions in this repo expose gameplay state via NexusLinkData.
    bool inGame = NexusLink ? NexusLink->IsGameplay : true;

    if (Settings::OnlyInGame && !inGame)        return false;

    if (MumbleLink)
    {
        bool inCombat = MumbleLink->Context.IsInCombat;
        if (inCombat  && !Settings::ShowInCombat)    return false;
        if (!inCombat && !Settings::ShowOutOfCombat) return false;
    }

    if (Settings::HideWhileLeftMouseDragging || Settings::HideWhileRightMouseDragging)
    {
        // ImGui's IO mouse state isn't reliable here: Nexus only forwards
        // button events to ImGui when an ImGui window has capture, not for
        // plain game-world clicks. Query the real OS button state instead.
        const bool isLeftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool isRightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        if (!isLeftDown)  s_leftHiding = false;
        if (!isRightDown) s_rightHiding = false;

        POINT cursorPos = { 0, 0 };
        bool cursorMoved = false;
        if (GetCursorPos(&cursorPos))
        {
            if (s_hasLastCursorPos)
            {
                cursorMoved = (cursorPos.x != s_lastCursorPos.x) || (cursorPos.y != s_lastCursorPos.y);
            }
            s_lastCursorPos = cursorPos;
            s_hasLastCursorPos = true;
        }

        const bool cameraMoving = NexusLink ? NexusLink->IsCameraMoving : false;
        const bool isMoving = cursorMoved || cameraMoving;

        // Once movement is seen while held, stay hidden for the rest of the
        // hold even if the cursor/camera momentarily stops moving.
        if (isLeftDown  && isMoving) s_leftHiding = true;
        if (isRightDown && isMoving) s_rightHiding = true;

        const bool hideForLeft = Settings::HideWhileLeftMouseDragging && isLeftDown && s_leftHiding;
        const bool hideForRight = Settings::HideWhileRightMouseDragging && isRightDown && s_rightHiding;

        if (hideForLeft || hideForRight)
            return false;
    }

    return true;
}

// ── Draw ───────────────────────────────────────────────────────────────────

void Overlay::Draw()
{
    if (!ShouldDraw()) return;

    // We draw on the full-screen invisible overlay window that Nexus provides.
    // Using GetBackgroundDrawList() puts us behind all ImGui windows;
    // GetForegroundDrawList() puts us in front – foreground is what WoW addons do.
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImVec2 cursor = ImGui::GetMousePos();

    DrawRing(dl, cursor, Settings::Outer);
    DrawRing(dl, cursor, Settings::Inner);
}

// ── Options UI ─────────────────────────────────────────────────────────────

static void DrawRingOptions(const char* title, Settings::RingSettings& ring, ImGuiTreeNodeFlags flags)
{
    ImGui::PushID(title);
    if (ImGui::CollapsingHeader(title, flags))
    {
        ImGui::Checkbox("Enabled", &ring.Enabled);
        if (ring.Enabled)
        {
            ImGui::SliderFloat("Radius",    &ring.Radius,    4.0f,  128.0f, "%.0f px");
            ImGui::SliderFloat("Thickness", &ring.Thickness, 0.5f,   16.0f, "%.1f px");
            ImGui::SliderInt  ("Segments",  &ring.Segments,    8,     128);

            ImGui::Spacing();
            ImGui::ColorEdit4("Ring Color", ring.Color,
                ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);

            ImGui::Spacing();
            ImGui::Checkbox("Enable Pulse", &ring.PulseEnabled);
            if (ring.PulseEnabled)
            {
                ImGui::SliderFloat("Pulse Speed",  &ring.PulseSpeed,  0.1f, 10.0f, "%.1f Hz");
                ImGui::SliderFloat("Pulse Amount", &ring.PulseAmount, 0.5f, 32.0f, "%.1f px");
            }
        }
    }
    ImGui::PopID();
}

void Overlay::DrawOptions()
{
    ImGui::TextDisabled("Cursor Ring");
    ImGui::Separator();

    DrawRingOptions("Outer Ring", Settings::Outer, ImGuiTreeNodeFlags_DefaultOpen);
    DrawRingOptions("Inner Ring", Settings::Inner, ImGuiTreeNodeFlags_None);

    // ── Visibility ────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Visibility"))
    {
        ImGui::Checkbox("Only show in-game (hide on loading/login)", &Settings::OnlyInGame);
        ImGui::Checkbox("Hide while moving mouse with left button held", &Settings::HideWhileLeftMouseDragging);
        ImGui::Checkbox("Hide while moving mouse with right button held", &Settings::HideWhileRightMouseDragging);
        ImGui::Checkbox("Show in combat",     &Settings::ShowInCombat);
        ImGui::Checkbox("Show out of combat", &Settings::ShowOutOfCombat);
    }

    ImGui::Spacing();

    // Live preview hint
    ImGui::TextDisabled("Changes apply live – no restart needed.");
}
