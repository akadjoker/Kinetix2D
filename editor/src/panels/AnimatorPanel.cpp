#include "AnimatorPanel.h"

#include "AssetsPanel.h"
#include "core/EditorApplication.h"

#include <k2d/Animation2D.h>
#include <k2d/GameObject.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Texture.h>

#include <imgui.h>
#include <IconsMaterialDesignIcons.h>

#include <cstdio>
#include <functional>

namespace k2d::editor
{
namespace
{
void commit(EditorApplication &app, const char *label, const std::function<void()> &change)
{
    const EditorApplication::SceneChange before = app.beginChange();
    change();
    app.commitChange(label, before);
}
}

void AnimatorPanel::drawFrameEventMarker(Animation2D &animation, AnimationClip &clip, std::size_t frameIndex)
{
    const int frame = static_cast<int>(frameIndex);
    int eventIndex = -1;
    for (std::size_t i = 0; i < clip.events.size(); ++i)
        if (clip.events[i].frame == frame)
        {
            eventIndex = static_cast<int>(i);
            break;
        }

    ImGui::PushID("event");
    if (eventIndex < 0)
    {
        if (ImGui::SmallButton(ICON_MDI_PLUS))
            commit(app(), "Add Animation Event",
                   [&] { animation.addEvent(clip.name.c_str(), frame, "event"); });
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Fire a named event when playback enters this frame");
    }
    else
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s", clip.events[static_cast<std::size_t>(eventIndex)].name.c_str());
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::InputText("##eventName", buffer, sizeof(buffer)))
            commit(app(), "Rename Animation Event",
                   [&] { animation.setEvent(clip.name.c_str(), static_cast<std::size_t>(eventIndex), frame, buffer); });
        if (ImGui::SmallButton(ICON_MDI_BELL_OFF))
            commit(app(), "Remove Animation Event",
                   [&] { animation.removeEvent(clip.name.c_str(), static_cast<std::size_t>(eventIndex)); });
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove this frame's event");
    }
    ImGui::PopID();
}

void AnimatorPanel::drawContents()
{
    GameObject *owner = app().selection().resolve(app().scene());
    Animation2D *animation = owner ? owner->getComponent<Animation2D>() : nullptr;
    if (!animation)
    {
        ImGui::TextDisabled("Select a Scene object with Animation2D.");
        return;
    }
    if (!owner->getComponent<SpriteComponent>())
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f),
                           "Add a Sprite Renderer to preview this animation in Scene.");
    ImGui::TextDisabled("Drag a rect from Sprite Editor directly into a clip. No sprite-library nodes are created.");

    static char newClip[64] = "idle";
    static float newClipFps = 10.0f;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Clip", newClip, sizeof(newClip));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("FPS", &newClipFps, 0.5f, 1.0f, 240.0f);
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FILE_PLUS "  New Clip") && newClip[0])
        commit(app(), "Create Animation Clip", [&]
        {
            animation->addClip(newClip, nullptr, 0, 0, 0, newClipFps, AnimationMode::Loop);
            animation->play(newClip);
            mSelectedFrame = -1;
        });

    ImGui::BeginChild("##clip_list", ImVec2(180.0f, 0.0f), true);
    ImGui::TextUnformatted("Clips");
    const char *currentName = animation->currentClip();
    for (size_t i = 0; i < animation->clipCount(); ++i)
    {
        AnimationClip *clip = animation->clipAt(i);
        if (clip && ImGui::Selectable(clip->name.c_str(), currentName && clip->name == currentName))
        {
            animation->play(clip->name.c_str());
            mSelectedFrame = -1;
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##timeline", ImVec2(0.0f, 0.0f), true);
    AnimationClip *clip = nullptr;
    currentName = animation->currentClip();
    for (size_t i = 0; i < animation->clipCount(); ++i)
        if (animation->clipAt(i) && currentName && animation->clipAt(i)->name == currentName)
            clip = animation->clipAt(i);
    if (!clip)
    {
        ImGui::TextDisabled("Create or select a clip.");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("Timeline: %s  (drag rects into this row)", clip->name.c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.14f, 0.16f, 1.0f));
    if (ImGui::SmallButton(ICON_MDI_DELETE "  Delete Clip"))
    {
        const ct::String clipName = clip->name;
        commit(app(), "Delete Animation Clip", [&]
        {
            animation->removeClip(clipName.c_str());
            mSelectedFrame = -1;
        });
        ImGui::PopStyleColor();
        ImGui::EndChild();
        return;
    }
    ImGui::PopStyleColor();
    ImGui::BeginChild("##frames_row", ImVec2(0.0f, 148.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (clip->frames.empty())
        ImGui::TextDisabled("Drag rects from Sprite Editor here");
    for (size_t i = 0; i < clip->frames.size(); ++i)
    {
        const AnimationFrame &frame = clip->frames[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();
        const bool selected = mSelectedFrame == static_cast<int>(i);
        if (frame.texture)
        {
            const float width = static_cast<float>(frame.texture->Width());
            const float height = static_cast<float>(frame.texture->Height());
            const ImVec2 uv0(frame.rect.x / width, frame.rect.y / height);
            const ImVec2 uv1((frame.rect.x + frame.rect.z) / width, (frame.rect.y + frame.rect.w) / height);
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(frame.texture->Id())),
                         ImVec2(72.0f, 72.0f), uv0, uv1, ImVec4(1, 1, 1, 1),
                         selected ? ImVec4(0.25f, 0.65f, 1.0f, 1.0f) : ImVec4(0, 0, 0, 0));
        }
        else
            ImGui::Button("missing", ImVec2(72.0f, 72.0f));
        if (ImGui::IsItemClicked())
            mSelectedFrame = static_cast<int>(i);
        ImGui::Text("%d", static_cast<int>(i) + 1);
        drawFrameEventMarker(*animation, *clip, i);
        ImGui::EndGroup();
        if (i + 1 < clip->frames.size()) ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::EndChild();
    // After EndChild the whole timeline child is the last ImGui item, so this
    // target covers the actual frame row rather than a separate drop button.
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kSpriteRegionDragDropPayload))
        {
            const auto *data = static_cast<const SpriteRegionDragDropData *>(payload->Data);
            const Math::Vec4 rect(data->x, data->y, data->width, data->height);
            commit(app(), "Add Animation Frame", [&]
            {
                animation->addFrame(clip->name.c_str(), data->texture, rect, data->texturePath);
                app().settings().viewportLivePreview = true;
                animation->play(clip->name.c_str());
                mSelectedFrame = static_cast<int>(clip->frames.size()) - 1;
            });
        }
        ImGui::EndDragDropTarget();
    }

    if (mSelectedFrame >= 0 && static_cast<size_t>(mSelectedFrame) < clip->frames.size())
    {
        ImGui::Separator();
        AnimationFrame &frame = clip->frames[static_cast<size_t>(mSelectedFrame)];
        Math::Vec2 offset = frame.offset;
        if (ImGui::DragFloat2("Frame Offset", &offset.x, 0.25f))
            animation->setFrameOffset(clip->name.c_str(), static_cast<size_t>(mSelectedFrame), offset);
        ImGui::SameLine(0.0f, 8.0f);
        if (ImGui::Button(ICON_MDI_RESTORE "##resetFrameOffset", ImVec2(26.0f, 0.0f)))
            commit(app(), "Reset Animation Frame Offset", [&]
            { animation->setFrameOffset(clip->name.c_str(), static_cast<size_t>(mSelectedFrame), Math::Vec2(0.0f)); });
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reset Frame Offset");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.14f, 0.16f, 1.0f));
        if (ImGui::Button(ICON_MDI_DELETE "  Delete Frame"))
        {
            const size_t frameIndex = static_cast<size_t>(mSelectedFrame);
            commit(app(), "Delete Animation Frame", [&]
            {
                animation->removeFrame(clip->name.c_str(), frameIndex);
                mSelectedFrame = -1;
            });
        }
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Moves only this frame around the animated object's origin.");
    }
    ImGui::EndChild();
}
}
