#include "SpriteEditorPanel.h"

#include "AssetsPanel.h"
#include "core/EditorApplication.h"
#include "core/EditorFileSystem.h"

#include <k2d/Assets.h>
#include <k2d/Animation2D.h>
#include <k2d/GameObject.h>
#include <k2d/FileSystem.h>
#include <k2d/Pixmap.h>
#include <k2d/Scene.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Texture.h>

#include <imgui.h>
#include <IconsMaterialDesignIcons.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace k2d::editor
{

SpriteEditorPanel::SpriteEditorPanel(EditorApplication &application) : EditorPanel("Sprite Editor", application)
{
}

void SpriteEditorPanel::openTexture(const char *path)
{
    if (!path || !path[0])
        return;
    mTexture = app().assets().GetTexture(path);
    if (!mTexture)
        mTexture = app().assets().LoadTexture(path, path, true, false);
    if (!mTexture)
        return;
    mMetadataPath = EditorFileSystem::withoutExtension(path);
    mMetadataPath += ".sprite.json";
    mAtlas.clear();
    mAtlas.setTexturePath(path);
    if (FileSystem::Instance().Exists(mMetadataPath.c_str()))
    {
        if (!mAtlas.load(mMetadataPath.c_str()))
            app().toasts().error("Could not load sprite metadata");
        else
            app().toasts().success("Sprite metadata loaded");
    }
    mSpriteBaseName = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path));
    std::snprintf(mRegionName, sizeof(mRegionName), "%s_001", mSpriteBaseName.c_str());
    std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", mRegionName);
    mSelectedRect = Math::Vec4(0.0f);
    mSelectedRegion = -1;
    mDirty = false;
}

void SpriteEditorPanel::drawContents()
{
    ImGui::TextDisabled("Create as many named Sprite objects as needed. They are saved with the Scene.");
    ImGui::Button(ICON_MDI_IMAGE "  Texture", ImVec2(180.0f, 28.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
            openTexture(static_cast<const char *>(payload->Data));
        ImGui::EndDragDropTarget();
    }
    if (!mTexture)
        return;
    SpriteAtlas &atlas = mAtlas;

    ImGui::SameLine();
    ImGui::TextDisabled("Creation palette — created sprites persist in the Scene");
    ImGui::SliderFloat("Zoom", &mZoom, 0.10f, 32.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
    ImGui::TextDisabled("%d region(s)", static_cast<int>(atlas.regionCount()));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.14f, 0.16f, 1.0f));
    if (ImGui::Button(ICON_MDI_DELETE "  Clear"))
    {
        atlas.clear();
        atlas.setTexturePath(app().assets().FindTextureName(mTexture));
        mSelectedRect = Math::Vec4(0.0f);
        mSelectedRegion = -1;
        std::snprintf(mRegionName, sizeof(mRegionName), "%s_001", mSpriteBaseName.c_str());
        std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", mRegionName);
        mDirty = true;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button(mDirty ? ICON_MDI_CONTENT_SAVE "  Save *" : ICON_MDI_CONTENT_SAVE "  Save"))
    {
        if (atlas.save(mMetadataPath.c_str()))
        {
            mDirty = false;
            app().toasts().success("Sprite metadata updated");
        }
        else
            app().toasts().error("Could not save sprite metadata");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", mMetadataPath.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragInt("Alpha", &mAlphaThreshold, 1.0f, 1, 255);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragInt("Min Pixels", &mMinimumPixels, 1.0f, 1, 1000000);
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_AUTO_FIX "  Detect"))
    {
        Pixmap image;
        const char *texturePath = app().assets().FindTextureName(mTexture);
        if (!texturePath || !image.Load(texturePath))
            app().toasts().error("Could not read texture pixels");
        else
        {
            const size_t found = atlas.detectTransparentRegions(
                image, static_cast<unsigned char>(mAlphaThreshold), mMinimumPixels);
            for (size_t i = 0; i < atlas.regionCount(); ++i)
            {
                const SpriteRegion *region = atlas.regionAt(i);
                if (!region)
                    continue;
                const ct::String oldId = region->id;
                char id[64];
                std::snprintf(id, sizeof(id), "%s_%03d", mSpriteBaseName.c_str(), static_cast<int>(i) + 1);
                atlas.set(oldId.c_str(), id, region->rect);
            }
            mSelectedRegion = -1;
            mSelectedRect = Math::Vec4(0.0f);
            mDirty = true;
            ct::String message("Detected ");
            message += std::to_string(found).c_str();
            message += " sprite regions";
            app().toasts().success(message);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Replaces the list with connected non-transparent pixel regions");

    ImGui::SeparatorText("Atlas Grid");
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("Columns", &mAtlasColumns);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("Rows", &mAtlasRows);
    ImGui::SameLine();
    ImGui::BeginDisabled(mAtlasColumns < 1 || mAtlasRows < 1);
    if (ImGui::Button(ICON_MDI_GRID "  Split"))
    {
        const int columns = std::max(1, mAtlasColumns);
        const int rows = std::max(1, mAtlasRows);
        const float cellWidth = static_cast<float>(mTexture->Width()) / static_cast<float>(columns);
        const float cellHeight = static_cast<float>(mTexture->Height()) / static_cast<float>(rows);
        atlas.clear();
        atlas.setTexturePath(app().assets().FindTextureName(mTexture));
        int number = 0;
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < columns; ++x)
            {
                char id[64];
                std::snprintf(id, sizeof(id), "%s_%03d", mSpriteBaseName.c_str(), ++number);
                atlas.add(id, Math::Vec4(x * cellWidth, y * cellHeight, cellWidth, cellHeight));
            }
        mSelectedRegion = atlas.regionCount() ? 0 : -1;
        if (const SpriteRegion *first = atlas.regionAt(0))
        {
            mSelectedRect = first->rect;
            std::snprintf(mRegionName, sizeof(mRegionName), "%s", first->id.c_str());
            std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", first->id.c_str());
        }
        app().toasts().success("Atlas split into named sprite regions");
        mDirty = true;
    }
    ImGui::EndDisabled();

    ImGui::BeginChild("##sprite_regions", ImVec2(190.0f, 330.0f), true);
    ImGui::TextDisabled("Sprite Regions");
    for (size_t i = 0; i < atlas.regionCount(); ++i)
    {
        const SpriteRegion *region = atlas.regionAt(i);
        if (!region)
            continue;
        if (ImGui::Selectable(region->id.c_str(), mSelectedRegion == static_cast<int>(i)))
        {
            mSelectedRegion = static_cast<int>(i);
            mSelectedRect = region->rect;
            std::snprintf(mRegionName, sizeof(mRegionName), "%s", region->id.c_str());
            std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", region->id.c_str());
        }
        if (ImGui::BeginDragDropSource())
        {
            SpriteRegionDragDropData data;
            data.texture = mTexture;
            const char *texturePath = app().assets().FindTextureName(mTexture);
            if (!texturePath || !texturePath[0])
                texturePath = atlas.texturePath().c_str();
            std::snprintf(data.texturePath, sizeof(data.texturePath), "%s", texturePath ? texturePath : "");
            data.x = region->rect.x;
            data.y = region->rect.y;
            data.width = region->rect.z;
            data.height = region->rect.w;
            ImGui::SetDragDropPayload(kSpriteRegionDragDropPayload, &data, sizeof(data));
            ImGui::Text("Add '%s' to Animation2D", region->id.c_str());
            ImGui::EndDragDropSource();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##sprite_canvas", ImVec2(0.0f, 330.0f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(mTexture->Width() * mZoom, mTexture->Height() * mZoom);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(mTexture->Id())), size);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
    {
        const float zoomFactor = std::pow(1.18f, ImGui::GetIO().MouseWheel);
        mZoom = std::max(0.10f, std::min(32.0f, mZoom * zoomFactor));
    }
    const auto sourcePosition = [&]()
    {
        return Math::Vec2(std::max(0.0f, std::min((mouse.x - origin.x) / mZoom, (float)mTexture->Width())),
                          std::max(0.0f, std::min((mouse.y - origin.y) / mZoom, (float)mTexture->Height())));
    };
    const auto handleAtMouse = [&]() -> int
    {
        if (mSelectedRegion < 0 || mSelectedRect.z <= 0.0f || mSelectedRect.w <= 0.0f)
            return -1;
        const Math::Vec2 handles[4] = {
            Math::Vec2(mSelectedRect.x, mSelectedRect.y),
            Math::Vec2(mSelectedRect.x + mSelectedRect.z, mSelectedRect.y),
            Math::Vec2(mSelectedRect.x + mSelectedRect.z, mSelectedRect.y + mSelectedRect.w),
            Math::Vec2(mSelectedRect.x, mSelectedRect.y + mSelectedRect.w)};
        for (int i = 0; i < 4; ++i)
        {
            const float dx = mouse.x - (origin.x + handles[i].x * mZoom);
            const float dy = mouse.y - (origin.y + handles[i].y * mZoom);
            if (dx * dx + dy * dy <= 196.0f)
                return i;
        }
        return -1;
    };
    const int hoveredHandle = hovered ? handleAtMouse() : -1;
    if (hoveredHandle >= 0 || mDraggingHandle >= 0)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const int handle = handleAtMouse();
        if (handle >= 0)
        {
            mDraggingHandle = handle;
            mSelecting = false;
        }
        else
        {
            mSelectedRegion = -1;
            std::snprintf(mRegionName, sizeof(mRegionName), "%s_%03d", mSpriteBaseName.c_str(),
                          static_cast<int>(atlas.regionCount()) + 1);
            std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", mRegionName);
            mSelecting = true;
            mSelectionStart = sourcePosition();
            mSelectionEnd = mSelectionStart;
        }
    }
    if (mDraggingHandle >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const Math::Vec2 point = sourcePosition();
        const float right = mSelectedRect.x + mSelectedRect.z;
        const float bottom = mSelectedRect.y + mSelectedRect.w;
        switch (mDraggingHandle)
        {
        case 0:
            mSelectedRect.x = std::min(point.x, right - 1.0f);
            mSelectedRect.y = std::min(point.y, bottom - 1.0f);
            mSelectedRect.z = right - mSelectedRect.x;
            mSelectedRect.w = bottom - mSelectedRect.y;
            break;
        case 1:
            mSelectedRect.y = std::min(point.y, bottom - 1.0f);
            mSelectedRect.z = std::max(1.0f, point.x - mSelectedRect.x);
            mSelectedRect.w = bottom - mSelectedRect.y;
            break;
        case 2:
            mSelectedRect.z = std::max(1.0f, point.x - mSelectedRect.x);
            mSelectedRect.w = std::max(1.0f, point.y - mSelectedRect.y);
            break;
        case 3:
            mSelectedRect.x = std::min(point.x, right - 1.0f);
            mSelectedRect.z = right - mSelectedRect.x;
            mSelectedRect.w = std::max(1.0f, point.y - mSelectedRect.y);
            break;
        }
    }
    if (mDraggingHandle >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const SpriteRegion *selected = mSelectedRegion >= 0
                                           ? atlas.regionAt(static_cast<size_t>(mSelectedRegion))
                                           : nullptr;
        if (selected)
            mDirty = atlas.set(selected->id.c_str(), mRegionName, mSelectedRect) || mDirty;
        mDraggingHandle = -1;
    }
    if (mSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        mSelectionEnd = sourcePosition();
    if (mSelecting && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float left = std::min(mSelectionStart.x, mSelectionEnd.x);
        const float top = std::min(mSelectionStart.y, mSelectionEnd.y);
        mSelectedRect = Math::Vec4(left, top, std::fabs(mSelectionEnd.x - mSelectionStart.x),
                                   std::fabs(mSelectionEnd.y - mSelectionStart.y));
        mSelecting = false;
    }

    ImDrawList *draw = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < atlas.regionCount(); ++i)
    {
        const SpriteRegion *region = atlas.regionAt(i);
        if (!region)
            continue;
        const Math::Vec4 &r = region->rect;
        const ImU32 color = mSelectedRegion == static_cast<int>(i) ? IM_COL32(255, 205, 65, 255)
                                                                    : IM_COL32(100, 200, 255, 190);
        draw->AddRect(ImVec2(origin.x + r.x * mZoom, origin.y + r.y * mZoom),
                      ImVec2(origin.x + (r.x + r.z) * mZoom, origin.y + (r.y + r.w) * mZoom),
                      color, 0.0f, 0, mSelectedRegion == static_cast<int>(i) ? 2.0f : 1.0f);
    }
    if (mSelectedRect.z > 0.0f && mSelectedRect.w > 0.0f)
    {
        draw->AddRect(ImVec2(origin.x + mSelectedRect.x * mZoom, origin.y + mSelectedRect.y * mZoom),
                      ImVec2(origin.x + (mSelectedRect.x + mSelectedRect.z) * mZoom,
                             origin.y + (mSelectedRect.y + mSelectedRect.w) * mZoom),
                      IM_COL32(255, 205, 65, 255), 0.0f, 0, 2.0f);
        const ImVec2 handles[4] = {
            ImVec2(origin.x + mSelectedRect.x * mZoom, origin.y + mSelectedRect.y * mZoom),
            ImVec2(origin.x + (mSelectedRect.x + mSelectedRect.z) * mZoom, origin.y + mSelectedRect.y * mZoom),
            ImVec2(origin.x + (mSelectedRect.x + mSelectedRect.z) * mZoom,
                   origin.y + (mSelectedRect.y + mSelectedRect.w) * mZoom),
            ImVec2(origin.x + mSelectedRect.x * mZoom, origin.y + (mSelectedRect.y + mSelectedRect.w) * mZoom)};
        for (int i = 0; i < 4; ++i)
        {
            const bool hot = hoveredHandle == i || mDraggingHandle == i;
            draw->AddCircleFilled(handles[i], hot ? 11.0f : 7.0f,
                                  mDraggingHandle == i ? IM_COL32(255, 130, 55, 255)
                                                       : IM_COL32(255, 205, 65, 255));
            draw->AddCircle(handles[i], hot ? 11.0f : 7.0f, IM_COL32(20, 24, 31, 255), 16, 1.5f);
        }
    }
    ImGui::EndChild();

    ImGui::SeparatorText(mSelectedRegion >= 0 ? "Selected Sprite" : "New Sprite");
    ImGui::TextDisabled("Drag the four yellow corner handles to resize the rectangle.");
    ImGui::InputText("ID", mRegionName, sizeof(mRegionName));
    ImGui::DragFloat4("Rect", &mSelectedRect.x, 0.5f, 0.0f, 0.0f, "%.0f");
    ImGui::BeginDisabled(!mRegionName[0] || mSelectedRect.z <= 0.0f || mSelectedRect.w <= 0.0f);
    if (mSelectedRegion < 0 && ImGui::Button(ICON_MDI_PLUS "  Add Region"))
    {
        if (!atlas.add(mRegionName, mSelectedRect))
            app().toasts().error("Region ID already exists");
        else
        {
            mSelectedRegion = static_cast<int>(atlas.regionCount() - 1);
            std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", mRegionName);
            mDirty = true;
        }
    }
    if (mSelectedRegion >= 0 && ImGui::Button(ICON_MDI_CONTENT_SAVE_EDIT "  Update"))
    {
        const SpriteRegion *selected = atlas.regionAt(static_cast<size_t>(mSelectedRegion));
        if (!selected || !atlas.set(selected->id.c_str(), mRegionName, mSelectedRect))
            app().toasts().error("Sprite ID already exists");
        else
            mDirty = true;
    }
    ImGui::EndDisabled();
    if (mSelectedRegion >= 0)
    {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_FILE_PLUS "  New"))
        {
            mSelectedRegion = -1;
            mSelectedRect = Math::Vec4(0.0f);
            std::snprintf(mRegionName, sizeof(mRegionName), "%s_%03d", mSpriteBaseName.c_str(),
                          static_cast<int>(atlas.regionCount()) + 1);
            std::snprintf(mSceneObjectName, sizeof(mSceneObjectName), "%s", mRegionName);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.14f, 0.16f, 1.0f));
        if (ImGui::Button(ICON_MDI_DELETE "  Delete"))
        {
            const SpriteRegion *selected = atlas.regionAt(static_cast<size_t>(mSelectedRegion));
            if (selected)
                atlas.remove(selected->id.c_str());
            mSelectedRegion = -1;
            mSelectedRect = Math::Vec4(0.0f);
            mDirty = true;
        }
        ImGui::PopStyleColor();
    }

    if (atlas.regionCount() > 0)
    {
        ImGui::SeparatorText("Animation");
        GameObject *animationOwner = app().selection().resolve(app().scene());
        Animation2D *animation = animationOwner ? animationOwner->getComponent<Animation2D>() : nullptr;
        const char *clipName = animation ? animation->currentClip() : "";
        ImGui::BeginDisabled(!animation || !clipName || !clipName[0] || mSelectedRegion < 0);
        if (ImGui::Button(ICON_MDI_ANIMATION_PLAY "  Add to Clip"))
        {
            const EditorApplication::SceneChange before = app().beginChange();
            const char *texturePath = app().assets().FindTextureName(mTexture);
            if (!texturePath || !texturePath[0])
                texturePath = atlas.texturePath().c_str();
            animation->addFrame(clipName, mTexture, mSelectedRect, texturePath);
            animation->play(clipName);
            app().settings().viewportLivePreview = true;
            app().commitChange("Add Sprite Editor Frame", before);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!animation || !clipName || !clipName[0]);
        if (ImGui::Button(ICON_MDI_ANIMATION "  Add All"))
        {
            const EditorApplication::SceneChange before = app().beginChange();
            const char *texturePath = app().assets().FindTextureName(mTexture);
            if (!texturePath || !texturePath[0])
                texturePath = atlas.texturePath().c_str();
            for (size_t i = 0; i < atlas.regionCount(); ++i)
                if (const SpriteRegion *region = atlas.regionAt(i))
                    animation->addFrame(clipName, mTexture, region->rect, texturePath);
            animation->play(clipName);
            app().settings().viewportLivePreview = true;
            app().commitChange("Add All Sprite Editor Frames", before);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled(animation ? "or drag this region to Animator timeline" :
                                       "select the animated object to enable");

    }

    if (mSelectedRegion >= 0 && mSelectedRect.z > 0.0f && mSelectedRect.w > 0.0f)
    {
        ImGui::SeparatorText("Scene Sprite");
        ImGui::InputText("Scene Object Name", mSceneObjectName, sizeof(mSceneObjectName));
        if (ImGui::Button(ICON_MDI_PLUS "  Create Sprite"))
        {
            const EditorApplication::SceneChange before = app().beginChange();
            GameObject *created = app().scene().createObject(
                mSceneObjectName[0] ? mSceneObjectName : mRegionName);
            SpriteComponent *sprite = created->addComponent<SpriteComponent>();
            sprite->setTexture(mTexture);
            sprite->setSize(Math::Vec2(mSelectedRect.z, mSelectedRect.w));
            sprite->setSourceRect(mSelectedRect.x, mSelectedRect.y, mSelectedRect.z, mSelectedRect.w);
            app().selection().select(created);
            app().commitChange("Create Sprite from Region", before);
            app().toasts().success("Sprite created in Scene");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Creates a serializable Sprite Renderer object.");
    }
}

}
