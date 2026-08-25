#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <mathc.h>

namespace k2d::editor
{

class EditorSettings
{
  public:
    bool load(const char* path);
    bool save(const char* path) const;

    void touchRecentProject(const ct::String& projectFile);

    ct::String lastProjectPath;
    ct::String lastScenePath;
    ct::Vector<ct::String> recentProjectPaths;

    int themeIndex = 0;
    int windowDisplayIndex = 0;

    ct::String assetsDirectory;

    Math::Vec2 viewportPan = Math::Vec2(0.0f, 0.0f);
    float viewportZoom = 1.0f;
    int viewportTool = 0;
    bool viewportSnap = false;
    bool viewportShowGrid = true;
    Math::Vec2 viewportGridSize = Math::Vec2(32.0f, 32.0f);
    bool viewportLivePreview = true;
    bool showColliders = true;
    bool showPhysicsDebug = false;
};

} // namespace k2d::editor
