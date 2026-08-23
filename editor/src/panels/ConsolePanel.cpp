#include "ConsolePanel.h"

#include "core/EditorApplication.h"

namespace k2d::editor
{

ConsolePanel::ConsolePanel(EditorApplication &application)
    : EditorPanel("Console", application)
{
}

void ConsolePanel::drawContents()
{
    if (ImGui::SmallButton("Clear"))
        app().clearConsole();
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy"))
        ImGui::SetClipboardText(app().consoleText().c_str());
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &mFollowOutput);
    ImGui::Separator();

    ImGui::BeginChild("##console_output", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(app().consoleText().c_str());
    if (mFollowOutput)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

}
