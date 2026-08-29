#include "ScriptsPanel.h"

#include "core/EditorApplication.h"
#include "widgets/EditorToolbar.h"

#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/ZenScriptComponent.h>

#include <IconsMaterialDesignIcons.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace k2d::editor
{

ScriptsPanel::ScriptsPanel(EditorApplication &application) : EditorPanel("Scripts", application, false)
{
}

void ScriptsPanel::onHostEvent(const char *name, double value, void *user)
{
    ScriptsPanel &panel = *static_cast<ScriptsPanel *>(user);
    if (!panel.mEvents.empty() && panel.mEvents.back().name == name &&
        panel.mEvents.back().value == value)
    {
        ++panel.mEvents.back().count;
        return;
    }

    EventLogEntry entry;
    entry.name = name;
    entry.value = value;
    panel.mEvents.push_back(entry);
    while (panel.mEvents.size() > 100)
        panel.mEvents.erase(panel.mEvents.begin());
}

void ScriptsPanel::drawScriptList(GameObject &object, int depth)
{
    const size_t count = object.componentCount<ZenScriptComponent>();
    for (size_t i = 0; i < count; ++i)
    {
        ZenScriptComponent *script = object.getComponentAt<ZenScriptComponent>(i);
        if (!script)
            continue;

        ImGui::PushID(script);
        ImGui::Indent(depth * 12.0f);
        const bool ok = script->loaded();
        ImGui::TextColored(ok ? ImVec4(0.4f, 0.85f, 0.4f, 1.0f) : ImVec4(1.0f, 0.45f, 0.3f, 1.0f),
                           "%s", ok ? ICON_MDI_CHECK_CIRCLE : ICON_MDI_ALERT_CIRCLE);
        ImGui::SameLine();
        if (ImGui::Selectable(object.name().c_str(), false))
            app().selection().select(&object);
        ImGui::SameLine();
        const ct::String &path = script->scriptPath();
        char tuned[32] = "";
        if (script->overrideCount() > 0)
            std::snprintf(tuned, sizeof(tuned), "  [%d tuned]", (int)script->overrideCount());
        ImGui::TextDisabled("%s%s%s", path.empty() ? "(inline)" : path.c_str(),
                            script->hasFunction("on_event") ? "  [on_event]" : "", tuned);
        ImGui::Unindent(depth * 12.0f);
        ImGui::PopID();
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        drawScriptList(*object.child(i), depth + 1);
}

void ScriptsPanel::drawBlackboard()
{
    const size_t count = ZenBlackboard::keyCount();
    if (count == 0)
    {
        ImGui::TextDisabled("Blackboard is empty.");
    }
    else if (ImGui::BeginTable("blackboard", 3,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Key");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableHeadersRow();

        ct::String pendingRemove;
        for (size_t i = 0; i < count; ++i)
        {
            const ct::String key = ZenBlackboard::keyAt(i);
            if (key.empty())
                continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key.c_str());
            ImGui::TableNextColumn();
            switch (ZenBlackboard::kindOf(key.c_str()))
            {
            case ZenBlackboard::Kind::String:
                ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.5f, 1.0f), "\"%s\"",
                                   ZenBlackboard::getString(key.c_str()).c_str());
                break;
            case ZenBlackboard::Kind::Bool:
                ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.95f, 1.0f), "%s",
                                   ZenBlackboard::getBool(key.c_str()) ? "True" : "False");
                break;
            default:
                ImGui::Text("%g", ZenBlackboard::getNumber(key.c_str()));
                break;
            }
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(ICON_MDI_CLOSE))
                pendingRemove = key;
            ImGui::PopID();
        }
        ImGui::EndTable();

        if (!pendingRemove.empty())
            ZenBlackboard::remove(pendingRemove.c_str());
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputTextWithHint("##key", "key", mKeyBuffer, sizeof(mKeyBuffer));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputTextWithHint("##value", "value", mValueBuffer, sizeof(mValueBuffer));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    static const char *kinds[] = {"Number", "String", "Bool"};
    ImGui::Combo("##kind", &mValueKind, kinds, 3);
    ImGui::SameLine();
    ImGui::BeginDisabled(mKeyBuffer[0] == '\0');
    if (ImGui::Button("Set"))
    {
        if (mValueKind == 1)
            ZenBlackboard::setString(mKeyBuffer, mValueBuffer);
        else if (mValueKind == 2)
            ZenBlackboard::setBool(mKeyBuffer, std::strcmp(mValueBuffer, "0") != 0 &&
                                                   std::strcmp(mValueBuffer, "False") != 0 &&
                                                   mValueBuffer[0] != '\0');
        else
            ZenBlackboard::setNumber(mKeyBuffer, std::atof(mValueBuffer));
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Write into the blackboard - scripts read it with get_number/get_string/get_flag");
}

void ScriptsPanel::drawEventLog()
{
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##event", "event name", mKeyBuffer, sizeof(mKeyBuffer));
    ImGui::SameLine();
    ImGui::BeginDisabled(mKeyBuffer[0] == '\0' || !app().playing());
    if (ImGui::Button(ICON_MDI_SEND " Emit"))
        ZenBlackboard::emit(mKeyBuffer, std::atof(mValueBuffer));
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(app().playing() ? "Send this event to every script's on_event"
                                          : "Only while playing");
    ImGui::SameLine();
    if (ImGui::Button("Clear log"))
        mEvents.clear();

    if (mEvents.empty())
    {
        ImGui::TextDisabled("No events yet.");
        return;
    }

    if (ImGui::BeginChild("eventlog", ImVec2(0.0f, 0.0f), true))
    {
        for (size_t i = mEvents.size(); i > 0; --i)
        {
            const EventLogEntry &entry = mEvents[i - 1];
            if (entry.count > 1)
                ImGui::Text("%s (%g)  x%d", entry.name.c_str(), entry.value, entry.count);
            else
                ImGui::Text("%s (%g)", entry.name.c_str(), entry.value);
        }
    }
    ImGui::EndChild();
}

void ScriptsPanel::drawContents()
{
    if (!mHandlerInstalled)
    {
        ZenBlackboard::setHostHandler(&onHostEvent, this);
        mHandlerInstalled = true;
    }

    ImGui::TextDisabled("%s", app().playing() ? "Playing - scripts are running"
                                              : "Stopped - scripts idle until Play");
    ImGui::Separator();

    if (!ImGui::BeginTabBar("scriptsTabs"))
        return;

    if (ImGui::BeginTabItem("Blackboard"))
    {
        drawBlackboard();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Events"))
    {
        drawEventLog();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Scripts"))
    {
        Scene &scene = app().playing() ? app().runtimeScene() : app().scene();
        drawScriptList(scene.root(), 0);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

}
