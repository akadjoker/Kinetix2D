#pragma once

#include "core/EditorPanel.h"

#include <mathc.h>

namespace k2d
{
class GameObject;
}

namespace k2d::editor
{

class SceneViewportPanel final : public EditorPanel
{
public:
    explicit SceneViewportPanel(EditorApplication &application);

private:
    void drawContents() override;
    void drawGrid(ImDrawList &drawList, const ImVec2 &min, const ImVec2 &max) const;
    void drawObject(ImDrawList &drawList, GameObject &object, const ImVec2 &origin);
    void pickObject(GameObject &object, const ImVec2 &mouse, const ImVec2 &origin,
                    GameObject *&best, float &bestDistance);
    ImVec2 worldToScreen(float x, float y, const ImVec2 &origin) const;
    Math::Vec2 screenToWorld(const ImVec2 &screen, const ImVec2 &origin) const;
    void handlePrefabDrop(const ImVec2 &origin);

    ImVec2 mPan = ImVec2(0.0f, 0.0f);
    float mZoom = 1.0f;
    int mTool = 0;
    bool mSnap = false;
    bool mShowGrid = true;
};

}
