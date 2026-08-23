#pragma once

namespace k2d
{

    class GameObject;
    class BatchRenderer;

    struct SceneDebugOptions
    {
        bool showOrigins = true;      // small dot at each object's global position
        bool showAxes = true;         // local right (red) / up (green) directions
        bool showHierarchyLines = true; // faint line from each object to its parent
        bool showSpriteBounds = true; // wire rect matching SpriteComponent size/pivot
        bool showNames = false;       // object name as text (BatchRenderer::DrawText) -- off by default, gets busy fast
        float axisLength = 16.0f;
        float originRadius = 3.0f;
        float textSize = 10.0f;
    };

    // Debug gizmos for a GameObject subtree (origins, local axes, parent links,
    // sprite bounds), drawn through BatchRenderer -- the same screen/world
    // immediate-mode renderer already used for HUD text and kx physics debug
    // draw, so this composes with whatever projection the caller already set
    // (world-space Camera2D projection to overlay on the game, or a plain
    // screen ortho). Recursive over the whole subtree including `root` itself,
    // so pass scene.root() for the whole scene or any GameObject for just
    // that branch. Call between batch.BeginFrame() and batch.EndFrame(),
    // after (or instead of) the normal CanvasRenderer scene pass.
    void DrawSceneDebug(BatchRenderer &batch, GameObject &root,
                        const SceneDebugOptions &options = SceneDebugOptions());

}
