#pragma once

#include "core/EditorPanel.h"

#include <k2d/SpriteAtlas.h>
#include <mathc.h>

namespace k2d
{
class Texture;
}

namespace k2d::editor
{

class SpriteEditorPanel final : public EditorPanel
{
public:
    explicit SpriteEditorPanel(EditorApplication &application);

private:
    void drawContents() override;
    void openTexture(const char *path);

    Texture *mTexture = nullptr;
    // These regions are a creation palette. The Sprite objects created from
    // them are the persistent data and are serialized with the Scene.
    SpriteAtlas mAtlas;
    ct::String mMetadataPath;
    float mZoom = 1.0f;
    bool mSelecting = false;
    int mDraggingHandle = -1;
    Math::Vec2 mSelectionStart = Math::Vec2(0.0f);
    Math::Vec2 mSelectionEnd = Math::Vec2(0.0f);
    Math::Vec4 mSelectedRect = Math::Vec4(0.0f);
    int mSelectedRegion = -1;
    int mAlphaThreshold = 1;
    int mMinimumPixels = 4;
    int mAtlasColumns = 1;
    int mAtlasRows = 1;
    bool mDirty = false;
    ct::String mSpriteBaseName = "sprite";
    char mRegionName[64] = "sprite_001";
    char mSceneObjectName[64] = "sprite_001";
};

}
