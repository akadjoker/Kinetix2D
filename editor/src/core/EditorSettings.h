#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <mathc.h>

namespace k2d::editor
{

class EditorSettings
{
public:
    bool load(const char *path);
    bool save(const char *path) const;

    void touchRecentProject(const ct::String &projectFile);

    ct::String lastProjectPath;
    ct::String lastScenePath;
    ct::Vector<ct::String> recentProjectPaths;

    int themeIndex = 0;

    ct::String assetsDirectory;

    Math::Vec2 viewportPan = Math::Vec2(0.0f, 0.0f);
    float viewportZoom = 1.0f;
    int viewportTool = 0;
    bool viewportSnap = false;
    bool viewportShowGrid = true;
    bool viewportLivePreview = true;
};

}
