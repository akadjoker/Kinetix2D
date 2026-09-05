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
    ImGui::TextDisabled("Drag Sprite Editor regions or images from Assets into a clip. No sprite-library nodes are created.");

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
            mSelectedPoint = -1;
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
            mSelectedPoint = -1;
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

    ImGui::Text("Timeline: %s  (drop regions or images into this row)", clip->name.c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.14f, 0.16f, 1.0f));
    if (ImGui::SmallButton(ICON_MDI_DELETE "  Delete Clip"))
    {
        const ct::String clipName = clip->name;
        commit(app(), "Delete Animation Clip", [&]
        {
            animation->removeClip(clipName.c_str());
            mSelectedFrame = -1;
            mSelectedPoint = -1;
        });
        ImGui::PopStyleColor();
        ImGui::EndChild();
        return;
    }
    ImGui::PopStyleColor();
    ImGui::BeginChild("##frames_row", ImVec2(0.0f, 148.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (clip->frames.empty())
        ImGui::TextDisabled("Drag regions from Sprite Editor or images from Assets here");
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
            const bool fullImage = frame.rect.z <= 0.0f || frame.rect.w <= 0.0f;
            const ImVec2 uv0 = fullImage ? ImVec2(0.0f, 0.0f) : ImVec2(frame.rect.x / width, frame.rect.y / height);
            const ImVec2 uv1 = fullImage ? ImVec2(1.0f, 1.0f)
                                         : ImVec2((frame.rect.x + frame.rect.z) / width,
                                                  (frame.rect.y + frame.rect.w) / height);
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(frame.texture->Id())),
                         ImVec2(72.0f, 72.0f), uv0, uv1, ImVec4(1, 1, 1, 1),
                         selected ? ImVec4(0.25f, 0.65f, 1.0f, 1.0f) : ImVec4(0, 0, 0, 0));
        }
        else
            ImGui::Button("missing", ImVec2(72.0f, 72.0f));
        if (ImGui::IsItemClicked())
        {
            if (mSelectedFrame != static_cast<int>(i))
                mSelectedPoint = -1;
            mSelectedFrame = static_cast<int>(i);
            animation->setFrame(static_cast<int>(i));
        }
        ImGui::Text("%d", static_cast<int>(i) + 1);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", (frame.rect.z <= 0.0f || frame.rect.w <= 0.0f) ? "Image" : "Region");
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
                mSelectedPoint = -1;
            });
        }
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
        {
            const char *imagePath = static_cast<const char *>(payload->Data);
            Texture *texture = app().loadOrGetTexture(imagePath);
            if (!texture)
                app().toasts().error("Could not load animation frame image");
            else
            {
                const char *texturePath = app().assets().FindTextureName(texture);
                commit(app(), "Add Animation Image Frame", [&]
                {
                    // A zero rect means the full texture, so image frames and
                    // atlas-region frames can live in the same clip.
                    animation->addFrame(clip->name.c_str(), texture, Math::Vec4(0.0f), texturePath);
                    app().settings().viewportLivePreview = true;
                    animation->play(clip->name.c_str());
                    mSelectedFrame = static_cast<int>(clip->frames.size()) - 1;
                    mSelectedPoint = -1;
                });
            }
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

        ImGui::Separator();
        ImGui::Text("Attachment Points: %d", static_cast<int>(frame.points.size()));
        const float frameWidth = frame.rect.z > 0.0f ? frame.rect.z
                                                      : frame.texture ? static_cast<float>(frame.texture->Width()) : 0.0f;
        const float frameHeight = frame.rect.w > 0.0f ? frame.rect.w
                                                       : frame.texture ? static_cast<float>(frame.texture->Height()) : 0.0f;
        if (frame.texture && frameWidth > 0.0f && frameHeight > 0.0f)
        {
            if (ImGui::Button(ICON_MDI_PLUS "  Add Point"))
            {
                const size_t newIndex = frame.points.size();
                commit(app(), "Add Animation Frame Point", [&]
                {
                    animation->addFramePoint(clip->name.c_str(), static_cast<size_t>(mSelectedFrame),
                                             Math::Vec2(frameWidth * 0.5f, frameHeight * 0.5f));
                });
                mSelectedPoint = static_cast<int>(newIndex);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Add an indexed point at the centre, then drag it over the image");

            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float maxWidth = availableWidth < 420.0f ? availableWidth : 420.0f;
            const float maxHeight = 280.0f;
            float previewScale = maxWidth / frameWidth;
            if (frameHeight * previewScale > maxHeight)
                previewScale = maxHeight / frameHeight;
            if (previewScale <= 0.0f)
                previewScale = 1.0f;
            const ImVec2 previewSize(frameWidth * previewScale, frameHeight * previewScale);
            const bool fullImage = frame.rect.z <= 0.0f || frame.rect.w <= 0.0f;
            const float textureWidth = static_cast<float>(frame.texture->Width());
            const float textureHeight = static_cast<float>(frame.texture->Height());
            const ImVec2 uv0 = fullImage ? ImVec2(0.0f, 0.0f)
                                         : ImVec2(frame.rect.x / textureWidth, frame.rect.y / textureHeight);
            const ImVec2 uv1 = fullImage ? ImVec2(1.0f, 1.0f)
                                         : ImVec2((frame.rect.x + frame.rect.z) / textureWidth,
                                                  (frame.rect.y + frame.rect.w) / textureHeight);
            const ImVec2 previewMin = ImGui::GetCursorScreenPos();
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(frame.texture->Id())), previewSize,
                         uv0, uv1);
            const ImVec2 previewMax(previewMin.x + previewSize.x, previewMin.y + previewSize.y);
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(previewMin, previewMax, IM_COL32(190, 200, 215, 220));

            for (size_t pointIndex = 0; pointIndex < frame.points.size(); ++pointIndex)
            {
                const Math::Vec2 point = frame.points[pointIndex];
                const ImVec2 marker(previewMin.x + point.x * previewScale,
                                    previewMin.y + point.y * previewScale);
                const ImU32 color = mSelectedPoint == static_cast<int>(pointIndex)
                                        ? IM_COL32(255, 225, 70, 255)
                                        : IM_COL32(60, 225, 255, 255);
                drawList->AddCircle(marker, 7.0f, color, 16, 2.0f);
                drawList->AddLine(ImVec2(marker.x - 10.0f, marker.y), ImVec2(marker.x + 10.0f, marker.y), color, 1.5f);
                drawList->AddLine(ImVec2(marker.x, marker.y - 10.0f), ImVec2(marker.x, marker.y + 10.0f), color, 1.5f);
                char indexLabel[16];
                std::snprintf(indexLabel, sizeof(indexLabel), "%d", static_cast<int>(pointIndex));
                drawList->AddText(ImVec2(marker.x + 9.0f, marker.y + 5.0f), color, indexLabel);
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !frame.points.empty())
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                int closest = -1;
                float closestDistance = 14.0f * 14.0f;
                for (size_t pointIndex = 0; pointIndex < frame.points.size(); ++pointIndex)
                {
                    const float markerX = previewMin.x + frame.points[pointIndex].x * previewScale;
                    const float markerY = previewMin.y + frame.points[pointIndex].y * previewScale;
                    const float dx = mouse.x - markerX;
                    const float dy = mouse.y - markerY;
                    const float distance = dx * dx + dy * dy;
                    if (distance <= closestDistance)
                    {
                        closest = static_cast<int>(pointIndex);
                        closestDistance = distance;
                    }
                }
                if (closest >= 0)
                    mSelectedPoint = closest;
                if (mSelectedPoint >= 0 && static_cast<size_t>(mSelectedPoint) < frame.points.size())
                {
                    app().beginTransaction("Move Animation Frame Point", app().beginChange());
                    mDraggingPoint = true;
                }
            }
            if (mDraggingPoint && mSelectedPoint >= 0 && static_cast<size_t>(mSelectedPoint) < frame.points.size())
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                float pointX = (mouse.x - previewMin.x) / previewScale;
                float pointY = (mouse.y - previewMin.y) / previewScale;
                if (pointX < 0.0f) pointX = 0.0f;
                if (pointY < 0.0f) pointY = 0.0f;
                if (pointX > frameWidth) pointX = frameWidth;
                if (pointY > frameHeight) pointY = frameHeight;
                animation->setFramePoint(clip->name.c_str(), static_cast<size_t>(mSelectedFrame),
                                         static_cast<size_t>(mSelectedPoint), Math::Vec2(pointX, pointY));
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    app().commitTransaction();
                    mDraggingPoint = false;
                }
            }

            bool removedPoint = false;
            for (size_t pointIndex = 0; pointIndex < frame.points.size(); ++pointIndex)
            {
                ImGui::PushID(static_cast<int>(pointIndex));
                char pointLabel[32];
                std::snprintf(pointLabel, sizeof(pointLabel), "Point %d", static_cast<int>(pointIndex));
                if (ImGui::Selectable(pointLabel, mSelectedPoint == static_cast<int>(pointIndex), 0,
                                      ImVec2(82.0f, 0.0f)))
                    mSelectedPoint = static_cast<int>(pointIndex);
                ImGui::SameLine();
                float point[2] = {frame.points[pointIndex].x, frame.points[pointIndex].y};
                ImGui::SetNextItemWidth(150.0f);
                const bool changed = ImGui::DragFloat2("##point", point, 0.25f, 0.0f, 0.0f, "%.1f");
                if (ImGui::IsItemActivated())
                    app().beginTransaction("Move Animation Frame Point", app().beginChange());
                if (changed)
                    animation->setFramePoint(clip->name.c_str(), static_cast<size_t>(mSelectedFrame), pointIndex,
                                             Math::Vec2(point[0], point[1]));
                if (ImGui::IsItemDeactivatedAfterEdit())
                    app().commitTransaction();
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_MDI_DELETE))
                {
                    commit(app(), "Remove Animation Frame Point", [&]
                    {
                        animation->removeFramePoint(clip->name.c_str(), static_cast<size_t>(mSelectedFrame), pointIndex);
                    });
                    mSelectedPoint = -1;
                    removedPoint = true;
                }
                ImGui::PopID();
                if (removedPoint)
                    break;
            }
            ImGui::TextDisabled("Points use frame pixels. Drag a numbered marker to place it.");
        }
        else
        {
            ImGui::TextDisabled("This frame has no image to edit points on.");
        }

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.14f, 0.16f, 1.0f));
        if (ImGui::Button(ICON_MDI_DELETE "  Delete Frame"))
        {
            const size_t frameIndex = static_cast<size_t>(mSelectedFrame);
            commit(app(), "Delete Animation Frame", [&]
            {
                animation->removeFrame(clip->name.c_str(), frameIndex);
                mSelectedFrame = -1;
                mSelectedPoint = -1;
            });
        }
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Moves only this frame around the animated object's origin.");
    }
    ImGui::EndChild();
}
}
