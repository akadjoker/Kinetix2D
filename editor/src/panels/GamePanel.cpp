#include "GamePanel.h"

#include "core/EditorApplication.h"

namespace k2d::editor
{

void GamePanel::drawContents()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        position, ImVec2(position.x + available.x, position.y + available.y),
        IM_COL32(12, 14, 18, 255));

    const char *message = app().playing()
        ? "Runtime scene rendering will be connected in M5"
        : "Press Play to create an isolated runtime scene";
    const ImVec2 textSize = ImGui::CalcTextSize(message);
    ImGui::SetCursorScreenPos(ImVec2(position.x + (available.x - textSize.x) * 0.5f,
                                    position.y + (available.y - textSize.y) * 0.5f));
    ImGui::TextDisabled("%s", message);
}

}
