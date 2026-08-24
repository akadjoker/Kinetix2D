#pragma once

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <ct/vector.hpp>

namespace k2d
{

    class CanvasRenderer;
    class CameraComponent;
    class UiControl;

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
        bool reparent(GameObject *object, GameObject *newParent);

        GameObject *find(const char *name) const;
        std::size_t objectCount() const;
        // Chooses the enabled camera with the highest render priority. Ties use
        // the oldest object, so the choice is deterministic.
        CameraComponent *activeCamera();
        const CameraComponent *activeCamera() const;

        void update(float deltaTime);
        void render(CanvasRenderer &canvas);
        // Builds the current frame's render commands without touching the GPU.
        // Useful for integrations that need to inspect or submit the queue themselves.
        RenderQueue &buildRenderQueue();
        std::size_t renderItemCount() const { return mRenderQueue.ItemCount(); }
        std::size_t renderCommandCount() const { return mRenderQueue.CommandCount(); }
        std::size_t renderLightCount() const { return mRenderQueue.LightCount(); }

        void clear();

    private:
        friend class GameObject;

        void registerBranch(GameObject *object);
        void unregisterBranch(GameObject *object);
        void registerComponent(Component *component);
        void unregisterComponent(Component *component);
        void compactComponentLists();
        void updateUi();
        void flushDisposed();
        GameObject *findRecursive(const GameObject *from, const char *name) const;
        void collectDisposed(GameObject *object, ct::Vector<GameObject *> &out);

        GameObject mRoot;
        RenderQueue mRenderQueue;
        uint64_t mNextId;
        std::size_t mObjectCount;
        // Flat event lists keep the update/render hot paths independent from
        // the number of component types an object does not have.
        ct::Vector<Component *> mAllComponents;
        ct::Vector<Component *> mLateUpdateComponents;
        ct::Vector<Component *> mRenderComponents;
        ct::Vector<CameraComponent *> mCameras;
        ct::Vector<UiControl *> mUiControls;
    };

}
