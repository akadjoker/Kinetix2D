#pragma once

#include "core/EditorPanel.h"

#include <k2d/Prefab.h>

#include <ct/string.hpp>

namespace k2d::editor
{

class PrefabsPanel final : public EditorPanel
{
public:
    explicit PrefabsPanel(EditorApplication &application)
        : EditorPanel("Prefabs", application)
    {
    }

private:
    void drawContents() override;
    void drawNode(const ct::Json &node, int index);

    Prefab mPrefab;
    ct::String mLoadedPath;
};

}
