#include <k2d/AStar2D.h>
#include <k2d/AStarGrid2D.h>
#include <k2d/TileMapComponent.h>

#include <cstdio>
#include <cmath>

namespace
{
    bool Near(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }

    bool TestGridDiagonalAlways()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);
        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Always);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 0}, {4, 4}, path);

        return ok && path.size() == 5 && path[0] == glm::ivec2(0, 0) &&
               path[4] == glm::ivec2(4, 4);
    }

    bool TestGridDiagonalNever()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);
        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Never);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 0}, {4, 4}, path);

        return ok && path.size() == 9;
    }

    bool TestGridWallDetour()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);
        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Never);

        for (int y = 0; y <= 3; ++y)
            grid.SetSolid(2, y, true);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 2}, {4, 2}, path);
        if (!ok)
            return false;
        for (size_t i = 0; i < path.size(); ++i)
            if (grid.IsSolid(path[i].x, path[i].y))
                return false; 

        return path.size() > 5;
    }

    bool TestGridUnreachablePartial()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);

        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Never);
        grid.SetSolid(1, 2, true);
        grid.SetSolid(3, 2, true);
        grid.SetSolid(2, 1, true);
        grid.SetSolid(2, 3, true);

        ct::Vector<glm::ivec2> path;
        bool failsClosed = !grid.GetIdPath({0, 0}, {2, 2}, path, false);

        ct::Vector<glm::ivec2> partial;
        bool partialOk = grid.GetIdPath({0, 0}, {2, 2}, partial, true);
        bool endsShortOfGoal = partialOk && !partial.empty() && partial.back() != glm::ivec2(2, 2);

        return failsClosed && endsShortOfGoal;
    }

    bool TestGridOnlyIfNoObstacles()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(3, 3);
        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::OnlyIfNoObstacles);

        grid.SetSolid(1, 0, true);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 0}, {1, 1}, path);

        return ok && path.size() > 2;
    }

    bool TestTileMapBlankCellsAreWalkable()
    {
        k2d::TileMapComponent map;
        map.setCellSize(16.0f, 16.0f);
        map.setMapSize(5, 5);

        k2d::AStarGrid2D grid;
        map.buildPathfindingGrid(grid);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 0}, {4, 4}, path);
        return ok && !grid.IsSolid(2, 2);
    }

    bool TestTileMapWallTileForcesDetour()
    {
        k2d::TileMapComponent map;
        map.setCellSize(16.0f, 16.0f);
        map.setMapSize(5, 5);
        map.setAtlasTilesX(4);

        for (int y = 0; y <= 3; ++y)
            map.setTile(2, y, 2);

        map.setTile(0, 0, 5);

        k2d::AStarGrid2D grid;
        const int solidIds[] = {2};
        map.buildPathfindingGrid(grid, solidIds, 1);

        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Never);

        if (grid.IsSolid(0, 0) || !grid.IsSolid(2, 0) || !grid.IsSolid(2, 3) || grid.IsSolid(2, 4))
            return false;

        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 2}, {4, 2}, path);
        if (!ok)
            return false;
        for (size_t i = 0; i < path.size(); ++i)
            if (grid.IsSolid(path[i].x, path[i].y))
                return false;
        return path.size() > 5; 
    }

    bool TestTileMapGridPositionsMatchCellSize()
    {
        k2d::TileMapComponent map;
        map.setCellSize(20.0f, 10.0f);
        map.setMapSize(3, 3);
        k2d::AStarGrid2D grid;
        map.buildPathfindingGrid(grid);

        glm::vec2 p = grid.GetPointPosition(1, 1);
        return Near(p.x, 30.0f) && Near(p.y, 15.0f);
    }

    bool TestPointGraphDirectDiagonal()
    {
        k2d::AStar2D astar;
        astar.AddPoint(0, {0.0f, 0.0f});
        astar.AddPoint(1, {10.0f, 0.0f});
        astar.AddPoint(2, {10.0f, 10.0f});
        astar.AddPoint(3, {0.0f, 10.0f});
        astar.ConnectPoints(0, 1);
        astar.ConnectPoints(1, 2);
        astar.ConnectPoints(2, 3);
        astar.ConnectPoints(3, 0);
        astar.ConnectPoints(0, 2); 

        ct::Vector<int> path;
        bool ok = astar.GetIdPath(0, 2, path);
        return ok && path.size() == 2 && path[0] == 0 && path[1] == 2;
    }

    bool TestPointGraphDetourWithoutShortcut()
    {
        k2d::AStar2D astar;
        astar.AddPoint(0, {0.0f, 0.0f});
        astar.AddPoint(1, {10.0f, 0.0f});
        astar.AddPoint(2, {10.0f, 10.0f});
        astar.AddPoint(3, {0.0f, 10.0f});
        astar.ConnectPoints(0, 1);
        astar.ConnectPoints(1, 2);
        astar.ConnectPoints(2, 3);
        astar.ConnectPoints(3, 0);

        ct::Vector<int> path;
        bool ok = astar.GetIdPath(0, 2, path);
        return ok && path.size() == 3 && path[0] == 0 && path[2] == 2;
    }

    bool TestPointGraphDisabledPointForcesDetour()
    {
        k2d::AStar2D astar;
        astar.AddPoint(0, {0.0f, 0.0f});
        astar.AddPoint(1, {10.0f, 0.0f});
        astar.AddPoint(2, {10.0f, 10.0f});
        astar.AddPoint(3, {0.0f, 10.0f});
        astar.ConnectPoints(0, 1);
        astar.ConnectPoints(1, 2);
        astar.ConnectPoints(2, 3);
        astar.ConnectPoints(3, 0);
        astar.SetPointDisabled(1, true);

        ct::Vector<int> path;
        bool ok = astar.GetIdPath(0, 2, path);

        return ok && path.size() == 3 && path[1] == 3;
    }

    bool TestPointGraphClosestPoint()
    {
        k2d::AStar2D astar;
        astar.AddPoint(0, {0.0f, 0.0f});
        astar.AddPoint(1, {10.0f, 0.0f});
        astar.AddPoint(2, {10.0f, 10.0f});
        int closest = astar.GetClosestPoint({9.0f, 9.0f});
        return closest == 2;
    }

    bool TestPointGraphPartialPathToIsolatedPoint()
    {
        k2d::AStar2D astar;
        astar.AddPoint(0, {0.0f, 0.0f});
        astar.AddPoint(1, {10.0f, 0.0f});
        astar.AddPoint(2, {20.0f, 0.0f}); 
        astar.ConnectPoints(0, 1);

        ct::Vector<int> path;
        bool failsClosed = !astar.GetIdPath(0, 2, path, false);

        ct::Vector<int> partial;
        bool partialOk = astar.GetIdPath(0, 2, partial, true);
        bool endsAtClosest = partialOk && !partial.empty() && partial.back() == 1;

        return failsClosed && endsAtClosest;
    }
}

int main()
{
    bool diagAlways = TestGridDiagonalAlways();
    bool diagNever = TestGridDiagonalNever();
    bool wallDetour = TestGridWallDetour();
    bool unreachablePartial = TestGridUnreachablePartial();
    bool onlyIfNoObstacles = TestGridOnlyIfNoObstacles();

    bool tileMapBlankWalkable = TestTileMapBlankCellsAreWalkable();
    bool tileMapWallDetour = TestTileMapWallTileForcesDetour();
    bool tileMapPositions = TestTileMapGridPositionsMatchCellSize();

    bool directDiagonal = TestPointGraphDirectDiagonal();
    bool detourNoShortcut = TestPointGraphDetourWithoutShortcut();
    bool disabledDetour = TestPointGraphDisabledPointForcesDetour();
    bool closestPoint = TestPointGraphClosestPoint();
    bool partialIsolated = TestPointGraphPartialPathToIsolatedPoint();

    std::printf("AStarGrid2D: diag_always=%s diag_never=%s wall_detour=%s "
                "unreachable_partial=%s only_if_no_obstacles=%s\n",
                diagAlways ? "pass" : "fail", diagNever ? "pass" : "fail",
                wallDetour ? "pass" : "fail", unreachablePartial ? "pass" : "fail",
                onlyIfNoObstacles ? "pass" : "fail");
    std::printf("TileMapComponent->AStarGrid2D: blank_walkable=%s wall_tile_detour=%s "
                "grid_positions=%s\n",
                tileMapBlankWalkable ? "pass" : "fail", tileMapWallDetour ? "pass" : "fail",
                tileMapPositions ? "pass" : "fail");
    std::printf("AStar2D: direct_diagonal=%s detour_no_shortcut=%s disabled_detour=%s "
                "closest_point=%s partial_isolated=%s\n",
                directDiagonal ? "pass" : "fail", detourNoShortcut ? "pass" : "fail",
                disabledDetour ? "pass" : "fail", closestPoint ? "pass" : "fail",
                partialIsolated ? "pass" : "fail");

    bool allPass = diagAlways && diagNever && wallDetour && unreachablePartial &&
                   onlyIfNoObstacles && tileMapBlankWalkable && tileMapWallDetour &&
                   tileMapPositions && directDiagonal && detourNoShortcut &&
                   disabledDetour && closestPoint && partialIsolated;
    return allPass ? 0 : 1;
}