#pragma once

#include "Signal.hpp"
#include "Widget.hpp"

namespace k2d
{
class TileMapComponent;
}

namespace k2d::editor
{

// The tileset of a TileMap cut into its tiles: a click picks a tile, a drag
// picks a block of them to stamp. Wheel zooms about the pointer, the middle or
// right button pans.
class TilePalette : public ig::retained::Widget
{
public:
    IG_WIDGET_CLASS(TilePalette, ig::retained::Widget);

    TilePalette();

    void setMap(const TileMapComponent* map);
    // The picked block, in tiles of the tileset.
    int blockX() const { return mX0 < mX1 ? mX0 : mX1; }
    int blockY() const { return mY0 < mY1 ? mY0 : mY1; }
    int blockWidth() const { return (mX0 < mX1 ? mX1 - mX0 : mX0 - mX1) + 1; }
    int blockHeight() const { return (mY0 < mY1 ? mY1 - mY0 : mY0 - mY1) + 1; }
    void pick(int tileX, int tileY);
    int tilesX() const;
    int tilesY() const;
    // While a terrain is edited a click does not pick: it lands on one of the
    // nine parts of a tile, the middle or a neighbour.
    void setTerrainEditing(int terrain);

    ig::retained::Signal<> picked;
    // The tile id, and the part clicked: 0..2 across and down.
    ig::retained::Signal<int, int, int> terrainClicked;

    Vec2f sizeHint() const override { return {300.0f, 200.0f}; }
    void paint(ig::retained::PaintContext& ctx) override;
    void onMousePress(ig::retained::MouseEvent& e) override;
    void onMouseMove(ig::retained::MouseEvent& e) override;
    void onMouseRelease(ig::retained::MouseEvent& e) override;
    void onMouseScroll(ig::retained::MouseEvent& e) override;

private:
    bool tileAt(float localX, float localY, int& tx, int& ty) const;
    bool tilePart(float localX, float localY, int& tx, int& ty, int& part) const;
    void fit();

    const TileMapComponent* mMap = nullptr;
    float mZoom = 1.0f;
    float mPanX = 8.0f;
    float mPanY = 8.0f;
    bool mFitPending = true;
    bool mPanning = false;
    bool mPicking = false;
    int mX0 = 0, mY0 = 0, mX1 = 0, mY1 = 0;
    int mTerrain = -1;
};

} // namespace k2d::editor
