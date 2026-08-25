#include "TileMapPanel.h"

#include "core/EditorApplication.h"

#include <k2d/GameObject.h>
#include <k2d/Texture.h>
#include <k2d/TileMapComponent.h>

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>

namespace k2d::editor
{

namespace
{
struct Cell
{
    int x;
    int y;
};

bool validCell(const TileMapComponent &map, int x, int y)
{
    return x >= 0 && x < map.columns() && y >= 0 && y < map.rows();
}

void paintCell(TileMapComponent &map, int x, int y, bool collision, int tile, bool solid)
{
    if (!validCell(map, x, y))
        return;
    if (collision)
        map.setCollision(x, y, solid);
    else
        map.setTile(x, y, tile);
}

void fillCells(TileMapComponent &map, int startX, int startY, bool collision, int tile, bool solid)
{
    if (!validCell(map, startX, startY))
        return;
    const int targetTile = map.getTile(startX, startY);
    const bool targetSolid = map.hasCollision(startX, startY);
    if ((collision && targetSolid == solid) || (!collision && targetTile == tile))
        return;

    std::vector<Cell> pending;
    pending.push_back({startX, startY});
    while (!pending.empty())
    {
        const Cell cell = pending.back();
        pending.pop_back();
        if (!validCell(map, cell.x, cell.y))
            continue;
        if (collision ? map.hasCollision(cell.x, cell.y) != targetSolid
                      : map.getTile(cell.x, cell.y) != targetTile)
            continue;
        paintCell(map, cell.x, cell.y, collision, tile, solid);
        pending.push_back({cell.x - 1, cell.y});
        pending.push_back({cell.x + 1, cell.y});
        pending.push_back({cell.x, cell.y - 1});
        pending.push_back({cell.x, cell.y + 1});
    }
}

void paintRectangle(TileMapComponent &map, int x0, int y0, int x1, int y1,
                    bool collision, int tile, bool solid)
{
    const int left = std::max(0, std::min(x0, x1));
    const int right = std::min(map.columns() - 1, std::max(x0, x1));
    const int top = std::max(0, std::min(y0, y1));
    const int bottom = std::min(map.rows() - 1, std::max(y0, y1));
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x)
            paintCell(map, x, y, collision, tile, solid);
}
}

TileMapPanel::TileMapPanel(EditorApplication &application) : EditorPanel("Tile Painter", application)
{
}

void TileMapPanel::drawContents()
{
    GameObject *object = app().selection().resolve(app().scene());
    TileMapComponent *map = object ? object->getComponent<TileMapComponent>() : nullptr;
    if (!map)
    {
        ImGui::TextDisabled("Select an object with a TileMap component.");
        return;
    }

    Texture *texture = map->texture();
    if (!texture || texture->Width() <= 0 || texture->Height() <= 0)
    {
        ImGui::TextDisabled("Assign a TileMap texture in the Inspector first.");
        return;
    }

    const float cellW = map->cellWidth();
    const float cellH = map->cellHeight();
    const int atlasColumns = map->atlasTilesX();
    const Math::Vec2 atlasPadding = map->atlasPadding();
    const Math::Vec2 atlasGap = map->atlasGap();
    const float atlasStepX = cellW + atlasGap.x;
    const float atlasStepY = cellH + atlasGap.y;
    const int atlasRows = std::max(1, static_cast<int>(
        (texture->Height() - atlasPadding.y * 2.0f + atlasGap.y) / atlasStepY));
    if (cellW <= 0.0f || cellH <= 0.0f || atlasColumns <= 0)
        return;

    ImGui::Text("%s  |  %d x %d cells", object->name().c_str(), map->columns(), map->rows());
    ImGui::SameLine();
    ImGui::Checkbox("Collision brush", &mCollisionBrush);
    ImGui::SameLine();
    if (ImGui::RadioButton("Brush", mTool == Tool::Brush))
        mTool = Tool::Brush;
    ImGui::SameLine();
    if (ImGui::RadioButton("Pick", mTool == Tool::Pick))
        mTool = Tool::Pick;
    ImGui::SameLine();
    if (ImGui::RadioButton("Fill", mTool == Tool::Fill))
        mTool = Tool::Fill;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rect", mTool == Tool::Rectangle))
        mTool = Tool::Rectangle;

    if (ImGui::SmallButton("Empty"))
        mSelectedTile = 0;
    ImGui::SameLine();
    ImGui::TextDisabled("Tile: %d", mSelectedTile);
    ImGui::SliderFloat("Atlas zoom", &mAtlasZoom, 0.25f, 3.0f, "%.2fx");
    ImGui::SliderFloat("Map zoom", &mMapZoom, 0.25f, 3.0f, "%.2fx");
    Math::Vec2 paddingEdit = atlasPadding;
    if (ImGui::DragFloat2("Atlas Padding", &paddingEdit.x, 0.5f, 0.0f, 4096.0f))
    {
        const EditorApplication::SceneChange before = app().beginChange();
        map->setAtlasPadding(paddingEdit.x, paddingEdit.y);
        app().commitChange("Set TileMap Atlas Padding", before);
    }
    Math::Vec2 gapEdit = atlasGap;
    if (ImGui::DragFloat2("Atlas Gap", &gapEdit.x, 0.5f, 0.0f, 4096.0f))
    {
        const EditorApplication::SceneChange before = app().beginChange();
        map->setAtlasGap(gapEdit.x, gapEdit.y);
        app().commitChange("Set TileMap Atlas Gap", before);
    }
    ImGui::TextDisabled("Left paints; right erases. Pick reads a tile; fill affects connected cells.");
    ImGui::SeparatorText("Atlas");

    const ImVec2 atlasSize(texture->Width() * mAtlasZoom, texture->Height() * mAtlasZoom);
    const ImVec2 atlasOrigin = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture->Id())), atlasSize);
    const bool atlasHovered = ImGui::IsItemHovered();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    for (int y = 0; y < atlasRows; ++y)
        for (int x = 0; x < atlasColumns; ++x)
            draw->AddRect(ImVec2(atlasOrigin.x + (atlasPadding.x + x * atlasStepX) * mAtlasZoom,
                                 atlasOrigin.y + (atlasPadding.y + y * atlasStepY) * mAtlasZoom),
                          ImVec2(atlasOrigin.x + (atlasPadding.x + x * atlasStepX + cellW) * mAtlasZoom,
                                 atlasOrigin.y + (atlasPadding.y + y * atlasStepY + cellH) * mAtlasZoom),
                          IM_COL32(255, 255, 255, 70));

    if (atlasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float localX = (mouse.x - atlasOrigin.x) / mAtlasZoom - atlasPadding.x;
        const float localY = (mouse.y - atlasOrigin.y) / mAtlasZoom - atlasPadding.y;
        const int x = static_cast<int>(std::floor(localX / atlasStepX));
        const int y = static_cast<int>(std::floor(localY / atlasStepY));
        const bool insideTile = x >= 0 && x < atlasColumns && y >= 0 && y < atlasRows &&
                                localX - x * atlasStepX < cellW && localY - y * atlasStepY < cellH;
        if (insideTile)
            mSelectedTile = y * atlasColumns + x + 1;
    }
    if (mSelectedTile > 0)
    {
        const int index = mSelectedTile - 1;
        const int x = index % atlasColumns;
        const int y = index / atlasColumns;
        draw->AddRect(ImVec2(atlasOrigin.x + (atlasPadding.x + x * atlasStepX) * mAtlasZoom,
                             atlasOrigin.y + (atlasPadding.y + y * atlasStepY) * mAtlasZoom),
                      ImVec2(atlasOrigin.x + (atlasPadding.x + x * atlasStepX + cellW) * mAtlasZoom,
                             atlasOrigin.y + (atlasPadding.y + y * atlasStepY + cellH) * mAtlasZoom),
                      IM_COL32(255, 202, 52, 255), 0.0f, 0, 2.0f);
    }

    ImGui::SeparatorText("Map");
    ImGui::BeginChild("##tilemap_canvas", ImVec2(0.0f, 0.0f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    const float drawCellW = cellW * mMapZoom;
    const float drawCellH = cellH * mMapZoom;
    const ImVec2 mapOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 mapSize(map->columns() * drawCellW, map->rows() * drawCellH);
    draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(mapOrigin, ImVec2(mapOrigin.x + mapSize.x, mapOrigin.y + mapSize.y),
                        IM_COL32(20, 24, 31, 255));

    const ImTextureID textureId = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture->Id()));
    for (int y = 0; y < map->rows(); ++y)
    {
        for (int x = 0; x < map->columns(); ++x)
        {
            const ImVec2 min(mapOrigin.x + x * drawCellW, mapOrigin.y + y * drawCellH);
            const ImVec2 max(min.x + drawCellW, min.y + drawCellH);
            const int tile = map->getTile(x, y);
            if (tile > 0)
            {
                const int tileIndex = tile - 1;
                const int tx = tileIndex % atlasColumns;
                const int ty = tileIndex / atlasColumns;
                const float sourceX = atlasPadding.x + tx * atlasStepX;
                const float sourceY = atlasPadding.y + ty * atlasStepY;
                draw->AddImage(textureId, min, max,
                               ImVec2(sourceX / texture->Width(), sourceY / texture->Height()),
                               ImVec2((sourceX + cellW) / texture->Width(),
                                      (sourceY + cellH) / texture->Height()));
            }
            if (map->hasCollision(x, y))
                draw->AddRectFilled(min, max, IM_COL32(230, 68, 55, 92));
            draw->AddRect(min, max, IM_COL32(255, 255, 255, 36));
        }
    }

    ImGui::SetCursorScreenPos(mapOrigin);
    ImGui::InvisibleButton("##tilemap_paint", mapSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const int cellX = static_cast<int>(std::floor((mouse.x - mapOrigin.x) / drawCellW));
    const int cellY = static_cast<int>(std::floor((mouse.y - mapOrigin.y) / drawCellH));
    const bool erase = rightDown;
    const int paintTile = erase ? 0 : mSelectedTile;
    const bool paintSolid = !erase;

    if (hovered && mTool == Tool::Pick && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && validCell(*map, cellX, cellY))
    {
        if (!mCollisionBrush)
            mSelectedTile = map->getTile(cellX, cellY);
        mTool = Tool::Brush;
    }
    else if (hovered && (leftDown || rightDown) && mTool != Tool::Pick)
    {
        if (!mPainting)
        {
            const EditorApplication::SceneChange before = app().beginChange();
            const char *label = mCollisionBrush ? "Paint TileMap Collision" : "Paint TileMap";
            app().beginTransaction(label, before);
            mPainting = true;
            mActionApplied = false;
            mRectStartX = mRectEndX = cellX;
            mRectStartY = mRectEndY = cellY;
            mRectangleErase = erase;
        }
        if (mTool == Tool::Brush)
            paintCell(*map, cellX, cellY, mCollisionBrush, paintTile, paintSolid);
        else if (mTool == Tool::Fill && !mActionApplied)
        {
            fillCells(*map, cellX, cellY, mCollisionBrush, paintTile, paintSolid);
            mActionApplied = true;
        }
        else if (mTool == Tool::Rectangle)
        {
            mRectEndX = cellX;
            mRectEndY = cellY;
        }
    }
    if (mPainting && !leftDown && !rightDown)
    {
        if (mTool == Tool::Rectangle)
            paintRectangle(*map, mRectStartX, mRectStartY, mRectEndX, mRectEndY,
                           mCollisionBrush, mRectangleErase ? 0 : mSelectedTile, !mRectangleErase);
        app().commitTransaction();
        mPainting = false;
        mActionApplied = false;
    }
    if (mPainting && mTool == Tool::Rectangle)
    {
        const int left = std::min(mRectStartX, mRectEndX);
        const int right = std::max(mRectStartX, mRectEndX);
        const int top = std::min(mRectStartY, mRectEndY);
        const int bottom = std::max(mRectStartY, mRectEndY);
        draw->AddRect(ImVec2(mapOrigin.x + left * drawCellW, mapOrigin.y + top * drawCellH),
                      ImVec2(mapOrigin.x + (right + 1) * drawCellW,
                             mapOrigin.y + (bottom + 1) * drawCellH),
                      mRectangleErase ? IM_COL32(244, 94, 82, 255) : IM_COL32(255, 202, 52, 255),
                      0.0f, 0, 2.0f);
    }
    ImGui::EndChild();
}

}
