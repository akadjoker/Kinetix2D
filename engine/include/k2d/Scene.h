#pragma once

#include "k2d/Camera2D.h"
#include "k2d/CollisionInfo.h"
#include "k2d/Contact2D.h"
#include "k2d/DynamicTree2D.h"
#include "k2d/GameObject.h"
#include "k2d/Geometry2D.h"
#include "k2d/ParticlePhysics2D.h"
#include "k2d/RenderQueue.h"

#include <ct/hashmap.hpp>
#include <ct/vector.hpp>

namespace k2d
{

    class CanvasRenderer;
    class CameraComponent;
    class UiControl;
    class RigidBody2D;
    class Joint2D;
    class Steering2D;
    class TileMapComponent;

    enum DebugDrawFlags : unsigned
    {
        DebugDrawShapes = 1 << 0,
        DebugDrawAABBs = 1 << 1,
        DebugDrawContacts = 1 << 2,
        DebugDrawJoints = 1 << 3
    };

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
        bool simulationEnabled() const { return mSimulationEnabled; }

        void setGravity(const Math::Vec2 &gravity) { mGravity = gravity; }
        Math::Vec2 gravity() const { return mGravity; }
        void setFixedTimeStep(float seconds) { mFixedStep = seconds > 0.0f ? seconds : 0.0f; }
        float fixedTimeStep() const { return mFixedStep; }
        void setVelocityIterations(int iterations) { mVelocityIterations = iterations; }
        int velocityIterations() const { return mVelocityIterations; }
        void setTreeBroadphase(bool enabled) { mUseTree = enabled; }
        bool treeBroadphase() const { return mUseTree; }
        // Scene-owned VFX projectiles. Stepped with the rest of the physics,
        // so they only advance while the simulation is enabled.
        ParticlePhysics2D &particles() { return mParticles; }
        const ParticlePhysics2D &particles() const { return mParticles; }

        void setCollisionCallback(CollisionCallback callback, void *user);
        void setAnimationEventCallback(AnimationEventCallback callback, void *user);
        void dispatchAnimationEvent(GameObject *object, const char *clip, const char *event, bool finished);
        void setActionEventCallback(ActionEventCallback callback, void *user);
        void dispatchActionEvent(GameObject *object, const char *event);

        GameObject *raycast(const Math::Vec2 &origin, const Math::Vec2 &direction, float distance,
                            Math::Vec2 *outPoint = nullptr, Math::Vec2 *outNormal = nullptr,
                            const GameObject *ignore = nullptr, uint16_t categoryMask = 0xFFFF);
        GameObject *objectAtPoint(const Math::Vec2 &point);
        void overlapCircle(const Math::Vec2 &center, float radius, ct::Vector<GameObject *> &out);
        bool testMotion(RigidBody2D &body, const Math::Vec2 &motion, MotionResult &out,
                        float safeMargin = kLinearSlop) const;
        bool testPosition(RigidBody2D &body, const Math::Vec2 &position, MotionResult &out) const;
        // Offset that lifts the body out of anything it already overlaps. A
        // shape cast reports no hit from inside a shape, so without this a
        // character that ends a frame embedded walks straight through walls.
        Math::Vec2 recoverOverlap(RigidBody2D &body, const Math::Vec2 &position, float margin) const;

        // Sums the weighted forces of the object's own steering components.
        Math::Vec2 steeringForce(const GameObject &object, const Math::Vec2 &velocity, float deltaTime,
                                 bool *outVetoed = nullptr) const;
        std::size_t steeringCount() const { return mSteerings.size(); }
        Steering2D *steeringAt(std::size_t index) const;

        // Steering neighbours come from the physics broadphase, so only objects
        // with a body and a collider can be seen. The result lives in a
        // scene-owned scratch list and is valid until the next call.
        std::size_t queryNeighbours(const GameObject &self, const Math::Vec2 &center, float radius,
                                    uint16_t mask) const;
        GameObject *neighbourAt(std::size_t index) const;

        std::size_t bodyCount() const { return mBodies.size(); }
        std::size_t contactCount() const { return mContacts.size(); }
        const StepProfile &stepProfile() const { return mStepProfile; }

        void debugDrawBodies(CanvasRenderer &canvas, unsigned flags);

        static GameObject *objectForBody(const RigidBody2D *body);
        void markBodyDirty(RigidBody2D &rigidBody);
        // Builds a body's shapes from its colliders now, simulating or not.
        void buildBodyShapes(RigidBody2D &rigidBody);

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

        // The tree is only stale for queries made outside the step, so one sync
        // per frame serves every agent instead of one sync per agent.
        void syncBroadphaseOnce() const;

        void attachBody(RigidBody2D &rigidBody);
        void detachBody(RigidBody2D &rigidBody);
        void clearBodies();
        void collectBodies(GameObject &object);
        void createBody(GameObject &object, RigidBody2D &rigidBody);
        void attachColliders(GameObject &object, RigidBody2D &rigidBody);
        void rebuildBody(RigidBody2D &rigidBody);
        void rebuildDirtyBodies();
        void resolveJoints();
        void pushTransforms();
        void pullTransforms();
        void stepBodies(float deltaTime);

        void destroyBody(RigidBody2D *body);
        void step(float dt);
        void updateContacts();
        void updateContactsBrute();
        void updateContactsTree();
        void syncProxies();
        void findNewPairs();
        void collidePair(RigidBody2D *first, RigidBody2D *second);
        bool jointsAllowCollision(const RigidBody2D *a, const RigidBody2D *b) const;
        void initContactConstraints();
        void warmStartContacts();
        void solveContactVelocitiesOne(ContactInfo &c);
        void solveContactVelocities();
        void solveContactPointPosition(ContactInfo &c, int pointIndex, float baumgarte);
        void solveContactPositions();
        void storeContactImpulses();
        void updateContactEvents();
        void dispatchContactEvent(ContactPhase phase, const PairContactState &state);
        void removeBodyContactEvents(RigidBody2D *body);
        void updateSleeping(float dt);
        void solveBulletSweeps();

        RigidBody2D *bodyAtPoint(const Math::Vec2 &point, bool dynamicOnly) const;
        void queryAABB(const AABB &aabb, ct::Vector<RigidBody2D *> &out) const;
        void queryCircle(const Math::Vec2 &center, float radius, ct::Vector<RigidBody2D *> &out) const;
        bool rayCastClosest(const Math::Vec2 &origin, const Math::Vec2 &translation, RayCastHit &outHit,
                            uint16_t categoryMask, bool includeSensors, const RigidBody2D *ignoreBody) const;
        void rayCastGather(const Math::Vec2 &origin, const Math::Vec2 &translation, uint16_t categoryMask,
                           bool includeSensors, const RigidBody2D *ignoreBody, bool stopAtFirst,
                           ct::Vector<RayCastHit> &outHits) const;

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
        ct::Vector<Steering2D *> mSteerings;
        const Camera2D *mRenderCamera;
        float mRenderViewportWidth;
        float mRenderViewportHeight;

        Math::Vec2 mGravity;
        ct::Vector<RigidBody2D *> mBodies;
        ct::Vector<ContactInfo> mContacts;
        ct::Vector<Joint2D *> mJoints;
        DynamicTree mTree;
        ct::Vector<int32_t> mMoveBuffer;
        ct::Vector<int32_t> mPairScratch;
        ct::HashMap<uint64_t, BodyPair> mPairs;
        ct::Vector<uint64_t> mKeyScratch;
        bool mUseTree;
        double (*mClock)();
        StepProfile mStepProfile;
        float mNarrowMs;
        ct::HashMap<uint64_t, PairContactState> mPairContactStates;
        ct::Vector<ContactInfo *> mDynamicContacts;
        ct::Vector<ContactInfo *> mStaticContacts;
        uint32_t mStepStamp;
        uint32_t mNextBodyId;
        ct::Vector<uint32_t> mFreeBodyIds;
        ct::HashMap<RigidBody2D *, unsigned char> mTouchingActive;
        mutable ct::Vector<RayCastHit> mRayScratch;
        mutable ct::Vector<RigidBody2D *> mBodyScratch;
        mutable ct::Vector<RigidBody2D *> mNeighbourBodies;
        mutable ct::Vector<GameObject *> mNeighbours;
        uint32_t mFrameStamp;
        mutable uint32_t mBroadphaseStamp;
        ct::Vector<BulletSweep> mBulletSweeps;
        int mVelocityIterations;

        float mFixedStep;
        float mAccumulator;
        CollisionCallback mCollisionCallback;
        void *mCollisionCallbackUser;
        AnimationEventCallback mAnimationEventCallback;
        void *mAnimationEventCallbackUser;
        ActionEventCallback mActionEventCallback;
        void *mActionEventCallbackUser;
        bool mSimulationEnabled;
        bool mHasDirtyBodies;
        ParticlePhysics2D mParticles;
    };

}
