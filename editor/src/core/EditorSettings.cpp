#include "EditorSettings.h"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>

#include <ct/json.hpp>

namespace k2d::editor
{

namespace
{
Math::Vec2 readVec2(const ct::Json &value, const Math::Vec2 &fallback)
{
    if (!value.is_array() || value.size() < 2)
        return fallback;
    return Math::Vec2(static_cast<float>(value[0].as_double(fallback.x)),
                      static_cast<float>(value[1].as_double(fallback.y)));
}

ct::Json writeVec2(const Math::Vec2 &value)
{
    ct::Json array = ct::Json::array();
    array.push_back(value.x);
    array.push_back(value.y);
    return array;
}
}

bool EditorSettings::load(const char *path)
{
    FileBuffer buffer;
    if (!FileSystem::Instance().LoadFile(path, buffer, true))
        return false;

    ct::Json::Error err;
    const ct::Json root = ct::Json::parse(buffer.Text(), &err);
    if (err)
        return false;

    lastProjectPath = root["lastProjectPath"].as_cstr("");
    lastScenePath = root["lastScenePath"].as_cstr("");
    recentProjectPaths.clear();
    const ct::Json &recent = root["recentProjectPaths"];
    if (recent.is_array())
        for (size_t i = 0; i < recent.size(); ++i)
            recentProjectPaths.push_back(ct::String(recent[i].as_cstr("")));

    themeIndex = static_cast<int>(root["themeIndex"].as_int(0));
    assetsDirectory = root["assetsDirectory"].as_cstr("");

    viewportPan = readVec2(root["viewportPan"], Math::Vec2(0.0f, 0.0f));
    viewportZoom = static_cast<float>(root["viewportZoom"].as_double(1.0));
    viewportTool = static_cast<int>(root["viewportTool"].as_int(0));
    viewportSnap = root["viewportSnap"].as_bool(false);
    viewportShowGrid = root["viewportShowGrid"].as_bool(true);
    viewportLivePreview = root["viewportLivePreview"].as_bool(true);
    scriptHotReload = root["scriptHotReload"].as_bool(true);
    showColliders = root["showColliders"].as_bool(true);
    return true;
}

bool EditorSettings::save(const char *path) const
{
    ct::Json root = ct::Json::object();
    root.set("lastProjectPath", lastProjectPath);
    root.set("lastScenePath", lastScenePath);
    ct::Json recent = ct::Json::array();
    for (size_t i = 0; i < recentProjectPaths.size(); ++i)
        recent.push_back(recentProjectPaths[i]);
    root.set("recentProjectPaths", recent);
    root.set("themeIndex", themeIndex);
    root.set("assetsDirectory", assetsDirectory);
    root.set("viewportPan", writeVec2(viewportPan));
    root.set("viewportZoom", viewportZoom);
    root.set("viewportTool", viewportTool);
    root.set("viewportSnap", viewportSnap);
    root.set("viewportShowGrid", viewportShowGrid);
    root.set("viewportLivePreview", viewportLivePreview);
    root.set("scriptHotReload", scriptHotReload);
    root.set("showColliders", showColliders);

    return FileSystem::Instance().SaveTextFile(path, root.dump(2));
}

void EditorSettings::touchRecentProject(const ct::String &projectFile)
{
    if (projectFile.empty())
        return;

    for (size_t i = 0; i < recentProjectPaths.size(); ++i)
    {
        if (recentProjectPaths[i] == projectFile)
        {
            for (size_t j = i; j + 1 < recentProjectPaths.size(); ++j)
                recentProjectPaths[j] = recentProjectPaths[j + 1];
            recentProjectPaths.back() = projectFile;
            return;
        }
    }

    recentProjectPaths.push_back(projectFile);
    while (recentProjectPaths.size() > 10)
    {
        for (size_t j = 0; j + 1 < recentProjectPaths.size(); ++j)
            recentProjectPaths[j] = recentProjectPaths[j + 1];
        recentProjectPaths.pop_back();
    }
}

}
