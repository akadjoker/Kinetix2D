#include "Project.h"

#include "EditorFileSystem.h"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>

#include <ct/json.hpp>

namespace k2d::editor
{

namespace
{
constexpr const char *kProjectFileName = "project.k2dproj";
}

bool Project::create(const char *rootDirectory, const char *name)
{
    if (!rootDirectory || !rootDirectory[0])
        return false;

    ct::String root(rootDirectory);
    while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
        root.pop_back();

    if (!EditorFileSystem::makeDirectory(root))
        return false;
    if (!EditorFileSystem::makeDirectory(EditorFileSystem::join(root, "scenes")))
        return false;
    if (!EditorFileSystem::makeDirectory(EditorFileSystem::join(root, "assets")))
        return false;

    mRoot = root;
    mProjectFile = EditorFileSystem::join(root, kProjectFileName);
    mName = name && name[0] ? name : "Untitled";
    mScenes.clear();
    mStartupScene.clear();
    mValid = true;
    return save();
}

bool Project::load(const char *projectFile)
{
    if (!projectFile || !projectFile[0])
        return false;

    FileBuffer buffer;
    if (!FileSystem::Instance().LoadFile(projectFile, buffer, true))
        return false;

    ct::Json::Error err;
    const ct::Json document = ct::Json::parse(buffer.Text(), &err);
    if (err)
        return false;

    mProjectFile = projectFile;
    mRoot = EditorFileSystem::parentPath(mProjectFile);
    mName = document["name"].as_cstr("Untitled");
    mStartupScene = document["startupScene"].as_cstr("");

    mScenes.clear();
    const ct::Json &scenes = document["scenes"];
    if (scenes.is_array())
        for (size_t i = 0; i < scenes.size(); ++i)
            mScenes.push_back(ct::String(scenes[i].as_cstr("")));

    mPhysics = PhysicsSettings();
    const ct::Json &physics = document["physics"];
    if (physics.is_object())
    {
        const ct::Json &gravity = physics["gravity"];
        if (gravity.is_array() && gravity.size() >= 2)
            mPhysics.gravity = Math::Vec2((float)gravity[0].as_double(0.0),
                                          (float)gravity[1].as_double(980.0));
        mPhysics.fixedTimeStep = (float)physics["fixedTimeStep"].as_double(1.0 / 60.0);
        mPhysics.velocityIterations = (int)physics["velocityIterations"].as_int(8);
        mPhysics.treeBroadphase = physics["treeBroadphase"].as_bool(true);
    }

    mValid = true;
    return true;
}

bool Project::save() const
{
    if (!mValid)
        return false;

    ct::Json document = ct::Json::object();
    document.set("name", mName);
    document.set("startupScene", mStartupScene);
    ct::Json scenes = ct::Json::array();
    for (size_t i = 0; i < mScenes.size(); ++i)
        scenes.push_back(mScenes[i]);
    document.set("scenes", scenes);

    ct::Json gravity = ct::Json::array();
    gravity.push_back(ct::Json(mPhysics.gravity.x));
    gravity.push_back(ct::Json(mPhysics.gravity.y));

    ct::Json physics = ct::Json::object();
    physics.set("gravity", gravity);
    physics.set("fixedTimeStep", ct::Json((double)mPhysics.fixedTimeStep));
    physics.set("velocityIterations", ct::Json((int64_t)mPhysics.velocityIterations));
    physics.set("treeBroadphase", ct::Json(mPhysics.treeBroadphase));
    document.set("physics", physics);

    return FileSystem::Instance().SaveTextFile(mProjectFile.c_str(), document.dump(2));
}

void Project::close()
{
    mName.clear();
    mRoot.clear();
    mProjectFile.clear();
    mScenes.clear();
    mStartupScene.clear();
    mValid = false;
}

ct::String Project::scenesDirectory() const
{
    return EditorFileSystem::join(mRoot, "scenes");
}

ct::String Project::assetsDirectory() const
{
    return EditorFileSystem::join(mRoot, "assets");
}

bool Project::hasScene(const char *relativePath) const
{
    if (!relativePath)
        return false;
    for (size_t i = 0; i < mScenes.size(); ++i)
        if (mScenes[i] == relativePath)
            return true;
    return false;
}

void Project::addScene(const char *relativePath)
{
    if (!relativePath || !relativePath[0] || hasScene(relativePath))
        return;
    mScenes.push_back(ct::String(relativePath));
}

void Project::removeScene(const char *relativePath)
{
    if (!relativePath)
        return;
    for (auto it = mScenes.begin(); it != mScenes.end(); ++it)
    {
        if (*it == relativePath)
        {
            mScenes.erase(it);
            break;
        }
    }
    if (mStartupScene == relativePath)
        mStartupScene.clear();
}

void Project::setStartupScene(const char *relativePath)
{
    mStartupScene = relativePath ? relativePath : "";
}

}
