#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{

class TileMapPanel final : public EditorPanel
{
public:
    explicit TileMapPanel(EditorApplication &application);

private:
    enum class Tool
    {
        Brush,
        Pick,
        Fill,
        Rectangle
    };

    void drawContents() override;

    int mSelectedTile = 1;
    float mAtlasZoom = 1.0f;
    float mMapZoom = 1.0f;
    bool mCollisionBrush = false;
    bool mPainting = false;
    bool mActionApplied = false;
    Tool mTool = Tool::Brush;
    int mRectStartX = 0;
    int mRectStartY = 0;
    int mRectEndX = 0;
    int mRectEndY = 0;
    bool mRectangleErase = false;
};

}
