#include "EditorToasts.h"

#include <IconsMaterialDesignIcons.h>
#include <imgui.h>

#include <cstdio>

namespace k2d::editor
{

namespace
{
constexpr float kLifetimeSeconds = 4.0f;
constexpr float kFadeSeconds = 0.4f;
constexpr size_t kMaxVisible = 6;
constexpr float kBottomMargin = 40.0f;

ImVec4 colorFor(ToastKind kind)
{
    switch (kind)
    {
    case ToastKind::Success: return ImVec4(0.35f, 0.80f, 0.45f, 1.0f);
    case ToastKind::Warning: return ImVec4(1.00f, 0.75f, 0.25f, 1.0f);
    case ToastKind::Error: return ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
    case ToastKind::Info: break;
    }
    return ImVec4(0.70f, 0.80f, 0.95f, 1.0f);
}

const char *iconFor(ToastKind kind)
{
    switch (kind)
    {
    case ToastKind::Success: return ICON_MDI_CHECK_CIRCLE;
    case ToastKind::Warning: return ICON_MDI_ALERT;
    case ToastKind::Error: return ICON_MDI_CLOSE_CIRCLE;
    case ToastKind::Info: break;
    }
    return ICON_MDI_INFORMATION;
}
}

void EditorToasts::push(ToastKind kind, const ct::String &message)
{
    if (message.empty())
        return;

    for (size_t i = 0; i < mToasts.size(); ++i)
    {
        if (mToasts[i].kind == kind && mToasts[i].message == message)
        {
            mToasts[i].remaining = kLifetimeSeconds;
            return;
        }
    }

    Toast toast;
    toast.kind = kind;
    toast.message = message;
    toast.remaining = kLifetimeSeconds;
    toast.total = kLifetimeSeconds;
    mToasts.push_back(toast);
    if (mToasts.size() > kMaxVisible)
        mToasts.erase(mToasts.begin());
}

void EditorToasts::info(const ct::String &message) { push(ToastKind::Info, message); }
void EditorToasts::success(const ct::String &message) { push(ToastKind::Success, message); }
void EditorToasts::warning(const ct::String &message) { push(ToastKind::Warning, message); }
void EditorToasts::error(const ct::String &message) { push(ToastKind::Error, message); }

void EditorToasts::update(float deltaTime)
{
    for (size_t i = mToasts.size(); i > 0; --i)
    {
        Toast &toast = mToasts[i - 1];
        toast.remaining -= deltaTime;
        if (toast.remaining <= 0.0f)
            mToasts.erase(mToasts.begin() + static_cast<long>(i - 1));
    }
}

void EditorToasts::clear()
{
    mToasts.clear();
}

void EditorToasts::draw()
{
    if (mToasts.empty())
        return;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float margin = 12.0f;
    float y = viewport->WorkPos.y + viewport->WorkSize.y - kBottomMargin;

    for (size_t i = mToasts.size(); i > 0; --i)
    {
        const Toast &toast = mToasts[i - 1];
        float alpha = toast.remaining / kFadeSeconds;
        if (alpha > 1.0f)
            alpha = 1.0f;

        ImGui::SetNextWindowBgAlpha(0.85f * alpha);
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, y),
                                ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        char name[32];
        std::snprintf(name, sizeof(name), "##toast%zu", i);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking;
        if (ImGui::Begin(name, nullptr, flags))
        {
            ImGui::TextColored(colorFor(toast.kind), "%s", iconFor(toast.kind));
            ImGui::SameLine();
            ImGui::TextUnformatted(toast.message.c_str());
            y -= ImGui::GetWindowSize().y + 6.0f;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

}
