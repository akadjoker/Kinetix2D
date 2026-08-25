#include "ImageEditorPanel.h"

#include "AssetsPanel.h"
#include "core/EditorApplication.h"
#include "core/EditorFileSystem.h"

#include <k2d/Pixmap.h>

#include <imgui.h>
#include <IconsMaterialDesignIcons.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace k2d::editor
{
namespace
{
unsigned char byte(float value)
{
    return static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, value)) * 255.0f + 0.5f);
}

Math::Vec4 makeRect(const Math::Vec2& a, const Math::Vec2& b)
{
    return Math::Vec4(std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(a.x - b.x) + 1.0f, std::fabs(a.y - b.y) + 1.0f);
}
} // namespace

ImageEditorPanel::ImageEditorPanel(EditorApplication& application) : EditorPanel("Image Editor", application)
{
}

ImageEditorPanel::~ImageEditorPanel()
{
    delete mImage;
    delete mClipboard;
}

void ImageEditorPanel::refreshPreview()
{
    if (!mImage)
        return;
    if (mPreview.Width() != mImage->Width() || mPreview.Height() != mImage->Height())
        mPreview.Create(mImage->Width(), mImage->Height(), mImage->Pixels(), false, false);
    else
        mPreview.Update(mImage->Pixels());
}

void ImageEditorPanel::openImage(const char* path)
{
    if (!path || !path[0])
        return;
    Pixmap* image = new Pixmap();
    if (!image->Load(path))
    {
        delete image;
        app().toasts().error("Could not read image pixels");
        return;
    }
    delete mImage;
    mImage = image;
    mSourcePath = path;
    mSelection = Math::Vec4(0.0f);
    mDirty = false;
    mZoom = 1.0f;
    refreshPreview();

    const ct::String base = EditorFileSystem::withoutExtension(mSourcePath);
    std::snprintf(mOutputDirectory, sizeof(mOutputDirectory), "%s_split", base.c_str());
    std::snprintf(mEditedOutputPath, sizeof(mEditedOutputPath), "%s_edited.png", base.c_str());
    std::snprintf(mOutputPrefix, sizeof(mOutputPrefix), "%s", EditorFileSystem::fileName(base).c_str());
    mColumns = 4;
    mRows = 1;
    mOverwrite = false;
    app().toasts().success("Image opened in editor");
}

Math::Vec2 ImageEditorPanel::canvasPoint(const ImVec2& origin, const ImVec2& mouse) const
{
    const float x = std::max(0.0f, std::min((mouse.x - origin.x) / mZoom, float(mImage->Width() - 1)));
    const float y = std::max(0.0f, std::min((mouse.y - origin.y) / mZoom, float(mImage->Height() - 1)));
    return Math::Vec2(std::floor(x), std::floor(y));
}

void ImageEditorPanel::paintStroke(const Math::Vec2& from, const Math::Vec2& to, bool erase)
{
    const int radius = std::max(1, int(mBrushSize * 0.5f));
    const int steps = std::max(1, std::max(int(std::fabs(to.x - from.x)), int(std::fabs(to.y - from.y))));
    const unsigned char r = erase ? 0 : byte(mColor[0]);
    const unsigned char g = erase ? 0 : byte(mColor[1]);
    const unsigned char b = erase ? 0 : byte(mColor[2]);
    const unsigned char a = erase ? 0 : byte(mColor[3]);
    for (int i = 0; i <= steps; ++i)
    {
        const float t = float(i) / float(steps);
        mImage->FillCircle(int(from.x + (to.x - from.x) * t), int(from.y + (to.y - from.y) * t), radius, r, g, b, a);
    }
}

void ImageEditorPanel::applyShape(const Math::Vec2& from, const Math::Vec2& to)
{
    const unsigned char r = byte(mColor[0]), g = byte(mColor[1]), b = byte(mColor[2]), a = byte(mColor[3]);
    if (mTool == Tool::Line)
        mImage->DrawLine(int(from.x), int(from.y), int(to.x), int(to.y), r, g, b, a);
    else if (mTool == Tool::Rectangle)
    {
        const Math::Vec4 rect = makeRect(from, to);
        if (mFillShapes)
            mImage->FillRect(int(rect.x), int(rect.y), int(rect.z), int(rect.w), r, g, b, a);
        else
            mImage->DrawRect(int(rect.x), int(rect.y), int(rect.z), int(rect.w), r, g, b, a);
    }
    else if (mTool == Tool::Circle)
    {
        const int radius = int(std::sqrt((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y)));
        if (mFillShapes)
            mImage->FillCircle(int(from.x), int(from.y), radius, r, g, b, a);
        else
            mImage->DrawCircle(int(from.x), int(from.y), radius, r, g, b, a);
    }
}

void ImageEditorPanel::copySelection(bool cut)
{
    if (!mImage || mSelection.z <= 0.0f || mSelection.w <= 0.0f)
        return;
    Pixmap* copy = new Pixmap();
    if (!mImage->CopyRect(int(mSelection.x), int(mSelection.y), int(mSelection.z), int(mSelection.w), *copy))
    {
        delete copy;
        return;
    }
    delete mClipboard;
    mClipboard = copy;
    if (cut)
    {
        mImage->FillRect(int(mSelection.x), int(mSelection.y), int(mSelection.z), int(mSelection.w), 0, 0, 0, 0);
        mDirty = true;
        refreshPreview();
    }
    mTool = Tool::Paste;
    app().toasts().info(cut ? "Selection cut — click the canvas to paste"
                            : "Selection copied — click the canvas to paste");
}

void ImageEditorPanel::saveEditedImage()
{
    if (!mImage || !mEditedOutputPath[0])
        return;
    if (!mImage->Save(mEditedOutputPath))
    {
        app().toasts().error("Could not save edited PNG");
        return;
    }
    mDirty = false;
    app().toasts().success("Edited PNG saved");
}

void ImageEditorPanel::splitGrid()
{
    if (!mImage || !mOutputDirectory[0] || !mOutputPrefix[0] || !EditorFileSystem::makeDirectory(mOutputDirectory))
    {
        app().toasts().error("Could not create split output folder");
        return;
    }
    int written = 0, skipped = 0, failed = 0;
    for (int row = 0; row < mRows; ++row)
        for (int column = 0; column < mColumns; ++column)
        {
            const int left = column * mImage->Width() / mColumns;
            const int top = row * mImage->Height() / mRows;
            const int right = (column + 1) * mImage->Width() / mColumns;
            const int bottom = (row + 1) * mImage->Height() / mRows;
            char name[192];
            if (mRows == 1)
                std::snprintf(name, sizeof(name), "%s_%03d.png", mOutputPrefix, row * mColumns + column + 1);
            else
                std::snprintf(name, sizeof(name), "%s_r%02d_c%02d.png", mOutputPrefix, row + 1, column + 1);
            const ct::String output = EditorFileSystem::join(mOutputDirectory, name);
            if (!mOverwrite && EditorFileSystem::exists(output))
            {
                ++skipped;
                continue;
            }
            Pixmap part;
            if (!mImage->CopyRect(left, top, right - left, bottom - top, part) || !part.Save(output.c_str()))
                ++failed;
            else
                ++written;
        }
    char message[128];
    std::snprintf(message, sizeof(message), "Split %d image(s), %d skipped, %d failed", written, skipped, failed);
    app().log(message);
    failed ? app().toasts().error(message) : app().toasts().success(message);
}

void ImageEditorPanel::drawContents()
{
    ImGui::TextDisabled("Paint pixels in a Pixmap. The preview texture is editor-only.");
    ImGui::Button(ICON_MDI_IMAGE "  Source Image", ImVec2(190.0f, 28.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
            openImage(static_cast<const char*>(payload->Data));
        ImGui::EndDragDropTarget();
    }
    if (!mImage)
    {
        ImGui::TextDisabled("Drag an image from Assets onto Source Image.");
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s — %d × %d%s", EditorFileSystem::fileName(mSourcePath).c_str(), mImage->Width(),
                        mImage->Height(), mDirty ? " *" : "");

    ImGui::SeparatorText("Paint Tools");
    const char* tools[] = {"Brush", "Eraser", "Line", "Rectangle", "Circle", "Select", "Paste"};
    int tool = int(mTool);
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::Combo("Tool", &tool, tools, int(sizeof(tools) / sizeof(tools[0]))))
        mTool = Tool(tool);
    ImGui::SameLine();
    ImGui::ColorEdit4("Color", mColor, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(125.0f);
    ImGui::SliderFloat("Brush", &mBrushSize, 1, 128, "%.0f px");
    if (mTool == Tool::Rectangle || mTool == Tool::Circle)
    {
        ImGui::SameLine();
        ImGui::Checkbox("Fill", &mFillShapes);
    }
    const bool selected = mSelection.z > 0 && mSelection.w > 0;
    ImGui::BeginDisabled(!selected);
    if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy"))
        copySelection(false);
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_CONTENT_CUT " Cut"))
        copySelection(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!mClipboard);
    if (ImGui::Button(ICON_MDI_CONTENT_PASTE " Paste"))
        mTool = Tool::Paste;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(105.0f);
    ImGui::SliderFloat("Zoom", &mZoom, 0.05f, 8.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);

    ImGui::BeginChild("##image_canvas", ImVec2(0, 390), true, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<ImTextureID>(intptr_t(mPreview.Id())),
                 ImVec2(mImage->Width() * mZoom, mImage->Height() * mZoom));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
        mZoom = std::max(0.05f, std::min(8.0f, mZoom * std::pow(1.18f, ImGui::GetIO().MouseWheel)));
    const Math::Vec2 point = canvasPoint(origin, ImGui::GetIO().MousePos);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (mTool == Tool::Paste && mClipboard)
        {
            mImage->Blit(*mClipboard, int(point.x), int(point.y));
            mSelection =
                Math::Vec4(point.x, point.y, float(std::min(mClipboard->Width(), mImage->Width() - int(point.x))),
                           float(std::min(mClipboard->Height(), mImage->Height() - int(point.y))));
            mDirty = true;
            refreshPreview();
        }
        else
        {
            mDrawing = true;
            mDrawStart = mDrawEnd = point;
            if (mTool == Tool::Brush || mTool == Tool::Eraser)
            {
                paintStroke(point, point, mTool == Tool::Eraser);
                mDirty = true;
                refreshPreview();
            }
        }
    }
    if (mDrawing && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (mTool == Tool::Brush || mTool == Tool::Eraser)
        {
            paintStroke(mDrawEnd, point, mTool == Tool::Eraser);
            mDirty = true;
            refreshPreview();
        }
        mDrawEnd = point;
    }
    if (mDrawing && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (mTool == Tool::Line || mTool == Tool::Rectangle || mTool == Tool::Circle)
        {
            applyShape(mDrawStart, mDrawEnd);
            mDirty = true;
            refreshPreview();
        }
        else if (mTool == Tool::Select)
            mSelection = makeRect(mDrawStart, mDrawEnd);
        mDrawing = false;
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (selected)
        draw->AddRect(
            ImVec2(origin.x + mSelection.x * mZoom, origin.y + mSelection.y * mZoom),
            ImVec2(origin.x + (mSelection.x + mSelection.z) * mZoom, origin.y + (mSelection.y + mSelection.w) * mZoom),
            IM_COL32(255, 205, 65, 255), 0, 0, 2);
    if (mDrawing && (mTool == Tool::Line || mTool == Tool::Rectangle || mTool == Tool::Circle || mTool == Tool::Select))
    {
        const ImU32 color = mTool == Tool::Select ? IM_COL32(255, 205, 65, 255) : IM_COL32(90, 200, 255, 255);
        if (mTool == Tool::Line)
            draw->AddLine(ImVec2(origin.x + mDrawStart.x * mZoom, origin.y + mDrawStart.y * mZoom),
                          ImVec2(origin.x + mDrawEnd.x * mZoom, origin.y + mDrawEnd.y * mZoom), color, 2);
        else if (mTool == Tool::Circle)
            draw->AddCircle(ImVec2(origin.x + mDrawStart.x * mZoom, origin.y + mDrawStart.y * mZoom),
                            std::sqrt((mDrawEnd.x - mDrawStart.x) * (mDrawEnd.x - mDrawStart.x) +
                                      (mDrawEnd.y - mDrawStart.y) * (mDrawEnd.y - mDrawStart.y)) *
                                mZoom,
                            color, 32, 2);
        else
        {
            const Math::Vec4 r = makeRect(mDrawStart, mDrawEnd);
            draw->AddRect(ImVec2(origin.x + r.x * mZoom, origin.y + r.y * mZoom),
                          ImVec2(origin.x + (r.x + r.z) * mZoom, origin.y + (r.y + r.w) * mZoom), color, 0, 0, 2);
        }
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Save Edited PNG");
    ImGui::InputText("Edited PNG", mEditedOutputPath, sizeof(mEditedOutputPath));
    ImGui::BeginDisabled(!mDirty || !mEditedOutputPath[0]);
    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save Edited PNG"))
        saveEditedImage();
    ImGui::EndDisabled();
    ImGui::SeparatorText("Split Grid");
    ImGui::SetNextItemWidth(90);
    ImGui::InputInt("Columns", &mColumns);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::InputInt("Rows", &mRows);
    mColumns = std::max(1, std::min(mColumns, mImage->Width()));
    mRows = std::max(1, std::min(mRows, mImage->Height()));
    ImGui::TextDisabled("%d output image(s), approximately %d × %d px", mColumns * mRows, mImage->Width() / mColumns,
                        mImage->Height() / mRows);
    ImGui::InputText("Output Folder", mOutputDirectory, sizeof(mOutputDirectory));
    ImGui::InputText("File Prefix", mOutputPrefix, sizeof(mOutputPrefix));
    ImGui::Checkbox("Overwrite existing PNGs", &mOverwrite);
    ImGui::BeginDisabled(!mOutputDirectory[0] || !mOutputPrefix[0]);
    if (ImGui::Button(ICON_MDI_CONTENT_CUT " Split Image"))
        splitGrid();
    ImGui::EndDisabled();
}
} // namespace k2d::editor
