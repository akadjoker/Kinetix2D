#pragma once

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <ct/vector.hpp>

namespace k2d
{

    class CanvasRenderer;

    class Scene
    {
    public:
        Scene();
        ~Scene();

        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;

        GameObject &root();
        const GameObject &root() const;

        GameObject *createObject(const char *name = "", GameObject *parent = nullptr);
        bool add(GameObject *object, GameObject *parent = nullptr);
        bool destroy(GameObject *object);

        GameObject *find(const char *name) const;
        std::size_t objectCount() const;

        void update(float deltaTime);
        void render(CanvasRenderer &canvas);
        std::size_t renderItemCount() const { return mRenderQueue.ItemCount(); }
        std::size_t renderCommandCount() const { return mRenderQueue.CommandCount(); }
        std::size_t renderLightCount() const { return mRenderQueue.LightCount(); }

        void clear();

    private:
        friend class GameObject;

        void registerBranch(GameObject *object);
        void unregisterBranch(GameObject *object);
        void flushDisposed();
        GameObject *findRecursive(const GameObject *from, const char *name) const;
        void collectDisposed(GameObject *object, ct::Vector<GameObject *> &out);

        GameObject mRoot;
        RenderQueue mRenderQueue;
        uint64_t mNextId;
        std::size_t mObjectCount;
        ct::Vector<GameObject *> mObjects;
        std::size_t mLateUpdateCount;
    };

}