#include "TilePainterPanel.h"

#include "Icons.h"
#include "TilePalette.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "TextInputWidgets.hpp"
#include "ToolBar.hpp"

#include <k2d/GameObject.h>
#include <k2d/Texture.h>
#include <k2d/TileMapComponent.h>

#include <ct/vector.hpp>

#include <cmath>
#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
int wrap(int value, int size)
{
    const int m = value % size;
    return m < 0 ? m + size : m;
}
} // namespace

TilePainterPanel::TilePainterPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    mEmpty = createChild<rt::Label>("Select an object to paint its TileMap");
    mEmpty->setMargins(12.0f);
    mAddMap = iconButton(this, "TileMap", "Add a TileMap to this object",
                         [this]() { addComponentRequested.emit("TileMap"); });

    mContent = createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    mContent->setStretch(1.0f);
    mContent->setPadding(4.0f);
    mContent->setSpacing(4.0f);
    auto* bar = mContent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(0.0f);
    bar->setSpacing(6.0f);
    mTools = bar->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    mToolItems[0] = mTools->addToggle(rt::IconId::PaintBrush, "Brush: paint the picked tiles", true);
    mToolItems[1] = mTools->addToggle(rt::IconId::Eraser, "Eraser: clear cells");
    mToolItems[2] = mTools->addToggle(rt::IconId::PaintBucket, "Fill: the cells like the one clicked, touching it");
    mToolItems[3] = mTools->addToggle(rt::IconId::SelectRect, "Rectangle: drag a rectangle to fill");
    mToolItems[4] = mTools->addToggle(rt::IconId::Eyedropper, "Picker: pick the tile under the pointer");
    mToolItems[5] = mTools->addToggle(Icons::get("TileMap"),
                                      "Terrain: paint the terrain below; the tiles around fit themselves to it");
    mTools->addSeparator();
    mCollisionItem = mTools->addToggle(Icons::get("TileMapCollider2D"),
                                       "Collision: the tools mark cells solid, the eraser clears them");
    mEditTerrainItem = mTools->addToggle(Icons::get("Settings"),
                                         "Edit the terrain's tiles: in the palette a tile's middle puts it in the "
                                         "terrain or takes it out, its sides and corners mark the neighbours it joins");
    mTools->addSeparator();
    const int columnsItem = mTools->addButton(rt::IconId::ViewGrid, "Atlas columns from the texture");
    mTools->buttonClicked.connect([this, columnsItem](int item)
    {
        TileMapComponent* tiles = map();
        const int columns = textureColumns();
        if (item != columnsItem || !tiles || columns <= 0 || columns == tiles->atlasTilesX())
            return;
        tiles->setAtlasTilesX(columns);
        mPalette->setMap(tiles);
        syncInfo();
        edited.emit("Atlas columns");
    });
    mTools->toggleChanged.connect([this](int item, bool on)
    {
        if (item == mCollisionItem)
            mCollision = on;
        if (item == mEditTerrainItem)
        {
            syncTerrains();
            return;
        }
        for (int i = 0; i < 6; ++i)
            if (item == mToolItems[i])
                mTool = static_cast<TileTool>(i);
        for (int i = 0; i < 6; ++i)
            mTools->setItemChecked(mToolItems[i], static_cast<int>(mTool) == i);
        syncInfo();
    });
    mInfo = bar->createChild<rt::Label>("");
    mInfo->setStretch(1.0f);
    mInfo->setLayoutAlign(rt::Align::Center);

    auto* terrainRow = mContent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    terrainRow->setPadding(0.0f);
    terrainRow->setSpacing(6.0f);
    terrainRow->createChild<rt::Label>("Terrain");
    mTerrains = terrainRow->createChild<rt::ComboBox>();
    mTerrains->selectionChanged.connect([this](int index)
    {
        if (mSyncingTerrains)
            return;
        mTerrain = index;
        syncTerrains();
    });
    mTerrainName = terrainRow->createChild<rt::TextInput>("");
    mTerrainName->setPlaceholder("Terrain name");
    mTerrainName->submitted.connect([this](const rt::String& name)
    {
        TileMapComponent* tiles = map();
        if (!tiles || mTerrain < 0 || mTerrain >= tiles->terrainCount() || name.empty() ||
            tiles->terrainName(mTerrain) == name)
            return;
        tiles->setTerrainName(mTerrain, name);
        syncTerrains();
        edited.emit("Rename terrain");
    });
    mTerrainTools = terrainRow->createChild<rt::ToolBar>();
    mTerrainTools->setDraggable(false);
    const int addItem = mTerrainTools->addButton(rt::IconId::Plus, "New terrain");
    const int removeItem = mTerrainTools->addButton(rt::IconId::Trash, "Delete the terrain, and what its tiles say");
    mTerrainTools->buttonClicked.connect([this, addItem, removeItem](int item)
    {
        TileMapComponent* tiles = map();
        if (!tiles)
            return;
        if (item == addItem)
        {
            char name[32];
            std::snprintf(name, sizeof name, "terrain_%d", tiles->terrainCount() + 1);
            mTerrain = tiles->addTerrain(name);
            syncTerrains();
            edited.emit("New terrain");
        }
        else if (item == removeItem && mTerrain >= 0 && mTerrain < tiles->terrainCount())
        {
            tiles->removeTerrain(mTerrain);
            mTerrain = mTerrain > 0 ? mTerrain - 1 : 0;
            syncTerrains();
            edited.emit("Delete terrain");
        }
    });

    mPalette = mContent->createChild<TilePalette>();
    mPalette->setStretch(1.0f);
    mPalette->terrainClicked.connect([this](int tile, int across, int down) { markTerrainTile(tile, across, down); });
    mPalette->picked.connect([this]()
    {
        // Picking tiles means painting them.
        if (mTool == TileTool::Eraser || mTool == TileTool::Picker)
            mTools->toggleChanged.emit(mToolItems[0], true);
        syncInfo();
    });

    show(nullptr);
}

TileMapComponent* TilePainterPanel::map() const
{
    return mObject ? mObject->getComponent<TileMapComponent>() : nullptr;
}

bool TilePainterPanel::painting() const
{
    return isVisible() && map() != nullptr;
}

void TilePainterPanel::show(GameObject* object)
{
    mObject = object;
    TileMapComponent* tiles = map();
    mEmpty->setVisible(!tiles);
    mEmpty->setText(object ? "This object has no TileMap" : "Select an object to paint its TileMap");
    mAddMap->setVisible(object && !tiles);
    mContent->setVisible(tiles != nullptr);
    mPalette->setMap(tiles);
    mStroke = false;
    syncTerrains();
    syncInfo();
}

void TilePainterPanel::syncTerrains()
{
    TileMapComponent* tiles = map();
    const int count = tiles ? tiles->terrainCount() : 0;
    if (mTerrain >= count)
        mTerrain = count - 1;
    if (mTerrain < 0 && count > 0)
        mTerrain = 0;
    mSyncingTerrains = true;
    mTerrains->clear();
    for (int t = 0; t < count; ++t)
        mTerrains->addItem(tiles->terrainName(t));
    mTerrains->setSelectedIndex(mTerrain);
    mSyncingTerrains = false;
    mTerrainName->setText(count > 0 ? tiles->terrainName(mTerrain) : ct::String());
    mTerrainName->setEnabled(count > 0);
    const bool editing = count > 0 && mTools->isItemChecked(mEditTerrainItem);
    mPalette->setTerrainEditing(editing ? mTerrain : -1);
}

// The middle puts the tile in the terrain or takes it out; a side or a corner
// turns that neighbour on or off.
void TilePainterPanel::markTerrainTile(int tile, int across, int down)
{
    TileMapComponent* tiles = map();
    if (!tiles || mTerrain < 0 || mTerrain >= tiles->terrainCount() || tile <= 0)
        return;
    static const unsigned char kParts[3][3] = {{TERRAIN_NW, TERRAIN_N, TERRAIN_NE},
                                               {TERRAIN_W, 0, TERRAIN_E},
                                               {TERRAIN_SW, TERRAIN_S, TERRAIN_SE}};
    int terrain = -1;
    unsigned char bits = 0;
    const bool member = tiles->tileTerrain(tile, terrain, bits) && terrain == mTerrain;
    if (across == 1 && down == 1)
        tiles->setTileTerrain(tile, member ? -1 : mTerrain, 0);
    else
        tiles->setTileTerrain(tile, mTerrain, static_cast<unsigned char>((member ? bits : 0) ^ kParts[down][across]));
    mPalette->markDirty();
    edited.emit("Edit terrain");
}

int TilePainterPanel::textureColumns() const
{
    TileMapComponent* tiles = map();
    if (!tiles || !tiles->texture() || tiles->cellWidth() <= 0.0f)
        return 0;
    const float step = tiles->cellWidth() + tiles->atlasGap().x;
    return static_cast<int>((tiles->texture()->Width() - tiles->atlasPadding().x + tiles->atlasGap().x) / step);
}

void TilePainterPanel::syncInfo()
{
    TileMapComponent* tiles = map();
    if (!tiles)
        return;
    char info[256];
    const int columns = textureColumns();
    // Tile numbers follow atlasTilesX: when it is not the texture's, what is
    // painted is not what the palette shows.
    const bool mismatch = columns > 0 && columns != tiles->atlasTilesX();
    std::snprintf(info, sizeof info, "%dx%d cells of %gx%g   brush %dx%d%s%s", tiles->columns(), tiles->rows(),
                  tiles->cellWidth(), tiles->cellHeight(), mPalette->blockWidth(), mPalette->blockHeight(),
                  mCollision ? "   collision" : "",
                  mismatch ? "   atlas columns differ from the texture: press the grid button" : "");
    mInfo->setText(info);
}

void TilePainterPanel::setView(const Camera2D& camera, float width, float height)
{
    mCamera = camera;
    mWidth = width > 1.0f ? width : 1.0f;
    mHeight = height > 1.0f ? height : 1.0f;
}

bool TilePainterPanel::cellAt(float x, float y, int& cx, int& cy) const
{
    TileMapComponent* tiles = map();
    if (!tiles || tiles->cellWidth() <= 0.0f || tiles->cellHeight() <= 0.0f)
        return false;
    const Math::Vec2 world = mCamera.ScreenToWorld(x, y, mWidth, mHeight);
    const Math::Vec2 local = mObject->globalTransform().AffineInverse().Transform(world);
    cx = static_cast<int>(std::floor(local.x / tiles->cellWidth()));
    cy = static_cast<int>(std::floor(local.y / tiles->cellHeight()));
    return true;
}

Math::Vec2 TilePainterPanel::cellCorner(float cx, float cy) const
{
    TileMapComponent* tiles = map();
    const Math::Vec2 world =
        mObject->globalTransform().Transform(Math::Vec2(cx * tiles->cellWidth(), cy * tiles->cellHeight()));
    return mCamera.CameraXform(mWidth, mHeight).AffineInverse().Transform(world);
}

int TilePainterPanel::stampTile(int dx, int dy) const
{
    const int tx = mPalette->blockX() + wrap(dx, mPalette->blockWidth());
    const int ty = mPalette->blockY() + wrap(dy, mPalette->blockHeight());
    const int columns = mPalette->tilesX();
    return columns > 0 ? ty * columns + tx + 1 : 0;
}

// One cell, as the tool and the mode say; the stamp repeats from the origin.
void TilePainterPanel::apply(int cx, int cy, int originX, int originY)
{
    TileMapComponent* tiles = map();
    if (cx < 0 || cy < 0 || cx >= tiles->columns() || cy >= tiles->rows())
        return;
    if (mCollision)
    {
        const bool solid = mTool != TileTool::Eraser;
        if (tiles->hasCollision(cx, cy) != solid)
        {
            tiles->setCollision(cx, cy, solid);
            mChanged = true;
        }
        return;
    }
    if (mTool == TileTool::Terrain || (mTool == TileTool::Eraser && tiles->terrainAt(cx, cy) >= 0))
    {
        const int terrain = mTool == TileTool::Terrain ? mTerrain : -1;
        if (mTool == TileTool::Terrain && (terrain < 0 || terrain >= tiles->terrainCount()))
            return;
        if (tiles->terrainAt(cx, cy) != terrain)
        {
            tiles->paintTerrain(cx, cy, terrain);
            mChanged = true;
        }
        return;
    }
    const int tile = mTool == TileTool::Eraser ? 0 : stampTile(cx - originX, cy - originY);
    if (tiles->getTile(cx, cy) != tile)
    {
        tiles->setTile(cx, cy, tile);
        mChanged = true;
    }
}

void TilePainterPanel::stamp(int cx, int cy, int originX, int originY)
{
    const bool block = mTool == TileTool::Brush && !mCollision;
    const int w = block ? mPalette->blockWidth() : 1, h = block ? mPalette->blockHeight() : 1;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            apply(cx + i, cy + j, originX, originY);
}

// Every cell like the one clicked and joined to it, side by side.
void TilePainterPanel::fill(int cx, int cy)
{
    TileMapComponent* tiles = map();
    const int columns = tiles->columns(), rows = tiles->rows();
    if (cx < 0 || cy < 0 || cx >= columns || cy >= rows)
        return;
    const bool collisionTarget = tiles->hasCollision(cx, cy);
    const int tileTarget = tiles->getTile(cx, cy);
    auto same = [&](int x, int y)
    {
        return mCollision ? tiles->hasCollision(x, y) == collisionTarget : tiles->getTile(x, y) == tileTarget;
    };
    ct::Vector<unsigned char> seen;
    seen.resize(static_cast<std::size_t>(columns) * rows, 0);
    ct::Vector<int> stack;
    stack.push_back(cy * columns + cx);
    seen[static_cast<std::size_t>(cy * columns + cx)] = 1;
    ct::Vector<int> cells;
    while (!stack.empty())
    {
        const int cell = stack.back();
        stack.pop_back();
        cells.push_back(cell);
        const int x = cell % columns, y = cell / columns;
        const int nx[4] = {x - 1, x + 1, x, x};
        const int ny[4] = {y, y, y - 1, y + 1};
        for (int k = 0; k < 4; ++k)
        {
            if (nx[k] < 0 || ny[k] < 0 || nx[k] >= columns || ny[k] >= rows)
                continue;
            const int next = ny[k] * columns + nx[k];
            if (!seen[static_cast<std::size_t>(next)] && same(nx[k], ny[k]))
            {
                seen[static_cast<std::size_t>(next)] = 1;
                stack.push_back(next);
            }
        }
    }
    // Changed only after the walk, so the walk sees the map as it was.
    for (int cell : cells)
        apply(cell % columns, cell / columns, cx, cy);
}

void TilePainterPanel::hover(float x, float y)
{
    mHovering = cellAt(x, y, mHoverX, mHoverY);
}

void TilePainterPanel::press(float x, float y)
{
    int cx = 0, cy = 0;
    if (!cellAt(x, y, cx, cy))
        return;
    mStroke = true;
    mChanged = false;
    mStartX = cx;
    mStartY = cy;
    mHoverX = cx;
    mHoverY = cy;
    mHovering = true;
    TileMapComponent* tiles = map();
    switch (mTool)
    {
    case TileTool::Brush:
    case TileTool::Eraser:
    case TileTool::Terrain:
        stamp(cx, cy, cx, cy);
        break;
    case TileTool::Fill:
        fill(cx, cy);
        break;
    case TileTool::Picker:
    {
        const int tile = tiles->getTile(cx, cy);
        const int columns = mPalette->tilesX();
        if (tile > 0 && columns > 0)
        {
            mPalette->pick((tile - 1) % columns, (tile - 1) / columns);
            mTools->toggleChanged.emit(mToolItems[0], true);
        }
        break;
    }
    case TileTool::Rectangle:
        break;
    }
}

void TilePainterPanel::drag(float x, float y)
{
    int cx = 0, cy = 0;
    if (!mStroke || !cellAt(x, y, cx, cy))
        return;
    const bool moved = cx != mHoverX || cy != mHoverY;
    mHoverX = cx;
    mHoverY = cy;
    // A brush stroke lays the stamp on every cell it crosses, keeping its pattern.
    if (moved && (mTool == TileTool::Brush || mTool == TileTool::Eraser || mTool == TileTool::Terrain))
        stamp(cx, cy, mStartX, mStartY);
}

bool TilePainterPanel::release()
{
    if (!mStroke)
        return false;
    mStroke = false;
    if (mTool == TileTool::Rectangle)
    {
        const int x0 = mStartX < mHoverX ? mStartX : mHoverX, x1 = mStartX < mHoverX ? mHoverX : mStartX;
        const int y0 = mStartY < mHoverY ? mStartY : mHoverY, y1 = mStartY < mHoverY ? mHoverY : mStartY;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                apply(x, y, x0, y0);
    }
    const bool changed = mChanged;
    mChanged = false;
    return changed;
}

void TilePainterPanel::paintOverlay(rt::PaintContext& ctx, const rt::Rect& area) const
{
    TileMapComponent* tiles = map();
    if (!tiles || !isVisible())
        return;
    auto box = [&](float x0, float y0, float x1, float y1, const rt::Color& color, float thickness)
    {
        const Math::Vec2 a = cellCorner(x0, y0), b = cellCorner(x1, y1);
        const float l = area.x + std::fmin(a.x, b.x), t = area.y + std::fmin(a.y, b.y);
        const float w = std::fabs(b.x - a.x), h = std::fabs(b.y - a.y);
        ctx.fill.SetColor(color);
        ctx.fillRect(l, t, w, thickness);
        ctx.fillRect(l, t + h - thickness, w, thickness);
        ctx.fillRect(l, t, thickness, h);
        ctx.fillRect(l + w - thickness, t, thickness, h);
    };
    box(0.0f, 0.0f, static_cast<float>(tiles->columns()), static_cast<float>(tiles->rows()),
        rt::Color(120, 200, 140, 200), 1.0f);

    if (mCollision)
    {
        ctx.fill.SetColor(rt::Color(230, 60, 60, 90));
        for (int y = 0; y < tiles->rows(); ++y)
            for (int x = 0; x < tiles->columns(); ++x)
                if (tiles->hasCollision(x, y))
                {
                    const Math::Vec2 a = cellCorner(static_cast<float>(x), static_cast<float>(y));
                    const Math::Vec2 b = cellCorner(static_cast<float>(x + 1), static_cast<float>(y + 1));
                    ctx.fillRect(area.x + std::fmin(a.x, b.x), area.y + std::fmin(a.y, b.y), std::fabs(b.x - a.x),
                                 std::fabs(b.y - a.y));
                }
    }

    if (!mHovering)
        return;
    if (mStroke && mTool == TileTool::Rectangle)
    {
        const int x0 = mStartX < mHoverX ? mStartX : mHoverX, x1 = mStartX < mHoverX ? mHoverX : mStartX;
        const int y0 = mStartY < mHoverY ? mStartY : mHoverY, y1 = mStartY < mHoverY ? mHoverY : mStartY;
        box(static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1 + 1), static_cast<float>(y1 + 1),
            rt::Color(255, 205, 65, 255), 2.0f);
        return;
    }
    // The stamp under the pointer, as it would be painted.
    const bool block = mTool == TileTool::Brush && !mCollision;
    const int w = block ? mPalette->blockWidth() : 1, h = block ? mPalette->blockHeight() : 1;
    const Texture* texture = tiles->texture();
    if (block && texture)
    {
        rt::TextureHandle handle;
        handle.value = texture->Id();
        const float tw = static_cast<float>(texture->Width()), th = static_cast<float>(texture->Height());
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const int tile = stampTile(i, j) - 1;
                const int columns = mPalette->tilesX();
                if (tile < 0 || columns <= 0)
                    continue;
                const float sx = tiles->atlasPadding().x + (tile % columns) * (tiles->cellWidth() + tiles->atlasGap().x);
                const float sy = tiles->atlasPadding().y + (tile / columns) * (tiles->cellHeight() + tiles->atlasGap().y);
                const Math::Vec2 a = cellCorner(static_cast<float>(mHoverX + i), static_cast<float>(mHoverY + j));
                const Math::Vec2 b = cellCorner(static_cast<float>(mHoverX + i + 1), static_cast<float>(mHoverY + j + 1));
                ctx.drawImage(handle,
                              rt::Rect{area.x + std::fmin(a.x, b.x), area.y + std::fmin(a.y, b.y), std::fabs(b.x - a.x),
                                       std::fabs(b.y - a.y)},
                              rt::Rect{sx / tw, sy / th, tiles->cellWidth() / tw, tiles->cellHeight() / th});
            }
    }
    const rt::Color color = mTool == TileTool::Eraser ? rt::Color(230, 80, 80, 255) : rt::Color(255, 205, 65, 255);
    box(static_cast<float>(mHoverX), static_cast<float>(mHoverY), static_cast<float>(mHoverX + w),
        static_cast<float>(mHoverY + h), color, 2.0f);
}

} // namespace k2d::editor
