#include "k2d/SceneManager.h"

#include "k2d/Assets.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Serializer.h"

#include <ct/json.hpp>

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
                if (path[0] && !assets.GetTexture(path))
                    assets.LoadTexture(path, path, true, false);
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
    GameObject *loadedRoot = Serializer::ReadObject(scene, root, nullptr, &assets);
    if (!loadedRoot)
        return nullptr;
    mCurrentPath = path;
    return loadedRoot;
}

void SceneManager::Request(const char *path)
{
    mRequestedPath = path ? path : "";
}

GameObject *SceneManager::ApplyRequest(Scene &scene, Assets &assets)
{
    if (mRequestedPath.empty())
        return nullptr;
    const std::string path = mRequestedPath;
    mRequestedPath.clear();
    return Load(scene, assets, path.c_str());
}

SceneManager &GetSceneManager()
{
    static SceneManager manager;
    return manager;
}
}
