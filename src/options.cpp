#include "options.h"
#include "settings.h"
#include "cursor_hook.h"
#include <imgui/imgui.h>

static void DrawRingOptions(const char* title, Settings::RingSettings& ring, ImGuiTreeNodeFlags flags)
{
    ImGui::PushID(title);
    if (ImGui::CollapsingHeader(title, flags))
    {
        bool changed = false;

        changed |= ImGui::Checkbox("Enabled", &ring.Enabled);
        if (ring.Enabled)
        {
            changed |= ImGui::SliderFloat("Radius",    &ring.Radius,    4.0f,  128.0f, "%.0f px");
            changed |= ImGui::SliderFloat("Thickness", &ring.Thickness, 0.5f,   16.0f, "%.1f px");

            ImGui::Spacing();
            changed |= ImGui::ColorEdit4("Ring Color", ring.Color,
                ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
        }

        if (changed) CursorHook::InvalidateCache();
    }
    ImGui::PopID();
}

void Options::DrawOptions()
{
    ImGui::TextDisabled("Cursor Ring");
    ImGui::Separator();

    DrawRingOptions("Outer Ring", Settings::Outer, ImGuiTreeNodeFlags_DefaultOpen);
    DrawRingOptions("Inner Ring", Settings::Inner, ImGuiTreeNodeFlags_None);

    ImGui::Spacing();
    ImGui::TextDisabled("The ring is baked into the real cursor, so it tracks");
    ImGui::TextDisabled("the mouse smoothly even when the game's FPS drops.");
}
