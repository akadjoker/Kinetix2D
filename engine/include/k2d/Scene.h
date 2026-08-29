#pragma once

#include "k2d/Camera2D.h"
#include "k2d/CollisionInfo.h"
#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <ct/vector.hpp>

namespace kx
{
class World;
class Body;
} // namespace kx

namespace k2d
{

    class CanvasRenderer;
    class CameraComponent;
    class UiControl;
    class RigidBody2D;
    struct ScenePhysics;

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
        // Bumped whenever objects or components enter, leave or are renamed.
        // Lets per-frame consumers cache name lookups instead of re-searching
        // the tree every frame, with deletion-safe invalidation.
        uint32_t topologyVersion() const { return mTopologyVersion; }
        // Chooses the enabled camera with the highest render priority. Ties use
        // the oldest object, so the choice is deterministic.
        CameraComponent *activeCamera();
        const CameraComponent *activeCamera() const;
        void makeCameraActive(CameraComponent &camera);

        void setRenderCamera(const Camera2D *camera, float viewportWidth, float viewportHeight);
        const Camera2D *renderCamera() const { return mRenderCamera; }
        float renderViewportWidth() const { return mRenderViewportWidth; }
        float renderViewportHeight() const { return mRenderViewportHeight; }

        void update(float deltaTime);
        void render(CanvasRenderer &canvas);
        // Builds the current frame's render commands without touching the GPU.
        // Useful for integrations that need to inspect or submit the queue themselves.
        RenderQueue &buildRenderQueue();
        std::size_t renderItemCount() const { return mRenderQueue.ItemCount(); }
        std::size_t renderCommandCount() const { return mRenderQueue.CommandCount(); }
        std::size_t renderLightCount() const { return mRenderQueue.LightCount(); }

        void clear();

        void setSimulationEnabled(bool enabled);
        bool simulationEnabled() const;

        void setGravity(const Math::Vec2 &gravity);
        Math::Vec2 gravity() const;
        void setFixedTimeStep(float seconds);
        float fixedTimeStep() const;
        void setCollisionCallback(CollisionCallback callback, void *user);

        GameObject *raycast(const Math::Vec2 &origin, const Math::Vec2 &direction, float distance,
                            Math::Vec2 *outPoint = nullptr, Math::Vec2 *outNormal = nullptr,
                            const GameObject *ignore = nullptr);
        GameObject *objectAtPoint(const Math::Vec2 &point);
        void overlapCircle(const Math::Vec2 &center, float radius, ct::Vector<GameObject *> &out);

        std::size_t physicsBodyCount() const;
        std::size_t physicsContactCount() const;
        kx::World *physicsWorld();

        void debugDrawPhysics(CanvasRenderer &canvas, unsigned flags);

        static GameObject *objectForBody(const kx::Body *body);
        void markPhysicsDirty(RigidBody2D &rigidBody);

    private:
        friend class GameObject;
        friend class RigidBody2D;

        void markTopologyChanged() { ++mTopologyVersion; }

        void registerBranch(GameObject *object);
        void unregisterBranch(GameObject *object);
        void registerComponent(Component *component);
        void unregisterComponent(Component *component);
        void compactComponentLists();
        void updateUi();
        void flushDisposed();
        GameObject *findRecursive(const GameObject *from, const char *name) const;
        void collectDisposed(GameObject *object, ct::Vector<GameObject *> &out);

        void attachPhysicsBody(RigidBody2D &rigidBody);
        void detachPhysicsBody(RigidBody2D &rigidBody);
        void physicsStep(float deltaTime);
        static ScenePhysics *createPhysics();
        void clearPhysics();
        void teardownPhysics();

        GameObject mRoot;
        RenderQueue mRenderQueue;
        uint64_t mNextId;
        std::size_t mObjectCount;
        uint32_t mTopologyVersion;
        bool mComponentListsDirty;
        bool mHasDisposed;
        // Flat event lists keep the update/render hot paths independent from
        // the number of component types an object does not have.
        ct::Vector<Component *> mAllComponents;
        ct::Vector<Component *> mLateUpdateComponents;
        ct::Vector<Component *> mRenderComponents;
        ct::Vector<CameraComponent *> mCameras;
        ct::Vector<UiControl *> mUiControls;
        ScenePhysics *mPhysics;
        const Camera2D *mRenderCamera;
        float mRenderViewportWidth;
        float mRenderViewportHeight;
    };

}
