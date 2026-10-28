#include "k2d/SceneManager.h"

#include "k2d/Assets.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Serializer.h"

#include <ct/json.hpp>

#include <cstring>

namespace k2d
{
namespace
{
void preloadTextures(const ct::Json &node, Assets &assets)
{
    if (node.is_object())
    {
        const ct::Json::Object &members = node.members();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const ct::String &key = members[i].key;
            const ct::Json &value = members[i].value;
            if ((key == "texture" || key == "normalMap") && value.is_string())
            {
                const char *path = value.as_cstr("");
                if (path[0] && !assets.GetTexture(path) && !assets.LoadTexture(path, path, true, false))
                {
                    // A path from another machine: try its tails ("assets/x.png",
                    // then "x.png", ...) against the search paths. The texture keeps
                    // the name the scene gives it, so the components still find it.
                    for (const char *tail = std::strchr(path + 1, '/'); tail; tail = std::strchr(tail + 1, '/'))
                        if (tail[1] && assets.LoadTexture(path, tail + 1, true, false))
                            break;
                }
            }
            else
                preloadTextures(value, assets);
        }
    }
    else if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); ++i)
            preloadTextures(node[i], assets);
    }
}
}

GameObject *SceneManager::Load(Scene &scene, Assets &assets, const char *path)
{
    if (!path || !path[0])
        return nullptr;
    FileBuffer buffer;
    if (!FileSystem::Instance().LoadFile(path, buffer, true))
        return nullptr;
    ct::Json::Error error;
    const ct::Json document = ct::Json::parse(buffer.Text(), &error);
    const ct::Json &root = document["root"];
    if (error || !root.is_object())
        return nullptr;

    preloadTextures(document, assets);
    scene.clear();

    // The file's "root" describes the scene root itself, not a child of it.
    // Reading it as an object would nest a second "Scene" under the real one,
    // and anything a script spawns afterwards (parent nullptr) would land
    // outside that nested branch instead of inside the loaded scene.
    GameObject &sceneRoot = scene.root();
    Serializer::ReadInto(scene, sceneRoot, root, &assets);

    mCurrentPath = path;
    return &sceneRoot;
}

void SceneManager::Request(const char *path)
{
    mRequestedPath = path ? path : "";
}

GameObject *SceneManager::ApplyRequest(Scene &scene, Assets &assets)
{
    if (mRequestedPath.empty())
        return nullptr;
    const ct::String path = mRequestedPath;
    mRequestedPath.clear();
    return Load(scene, assets, path.c_str());
}

SceneManager &GetSceneManager()
{
    static SceneManager manager;
    return manager;
}
}
