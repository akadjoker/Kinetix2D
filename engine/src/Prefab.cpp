#include "k2d/Prefab.h"

#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Serializer.h"

namespace k2d
{

    Prefab::Prefab() : mLoaded(false)
    {
    }

    bool Prefab::Load(const char *path)
    {
        mLoaded = false;
        mPath.clear();

        FileBuffer buffer;
        if (!FileSystem::Instance().LoadFile(path, buffer, true))
            return false;

        ct::Json::Error err;
        mData = ct::Json::parse(buffer.Text(), &err);
        if (err)
            return false;

        mPath = path;
        mLoaded = true;
        return true;
    }

    void Prefab::LoadFromJson(const ct::Json &data)
    {
        mData = data;
        mPath.clear();
        mLoaded = true;
    }

    void Prefab::SaveFromObject(const GameObject &object, Assets *assets)
    {
        mData = Serializer::WriteObject(object, assets);
        mLoaded = true;
    }

    bool Prefab::SaveToFile(const char *path, const GameObject &object, Assets *assets)
    {
        SaveFromObject(object, assets);
        if (!FileSystem::Instance().SaveTextFile(path, mData.dump(2)))
            return false;
        mPath = path;
        return true;
    }

    GameObject *Prefab::Instantiate(Scene &scene, GameObject *parent, Assets *assets) const
    {
        if (!mLoaded)
            return nullptr;
        return Serializer::ReadObject(scene, mData, parent, assets);
    }

}
