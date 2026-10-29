#include "TilePalette.h"

#include "Theme.hpp"

#include <k2d/Texture.h>
#include <k2d/TileMapComponent.h>

#include <cmath>

namespace rt = ig::retained;

namespace k2d::editor
{

TilePalette::TilePalette()
{
    setTabStop(false);
}

void TilePalette::setMap(const TileMapComponent* map)
{
    if (map != mMap)
    {
        mFitPending = true;
        mX0 = mY0 = mX1 = mY1 = 0;
    }
    mMap = map;
    markDirty();
}

int TilePalette::tilesX() const
{
    if (!mMap || !mMap->texture() || mMap->cellWidth() <= 0.0f)
        return 0;
    if (mMap->atlasTilesX() > 0)
        return mMap->atlasTilesX();
    const float step = mMap->cellWidth() + mMap->atlasGap().x;
    return static_cast<int>((mMap->texture()->Width() - mMap->atlasPadding().x + mMap->atlasGap().x) / step);
}

int TilePalette::tilesY() const
{
    if (!mMap || !mMap->texture() || mMap->cellHeight() <= 0.0f)
        return 0;
    const float step = mMap->cellHeight() + mMap->atlasGap().y;
    return static_cast<int>((mMap->texture()->Height() - mMap->atlasPadding().y + mMap->atlasGap().y) / step);
}

void TilePalette::setTerrainEditing(int terrain)
{
    mTerrain = terrain;
    markDirty();
}

void TilePalette::setSolidEditing(bool editing)
{
    mSolid = editing;
    markDirty();
}

bool TilePalette::tilePart(float localX, float localY, int& tx, int& ty, int& part) const
{
    if (!tileAt(localX, localY, tx, ty))
        return false;
    const float stepX = mMap->cellWidth() + mMap->atlasGap().x, stepY = mMap->cellHeight() + mMap->atlasGap().y;
    const float px = (localX - mPanX) / mZoom - mMap->atlasPadding().x - tx * stepX;
    const float py = (localY - mPanY) / mZoom - mMap->atlasPadding().y - ty * stepY;
    if (px < 0.0f || py < 0.0f || px >= mMap->cellWidth() || py >= mMap->cellHeight())
        return false;
    const int sx = static_cast<int>(px * 3.0f / mMap->cellWidth());
    const int sy = static_cast<int>(py * 3.0f / mMap->cellHeight());
    part = (sy > 2 ? 2 : sy) * 3 + (sx > 2 ? 2 : sx);
    return true;
}

void TilePalette::pick(int tileX, int tileY)
{
    mX0 = mX1 = tileX;
    mY0 = mY1 = tileY;
    markDirty();
}

void TilePalette::fit()
{
    const Texture* texture = mMap ? mMap->texture() : nullptr;
    if (!texture || rect_.w <= 1.0f || rect_.h <= 1.0f)
        return;
    mFitPending = false;
    const float zoom = std::fmin((rect_.w - 16.0f) / texture->Width(), (rect_.h - 16.0f) / texture->Height());
    mZoom = zoom >= 1.0f ? std::floor(zoom) : std::fmax(0.1f, zoom);
    mPanX = 8.0f;
    mPanY = 8.0f;
}

bool TilePalette::tileAt(float localX, float localY, int& tx, int& ty) const
{
    if (tilesX() <= 0 || tilesY() <= 0)
        return false;
    const float px = (localX - mPanX) / mZoom - mMap->atlasPadding().x;
    const float py = (localY - mPanY) / mZoom - mMap->atlasPadding().y;
    tx = static_cast<int>(std::floor(px / (mMap->cellWidth() + mMap->atlasGap().x)));
    ty = static_cast<int>(std::floor(py / (mMap->cellHeight() + mMap->atlasGap().y)));
    tx = tx < 0 ? 0 : (tx >= tilesX() ? tilesX() - 1 : tx);
    ty = ty < 0 ? 0 : (ty >= tilesY() ? tilesY() - 1 : ty);
    return true;
}

void TilePalette::paint(rt::PaintContext& ctx)
{
    if (!visible_)
        return;
    const rt::Rect abs = absoluteRect();
    if (ctx.isClipped(abs))
        return;
    if (mFitPending)
        fit();
    ctx.pushClip(abs);
    ctx.fill.SetColor(rt::Color(28, 30, 34, 255));
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
    const Texture* texture = mMap ? mMap->texture() : nullptr;
    const auto& theme = rt::Theme::instance();
    if (!texture)
    {
        ctx.font.SetFontSize(theme.fontSize);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(theme.textDisabled);
        ctx.font.Print("Give the TileMap a texture: drop an image on its texture field", abs.x + 12.0f,
                       abs.y + 24.0f);
        ctx.popClip();
        return;
    }
    const float x = abs.x + mPanX, y = abs.y + mPanY;
    const float w = texture->Width() * mZoom, h = texture->Height() * mZoom;
    ctx.fill.SetColor(rt::Color(60, 62, 68, 255));
    ctx.fillRect(x, y, w, h);
    rt::TextureHandle handle;
    handle.value = texture->Id();
    ctx.drawImage(handle, rt::Rect{x, y, w, h}, rt::Rect{0.0f, 0.0f, 1.0f, 1.0f});

    // The tile grid.
    const float cw = mMap->cellWidth() * mZoom, ch = mMap->cellHeight() * mZoom;
    const float gx = mMap->atlasGap().x * mZoom, gy = mMap->atlasGap().y * mZoom;
    const float ox = x + mMap->atlasPadding().x * mZoom, oy = y + mMap->atlasPadding().y * mZoom;
    const int columns = tilesX(), rows = tilesY();
    if (cw >= 4.0f && ch >= 4.0f)
    {
        ctx.fill.SetColor(rt::Color(0, 0, 0, 90));
        for (int c = 0; c <= columns; ++c)
            ctx.fillRect(ox + c * (cw + gx) - (c == columns ? gx : 0.0f), oy, 1.0f, rows * (ch + gy) - gy);
        for (int r = 0; r <= rows; ++r)
            ctx.fillRect(ox, oy + r * (ch + gy) - (r == rows ? gy : 0.0f), columns * (cw + gx) - gx, 1.0f);
    }

    // The terrain's tiles: their middle, and the neighbours each joins.
    if (mTerrain >= 0 && columns > 0)
    {
        static const unsigned char kParts[9] = {TERRAIN_NW, TERRAIN_N, TERRAIN_NE, TERRAIN_W, 0,
                                                TERRAIN_E,  TERRAIN_SW, TERRAIN_S, TERRAIN_SE};
        const float pw = cw / 3.0f, ph = ch / 3.0f;
        for (const TerrainTile& entry : mMap->terrainTiles())
        {
            if (entry.terrain != mTerrain || entry.tile <= 0)
                continue;
            const int tx = (entry.tile - 1) % columns, ty = (entry.tile - 1) / columns;
            const float tileX = ox + tx * (cw + gx), tileY = oy + ty * (ch + gy);
            for (int part = 0; part < 9; ++part)
                if (part == 4 || (entry.bits & kParts[part]))
                {
                    ctx.fill.SetColor(part == 4 ? rt::Color(80, 220, 120, 170) : rt::Color(80, 160, 255, 150));
                    ctx.fillRect(tileX + (part % 3) * pw + 1.0f, tileY + (part / 3) * ph + 1.0f, pw - 2.0f,
                                 ph - 2.0f);
                }
        }
    }

    // Solid tiles in red; animated ones marked in a corner.
    if (columns > 0)
    {
        const TileSet& tiles = mMap->tileSet();
        if (mSolid)
            for (int tile : tiles.solidTiles)
            {
                const int tx = (tile - 1) % columns, ty = (tile - 1) / columns;
                ctx.fill.SetColor(rt::Color(230, 60, 60, 120));
                ctx.fillRect(ox + tx * (cw + gx), oy + ty * (ch + gy), cw, ch);
            }
        for (const TileAnimation& animation : tiles.animations)
        {
            const int tx = (animation.tile - 1) % columns, ty = (animation.tile - 1) / columns;
            const float size = cw < 12.0f ? cw * 0.5f : 6.0f;
            ctx.fill.SetColor(rt::Color(80, 200, 255, 230));
            ctx.fillTriangle(ox + tx * (cw + gx) + 2.0f, oy + ty * (ch + gy) + 2.0f, ox + tx * (cw + gx) + 2.0f,
                             oy + ty * (ch + gy) + 2.0f + size * 2.0f, ox + tx * (cw + gx) + 2.0f + size * 1.6f,
                             oy + ty * (ch + gy) + 2.0f + size);
        }
    }

    // The picked block.
    if (mTerrain < 0 && !mSolid && columns > 0 && rows > 0)
    {
        const float bx = ox + blockX() * (cw + gx), by = oy + blockY() * (ch + gy);
        const float bw = blockWidth() * (cw + gx) - gx, bh = blockHeight() * (ch + gy) - gy;
        ctx.fill.SetColor(rt::Color(255, 205, 65, 50));
        ctx.fillRect(bx, by, bw, bh);
        ctx.fill.SetColor(rt::Color(255, 205, 65, 255));
        ctx.fillRect(bx, by, bw, 2.0f);
        ctx.fillRect(bx, by + bh - 2.0f, bw, 2.0f);
        ctx.fillRect(bx, by, 2.0f, bh);
        ctx.fillRect(bx + bw - 2.0f, by, 2.0f, bh);
    }
    ctx.popClip();
}

void TilePalette::onMousePress(rt::MouseEvent& e)
{
    rt::Widget::onMousePress(e);
    e.consumed = true;
    if (e.button != 0)
    {
        mPanning = true;
        return;
    }
    int tx = 0, ty = 0;
    if (mSolid)
    {
        if (tileAt(e.localX, e.localY, tx, ty))
            solidClicked.emit(ty * tilesX() + tx + 1);
        return;
    }
    if (mTerrain >= 0)
    {
        int part = 0;
        if (tilePart(e.localX, e.localY, tx, ty, part))
            terrainClicked.emit(ty * tilesX() + tx + 1, part % 3, part / 3);
        return;
    }
    if (!tileAt(e.localX, e.localY, tx, ty))
        return;
    mPicking = true;
    pick(tx, ty);
}

void TilePalette::onMouseMove(rt::MouseEvent& e)
{
    rt::Widget::onMouseMove(e);
    if (mPanning)
    {
        mPanX += e.relX;
        mPanY += e.relY;
        markDirty();
        return;
    }
    int tx = 0, ty = 0;
    if (mPicking && tileAt(e.localX, e.localY, tx, ty) && (tx != mX1 || ty != mY1))
    {
        mX1 = tx;
        mY1 = ty;
        markDirty();
    }
}

void TilePalette::onMouseRelease(rt::MouseEvent& e)
{
    rt::Widget::onMouseRelease(e);
    if (mPicking)
        picked.emit();
    mPicking = false;
    mPanning = false;
    e.consumed = true;
}

void TilePalette::onMouseScroll(rt::MouseEvent& e)
{
    e.consumed = true;
    const float px = (e.localX - mPanX) / mZoom, py = (e.localY - mPanY) / mZoom;
    mZoom = std::fmax(0.1f, std::fmin(32.0f, mZoom * std::pow(1.2f, e.scrollY)));
    mPanX = e.localX - px * mZoom;
    mPanY = e.localY - py * mZoom;
    markDirty();
}

} // namespace k2d::editor
