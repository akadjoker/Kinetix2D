#pragma once

#include <ct/json.hpp>

namespace k2d
{

    class GameObject;
    class Scene;
    class Assets;

    class Prefab
    {
    public:
        Prefab();

        bool Load(const char *path);
        void LoadFromJson(const ct::Json &data);

        bool SaveToFile(const char *path, const GameObject &object, Assets *assets = nullptr);
        void SaveFromObject(const GameObject &object, Assets *assets = nullptr);

        GameObject *Instantiate(Scene &scene, GameObject *parent = nullptr, Assets *assets = nullptr) const;

        bool valid() const { return mLoaded; }
        const ct::Json &data() const { return mData; }
        const ct::String &path() const { return mPath; }

    private:
        ct::Json mData;
        ct::String mPath;
        bool mLoaded;
    };

}
