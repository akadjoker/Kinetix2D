#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <mathc.h>

namespace k2d::editor
{

struct PhysicsSettings
{
    Math::Vec2 gravity = Math::Vec2(0.0f, 980.0f);
    float fixedTimeStep = 1.0f / 60.0f;
    int velocityIterations = 8;
    bool treeBroadphase = true;
};

class Project
{
public:
    bool create(const char *rootDirectory, const char *name);
    bool load(const char *projectFile);
    bool save() const;
    void close();

    bool valid() const { return mValid; }
    const ct::String &name() const { return mName; }
    const ct::String &root() const { return mRoot; }
    const ct::String &projectFile() const { return mProjectFile; }
    ct::String scenesDirectory() const;
    ct::String assetsDirectory() const;
    ct::String prefabsDirectory() const;

    const ct::Vector<ct::String> &scenes() const { return mScenes; }
    void addScene(const char *relativePath);
    void removeScene(const char *relativePath);
    bool hasScene(const char *relativePath) const;

    const ct::String &startupScene() const { return mStartupScene; }
    void setStartupScene(const char *relativePath);

    PhysicsSettings &physics() { return mPhysics; }
    const PhysicsSettings &physics() const { return mPhysics; }

private:
    ct::String mName;
    ct::String mRoot;
    ct::String mProjectFile;
    ct::Vector<ct::String> mScenes;
    ct::String mStartupScene;
    PhysicsSettings mPhysics;
    bool mValid = false;
};

}
