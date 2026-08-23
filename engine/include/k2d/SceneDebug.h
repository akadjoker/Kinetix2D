#pragma once

namespace k2d
{

    class GameObject;
    class BatchRenderer;

    struct SceneDebugOptions
    {
        bool showOrigins = true;      
        bool showAxes = true;         
        bool showHierarchyLines = true; 
        bool showSpriteBounds = true; 
        bool showNames = false;       
        float axisLength = 16.0f;
        float originRadius = 3.0f;
        float textSize = 10.0f;
    };

    void DrawSceneDebug(BatchRenderer &batch, GameObject &root,
                        const SceneDebugOptions &options = SceneDebugOptions());

}