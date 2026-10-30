#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <k2d/Camera2D.h>

namespace k2d
{
class GameObject;
class TileMapComponent;
}

namespace ig::retained
{
class ComboBox;
class Label;
class TextInput;
class ToolBar;
}

namespace k2d::editor
{

class TilePalette;

enum class TileTool
{
    Brush,
    Eraser,
    Fill,
    Rectangle,
    Picker,
    Terrain
};

// Paints the selected object's TileMap straight in the scene view: the tile,
// or block of tiles, picked in the palette goes where the pointer is, with a
// brush, a fill, a rectangle, an eraser, or turns into the pick. In collision
// mode the same tools mark cells solid or clear instead. The terrain brush
// autotiles: it paints a terrain and the tiles around fit themselves to it,
// from the neighbours each tile of the terrain is marked to join in the
// palette. While this panel is
// showing and the object has a TileMap, the scene view's left button paints.
class TilePainterPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(TilePainterPanel, ig::retained::BoxLayout);

    TilePainterPanel();

    void show(GameObject* object);
    // Painting takes the scene view's left button.
    bool painting() const;

    void setView(const Camera2D& camera, float width, float height);
    void hover(float x, float y);
    void press(float x, float y);
    void drag(float x, float y);
    // True when the stroke changed the map.
    bool release();
    void paintOverlay(ig::retained::PaintContext& ctx, const ig::retained::Rect& area) const;

    ig::retained::Signal<const char*> addComponentRequested;
    // The panel changed the map itself (the atlas columns).
    ig::retained::Signal<const char*> edited;
    // Save as tileset: the map's tiles into a .k2dtileset it then shares.
    ig::retained::Signal<> saveTileSetRequested;

private:
    TileMapComponent* map() const;
    bool cellAt(float x, float y, int& cx, int& cy) const;
    Math::Vec2 cellCorner(float cx, float cy) const;
    int stampTile(int dx, int dy) const;
    void stamp(int cx, int cy, int originX, int originY);
    void apply(int cx, int cy, int originX, int originY);
    void fill(int cx, int cy);
    void syncInfo();
    void syncTerrains();
    void markTerrainTile(int tile, int across, int down);
    void markSolidTile(int tile);
    void animatePicked();
    // Atlas columns as the texture holds them, or 0 when it cannot tell.
    int textureColumns() const;

    GameObject* mObject = nullptr;
    TileTool mTool = TileTool::Brush;
    bool mCollision = false;
    Camera2D mCamera;
    float mWidth = 1.0f;
    float mHeight = 1.0f;
    bool mHovering = false;
    int mHoverX = 0, mHoverY = 0;
    bool mStroke = false;
    bool mChanged = false;
    int mStartX = 0, mStartY = 0;

    ig::retained::Label* mEmpty = nullptr;
    ig::retained::ToolBar* mAddMap = nullptr;
    ig::retained::BoxLayout* mContent = nullptr;
    ig::retained::ToolBar* mTools = nullptr;
    int mToolItems[6] = {-1, -1, -1, -1, -1, -1};
    int mTerrain = 0;
    bool mSyncingTerrains = false;
    ig::retained::ComboBox* mTerrains = nullptr;
    ig::retained::TextInput* mTerrainName = nullptr;
    ig::retained::ToolBar* mTerrainTools = nullptr;
    int mEditTerrainItem = -1;
    int mSolidItem = -1;
    float mFps = 8.0f;
    int mCollisionItem = -1;
    ig::retained::Label* mInfo = nullptr;
    TilePalette* mPalette = nullptr;
};

} // namespace k2d::editor
