#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d::editor
{

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

    const ct::Vector<ct::String> &scenes() const { return mScenes; }
    void addScene(const char *relativePath);
    void removeScene(const char *relativePath);
    bool hasScene(const char *relativePath) const;

    const ct::String &startupScene() const { return mStartupScene; }
    void setStartupScene(const char *relativePath);

private:
    ct::String mName;
    ct::String mRoot;
    ct::String mProjectFile;
    ct::Vector<ct::String> mScenes;
    ct::String mStartupScene;
    bool mValid = false;
};

}
