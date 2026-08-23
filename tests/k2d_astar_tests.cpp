#include <k2d/AStar2D.h>
#include <k2d/AStarGrid2D.h>
#include <k2d/TileMapComponent.h>

#include <cstdio>
#include <cmath>

namespace
{
    bool Near(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }

    // --- AStarGrid2D ---------------------------------------------------

    bool TestGridDiagonalAlways()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);
        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Always);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 0}, {4, 4}, path);
        // Open grid, diagonals allowed: corner-to-corner is 4 diagonal steps
        // -> 5 points on the path.
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
        // Cardinal-only: Manhattan distance 8 -> 9 points on the path.
        return ok && path.size() == 9;
    }

    bool TestGridWallDetour()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);
        grid.SetDiagonalMode(k2d::AStarGrid2D::DiagonalMode::Never);
        // Solid wall column at x=2, rows 0..3 -- only row 4 is open, forcing
        // a detour down and back up.
        for (int y = 0; y <= 3; ++y)
            grid.SetSolid(2, y, true);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 2}, {4, 2}, path);
        if (!ok)
            return false;
        for (size_t i = 0; i < path.size(); ++i)
            if (grid.IsSolid(path[i].x, path[i].y))
                return false; // path must never cross a solid cell
        // Direct cardinal distance without the wall would be 5 points;
        // the detour must be longer.
        return path.size() > 5;
    }

    bool TestGridUnreachablePartial()
    {
        k2d::AStarGrid2D grid;
        grid.SetSize(5, 5);
        // Fully enclose (2,2) with solid cells on all 4 cardinal sides,
        // diagonal movement off so corners can't sneak in either.
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
        // Block the cell directly above the diagonal target's row so the
        // (0,0)->(1,1) cut has one solid flanking cell (1,0).
        grid.SetSolid(1, 0, true);
        ct::Vector<glm::ivec2> path;
        bool ok = grid.GetIdPath({0, 0}, {1, 1}, path);
        // With one flanking cell solid, OnlyIfNoObstacles must refuse the
        // direct diagonal cut -- path has to detour via (0,1), so > 2 points.
        return ok && path.size() > 2;
    }

    // --- TileMapComponent -> AStarGrid2D ----------------------------------

    bool TestTileMapBlankCellsAreWalkable()
    {
        k2d::TileMapComponent map;
        map.setCellSize(16.0f, 16.0f);
        map.setMapSize(5, 5);
        // No tiles painted at all -- an all-blank map must be fully open.
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
        // Atlas tile id 2 is our "wall" tile. Paint a wall column at x=2,
        // rows 0..3, leaving only row 4 open (mirrors TestGridWallDetour).
        for (int y = 0; y <= 3; ++y)
            map.setTile(2, y, 2);
        // A different, non-wall tile elsewhere must stay walkable.
        map.setTile(0, 0, 5);

        k2d::AStarGrid2D grid;
        const int solidIds[] = {2};
        map.buildPathfindingGrid(grid, solidIds, 1);
        // buildPathfindingGrid only sets size/cell size/solids -- diagonal
        // mode is the caller's call, same as any other AStarGrid2D.
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
        return path.size() > 5; // must detour, same as the wall-column test above
    }

    bool TestTileMapGridPositionsMatchCellSize()
    {
        k2d::TileMapComponent map;
        map.setCellSize(20.0f, 10.0f);
        map.setMapSize(3, 3);
        k2d::AStarGrid2D grid;
        map.buildPathfindingGrid(grid);
        // Cell (1,1) center in the tilemap's local space: ((1+0.5)*20, (1+0.5)*10).
        glm::vec2 p = grid.GetPointPosition(1, 1);
        return Near(p.x, 30.0f) && Near(p.y, 15.0f);
    }

    // --- AStar2D ---------------------------------------------------------

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
        astar.ConnectPoints(0, 2); // shortcut across the square

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
        // No 0-2 shortcut this time.

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
        // Only route left is via 3.
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
        astar.AddPoint(2, {20.0f, 0.0f}); // isolated -- no connections at all
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
