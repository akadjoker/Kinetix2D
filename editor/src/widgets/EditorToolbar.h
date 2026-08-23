#pragma once

#include <imgui.h>

namespace k2d::editor
{

inline bool toolbarIcon(const char *id, const char *icon, const char *tooltip,
                        bool active = false, bool enabled = true)
{
    ImGui::PushID(id);
    if (!enabled)
        ImGui::BeginDisabled();
    if (active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    const bool pressed = ImGui::Button(icon, ImVec2(30.0f, 26.0f));
    if (ImGui::IsItemHovered() && tooltip)
        ImGui::SetTooltip("%s", tooltip);

    if (active)
        ImGui::PopStyleColor();
    if (!enabled)
        ImGui::EndDisabled();
    ImGui::PopID();
    return pressed;
}

inline void toolbarSameLine()
{
    ImGui::SameLine(0.0f, 3.0f);
}

inline void toolbarDivider()
{
    toolbarSameLine();
    ImGui::TextDisabled("|");
    toolbarSameLine();
}

}
