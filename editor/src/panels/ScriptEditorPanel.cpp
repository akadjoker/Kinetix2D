#include "ScriptEditorPanel.h"

#include "core/EditorApplication.h"
#include "core/EditorFileSystem.h"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>

#include <ImGuiMinimap.h>
#include <IconsMaterialDesignIcons.h>

#include <string>

namespace k2d::editor
{

ScriptEditorPanel::ScriptEditorPanel(EditorApplication& application) : EditorPanel("Script Editor", application)
{
    mEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::ZenScript);
    mEditor.SetPalette(TextEditor::PaletteId::VsCodeDark);
    mEditor.SetAutoIndentEnabled(true);
    mEditor.SetAutoCloseBracketsEnabled(true);
    mEditor.SetShowLineNumbersEnabled(true);
    mEditor.SetShowWhitespacesEnabled(false);
    mEditor.SetShortTabsEnabled(true);
    mEditor.SetFoldingEnabled(true);
}

void ScriptEditorPanel::openFile(const char* path)
{
    if (!path || !path[0])
        return;

    if (dirty() && mPath != path)
    {
        mPendingPath = path;
        mConfirmOpen = true;
        open() = true;
        return;
    }
    loadFile(path);
}

bool ScriptEditorPanel::loadFile(const char* path)
{
    FileBuffer file;
    if (!FileSystem::Instance().LoadFile(path, file, false))
    {
        ct::String message("Could not open script: ");
        message += path;
        app().log(message);
        app().toasts().error(message);
        return false;
    }

    mEditor.SetText(std::string(file.Text(), file.Size()));
    mPath = path;
    mSavedUndoIndex = mEditor.GetUndoIndex();
    open() = true;

    ct::String message("Opened script: ");
    message += mPath;
    app().log(message);
    return true;
}

bool ScriptEditorPanel::saveFile()
{
    if (mPath.empty())
        return false;

    const std::string source = mEditor.GetText();
    if (!FileSystem::Instance().SaveFile(mPath.c_str(), source.data(), source.size()))
    {
        ct::String message("Could not save script: ");
        message += mPath;
        app().log(message);
        app().toasts().error(message);
        return false;
    }

    mSavedUndoIndex = mEditor.GetUndoIndex();
    ct::String message("Saved script: ");
    message += mPath;
    app().log(message);
    app().toasts().success("Script saved");
    return true;
}

bool ScriptEditorPanel::dirty() const
{
    return mEditor.GetUndoIndex() != mSavedUndoIndex;
}

void ScriptEditorPanel::drawContents()
{
    if (mConfirmOpen)
    {
        ImGui::OpenPopup("Discard unsaved script changes?");
        mConfirmOpen = false;
    }
    if (ImGui::BeginPopupModal("Discard unsaved script changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("The current script has unsaved changes.");
        ImGui::TextUnformatted("Open the other script and discard them?");
        if (ImGui::Button("Discard and Open"))
        {
            const ct::String path = mPendingPath;
            mPendingPath.clear();
            mSavedUndoIndex = mEditor.GetUndoIndex();
            loadFile(path.c_str());
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            mPendingPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (mPath.empty())
    {
        ImGui::TextDisabled("Open a .py file from Assets (double click) to edit it here.");
        ImGui::TextDisabled("While playing, press F6 to reload saved script changes.");
        return;
    }

    const bool hasChanges = dirty();
    ImGui::BeginDisabled(!hasChanges);
    if (ImGui::Button(ICON_MDI_CONTENT_SAVE))
        saveFile();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(hasChanges ? "Save script (Ctrl+S)" : "No changes to save");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_RELOAD))
    {
        if (hasChanges)
        {
            mPendingPath = mPath;
            mConfirmOpen = true;
        }
        else
        {
            loadFile(mPath.c_str());
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reloads the saved file from disk");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_CONTENT_COPY))
        mEditor.Copy();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Copy (Ctrl+C)");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_CONTENT_PASTE))
        mEditor.Paste();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Paste (Ctrl+V)");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FORMAT_INDENT_INCREASE))
        mEditor.Indent();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Indent selection (Ctrl+])");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FORMAT_INDENT_DECREASE))
        mEditor.Unindent();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Unindent selection (Ctrl+[)");
    ImGui::SameLine();
    const bool showIndentation = mEditor.IsShowWhitespacesEnabled();
    if (showIndentation)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(ICON_MDI_FORMAT_ALIGN_LEFT))
        mEditor.SetShowWhitespacesEnabled(!showIndentation);
    if (showIndentation)
        ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show indentation: dots for spaces and arrows for tabs");
    ImGui::SameLine();
    const bool minimapWasVisible = mShowMinimap;
    if (minimapWasVisible)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(ICON_MDI_MAP))
        mShowMinimap = !mShowMinimap;
    if (minimapWasVisible)
        ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(mShowMinimap ? "Hide minimap" : "Show minimap");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_MAGNIFY_MINUS))
        mEditor.SetFontScale(mEditor.GetFontScale() - 0.1f);
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::Text("%.0f%%", mEditor.GetFontScale() * 100.0f);
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::Button(ICON_MDI_MAGNIFY_PLUS))
        mEditor.SetFontScale(mEditor.GetFontScale() + 0.1f);
    ImGui::Separator();

    const float footerHeight = ImGui::GetTextLineHeightWithSpacing();
    const float minimapWidth = 110.0f;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 editorSize(available.x - (mShowMinimap ? minimapWidth + ImGui::GetStyle().ItemSpacing.x : 0.0f),
                            available.y - footerHeight);
    const bool focused = mEditor.Render("##zen_script_editor", false, editorSize, true);
    const bool hovered = ImGui::IsItemHovered();
    if (mShowMinimap)
    {
        ImGui::SameLine();
        ImGuiMinimap::Render("##zen_script_minimap", mEditor, minimapWidth, editorSize.y, ImGui::GetFont());
    }

    if (focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        saveFile();
    if (hovered && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
        mEditor.SetFontScale(mEditor.GetFontScale() + ImGui::GetIO().MouseWheel * 0.1f);

    const TextEditor::TextPosition cursor = mEditor.GetCursorPosition();
    const std::string source = mEditor.GetText();
    const ct::String fileName = EditorFileSystem::fileName(mPath);
    ImGui::TextDisabled("Ln %d, Col %d  |  %d lines  |  %d bytes  |  %s%s", cursor.line + 1, cursor.column + 1,
                        mEditor.GetLineCount(), static_cast<int>(source.size()), fileName.c_str(),
                        app().playing() ? "  |  F6 reloads saved changes" : "");
}

} // namespace k2d::editor
