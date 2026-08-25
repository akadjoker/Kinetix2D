#pragma once

#include "core/EditorPanel.h"

#include <ct/string.hpp>
#include <k2d/Texture.h>
#include <mathc.h>

namespace k2d
{
class Pixmap;
}

namespace k2d::editor
{

class ImageEditorPanel final : public EditorPanel
{
  public:
    explicit ImageEditorPanel(EditorApplication& application);
    ~ImageEditorPanel() override;

    void openImage(const char* path);

  private:
    void drawContents() override;
    void splitGrid();
    void saveEditedImage();
    void refreshPreview();
    void paintStroke(const Math::Vec2& from, const Math::Vec2& to, bool erase);
    void applyShape(const Math::Vec2& from, const Math::Vec2& to);
    void copySelection(bool cut);
    Math::Vec2 canvasPoint(const ImVec2& origin, const ImVec2& mouse) const;

    enum class Tool
    {
        Brush,
        Eraser,
        Line,
        Rectangle,
        Circle,
        Select,
        Paste
    };

    Pixmap* mImage = nullptr;
    Pixmap* mClipboard = nullptr;
    Texture mPreview;
    ct::String mSourcePath;
    char mOutputDirectory[512] = {};
    char mOutputPrefix[128] = "image";
    char mEditedOutputPath[512] = {};
    int mColumns = 4;
    int mRows = 1;
    bool mOverwrite = false;
    bool mDirty = false;
    bool mDrawing = false;
    bool mFillShapes = false;
    Tool mTool = Tool::Brush;
    float mZoom = 1.0f;
    float mBrushSize = 12.0f;
    float mColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    Math::Vec2 mDrawStart = Math::Vec2(0.0f);
    Math::Vec2 mDrawEnd = Math::Vec2(0.0f);
    Math::Vec4 mSelection = Math::Vec4(0.0f);
};

} // namespace k2d::editor
