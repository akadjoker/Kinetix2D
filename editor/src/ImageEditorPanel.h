#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <k2d/Pixmap.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
class Assets;
class Texture;
}

namespace ig::retained
{
class ColorButton;
class Label;
class SpinSlider;
class ToolBar;
class Widget;
}

namespace k2d::editor
{

class ImageCanvas;
class ColourSwatches;

enum class PixelTool
{
    Pencil,
    Eraser,
    Fill,
    Line,
    Rectangle,
    Picker
};

// Pixel painting on an image of the project, straight into the texture the
// scene uses, so sprites change as they are painted: pencil, eraser, fill,
// line, rectangle and picker, a colour with the recent ones, its own undo,
// and a normal map made from the image. Save writes the PNG; Revert reads it
// back.
class ImageEditorPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(ImageEditorPanel, ig::retained::BoxLayout);

    explicit ImageEditorPanel(Assets& assets);

    void setRoot(const ct::String& root) { mRoot = root; }
    bool open(const ct::String& path);
    bool save();
    void revert();
    void undo();
    void redo();
    bool owns(const ig::retained::Widget* focused) const;
    bool dirty() const { return mDirty; }

    // For the canvas.
    PixelTool tool() const { return mTool; }
    int brushSize() const;
    void colour(unsigned char out[4]) const;
    void setColour(const unsigned char rgba[4]);
    Pixmap& image() { return mImage; }
    bool hasImage() const { return mTexture != nullptr; }
    void beginEdit();          // before a change: an undo step
    void pixelsChanged();      // after a change: the texture follows

private:
    void syncTitle();
    void syncColourFields();
    void normalMap();
    void selectTool(PixelTool tool);

    Assets& mAssets;
    ct::String mRoot;
    ct::String mPath;          // project path
    Texture* mTexture = nullptr;
    Pixmap mImage;
    bool mDirty = false;
    PixelTool mTool = PixelTool::Pencil;
    unsigned char mColour[4] = {255, 255, 255, 255};
    ct::Vector<ct::Vector<unsigned char>> mUndo;
    ct::Vector<ct::Vector<unsigned char>> mRedo;

    ig::retained::ToolBar* mTools = nullptr;
    int mToolItems[6] = {-1, -1, -1, -1, -1, -1};
    ig::retained::SpinSlider* mSize = nullptr;
    ig::retained::ColorButton* mColourButton = nullptr;
    ColourSwatches* mSwatches = nullptr;
    ig::retained::Label* mTitle = nullptr;
    ImageCanvas* mCanvas = nullptr;
};

} // namespace k2d::editor
