#pragma once

#include "core/EditorFileSystem.h"
#include "core/EditorPanel.h"

#include <ct/string.hpp>

namespace k2d::editor
{

constexpr const char *kPrefabDragDropPayload = "K2D_PREFAB_FILE";

class AssetsPanel final : public EditorPanel
{
public:
    explicit AssetsPanel(EditorApplication &application);

private:
    void drawContents() override;
    void drawEntry(const EditorFileEntry &entry);

    ct::String mRoot;
};

}
