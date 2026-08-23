#pragma once

#include <glm/glm.hpp>
#include <ct/hashmap.hpp>
#include <ct/pool.hpp>
#include <ct/vector.hpp>

#include "body.h"
#include "dynamictree.h"
#include "joints/joint.h"
#include "manifold.h"

namespace kx
{

    struct ContactInfo
    {
        Body *a;
        Body *b;
        int shapeIndexA;
        int shapeIndexB;
        Manifold manifold;
        glm::vec2 normal;
        glm::vec2 tangent;
        glm::vec2 rA[kMaxManifoldPoints];
        glm::vec2 rB[kMaxManifoldPoints];
        float normalMass[kMaxManifoldPoints];
        float tangentMass[kMaxManifoldPoints];
        float velocityBias[kMaxManifoldPoints];
        float friction;
        float restitution;
        bool sensor;
    };

    enum class ContactPhase : unsigned char
    {
        Begin,
        Persist,
        End
    };

    struct ContactEvent
    {
        ContactPhase phase;
        Body *self;
        Body *other;
        int shapeIndexSelf;
        int shapeIndexOther;
        const Manifold *manifold;
        bool sensor;
    };

    struct BodyPair
    {
        Body *a;
        Body *b;
    };

    struct StoredImpulses
    {
        int count;
        uint32_t idKey[kMaxManifoldPoints];
        float normalImpulse[kMaxManifoldPoints];
        float tangentImpulse[kMaxManifoldPoints];
        uint32_t stamp;
    };

    struct ContactState
    {
        Body *a;
        Body *b;
        int shapeIndexA;
        int shapeIndexB;
        Manifold manifold;
        bool sensor;
        uint32_t stamp;
    };

    struct StepProfile
    {
        float broadphase;
        float narrowphase;
        float solveVelocity;
        float solvePosition;
        float integrate;
    };

    struct RayCastHit
    {
        Body *body;
        int shapeIndex;
        glm::vec2 point;
        glm::vec2 normal;
        float fraction;
    };

    struct BulletSweep
    {
        Body *body;
        glm::vec2 prevCenter;
    };

    class World
    {
    public:
        explicit World(const glm::vec2 &gravity);
        ~World();

        World(const World &) = delete;
        World &operator=(const World &) = delete;

        Body *CreateBody(BodyType type, const glm::vec2 &pos, float angle = 0.0f);
        Body *CreateBox(const glm::vec2 &pos, float halfWidth, float halfHeight, float density);
        Body *CreateCircle(const glm::vec2 &pos, float radius, float density);
        Body *CreateStaticBox(const glm::vec2 &pos, float halfWidth, float halfHeight);
        Body *CreateKinematicBox(const glm::vec2 &pos, float halfWidth, float halfHeight);
        Body *CreateEdge(const glm::vec2 &a, const glm::vec2 &b);
        Body *CreateChain(const glm::vec2 *points, int count, bool loop);
        Body *CreatePolygon(const glm::vec2 &pos, const glm::vec2 *points, int count, float density);
        Body *CreateMesh(const glm::vec2 &pos, const glm::vec2 *outline, int count, float density);
        Body *CreateFromImage(const glm::vec2 &pos, const unsigned char *pixels, int width, int height, int bpp,
                              unsigned char threshold, float density, float scale = 1.0f, float simplifyDegrees = 2.0f);
        void Destroy(Body *body);

        Body *BodyAtPoint(const glm::vec2 &point) const;
        void QueryAABB(const AABB &aabb, ct::Vector<Body *> &out) const;
        void QueryCircle(const glm::vec2 &center, float radius, ct::Vector<Body *> &out) const;

        bool RayCastClosest(const glm::vec2 &origin, const glm::vec2 &translation, RayCastHit &outHit,
                            uint16_t categoryMask = 0xFFFF, bool includeSensors = false,
                            const Body *ignoreBody = nullptr) const;

        void RayCastAll(const glm::vec2 &origin, const glm::vec2 &translation, ct::Vector<RayCastHit> &outHits,
                        uint16_t categoryMask = 0xFFFF, bool includeSensors = false,
                        const Body *ignoreBody = nullptr) const;

        void AddJoint(Joint *joint);
        void DestroyJoint(Joint *joint);

        void Step(float dt);

        const glm::vec2 &Gravity() const { return mGravity; }
        void SetGravity(const glm::vec2 &gravity) { mGravity = gravity; }

        const ct::Vector<Body *> &Bodies() const { return mBodies; }
        const ct::Vector<ContactInfo> &Contacts() const { return mContacts; }

        size_t BodyCount() const { return mBodies.size(); }
        size_t ContactCount() const { return mContacts.size(); }

        void SetVelocityIterations(int iterations) { mVelocityIterations = iterations; }

        void SetTreeBroadphase(bool enabled) { mUseTree = enabled; }
        bool TreeBroadphase() const { return mUseTree; }

        void SetTimeSource(double (*clock)()) { mClock = clock; }
        const StepProfile &Profile() const { return mProfile; }

        const ct::Vector<Joint *> &Joints() const { return mJoints; }

    private:
        void UpdateContacts();
        void UpdateContactsBrute();
        void UpdateContactsTree();
        void SyncProxies();
        void FindNewPairs();
        void CollidePair(Body *first, Body *second);
        bool JointsAllowCollision(const Body *a, const Body *b) const;
        void InitContactConstraints();
        void WarmStartContacts();
        void SolveContactVelocitiesOne(ContactInfo &c);
        void SolveContactVelocities();
        void SolveContactPointPosition(ContactInfo &c, int pointIndex, float baumgarte);
        void SolveContactPositions();
        void StoreContactImpulses();
        void UpdateContactEvents();
        void DispatchContactEvent(ContactPhase phase, const ContactState &state);
        void RemoveBodyContactEvents(Body *body);
        void UpdateSleeping(float dt);

        void SolveBulletSweeps();

        void DestroyJointInternal(Joint *joint);

        void RayCastGather(const glm::vec2 &origin, const glm::vec2 &translation,
                           uint16_t categoryMask, bool includeSensors, const Body *ignoreBody,
                           bool stopAtFirst, ct::Vector<RayCastHit> &outHits) const;

        static constexpr uint64_t kBodyIdBits = 27;
        static constexpr uint64_t kBodyIdMask = (uint64_t(1) << kBodyIdBits) - 1;

        static uint64_t ContactKey(const ContactInfo &c)
        {
            return (static_cast<uint64_t>(c.a->mId) << 37) |
                   (static_cast<uint64_t>(c.b->mId) << 10) |
                   (static_cast<uint64_t>(c.shapeIndexA) << 5) |
                   static_cast<uint64_t>(c.shapeIndexB);
        }

        glm::vec2 mGravity;
        ct::Pool<Body> mBodyPool;
        ct::Vector<Body *> mBodies;
        ct::Vector<ContactInfo> mContacts;
        ct::Vector<Joint *> mJoints;
        DynamicTree mTree;
        ct::Vector<int32_t> mMoveBuffer;
        ct::HashMap<uint64_t, BodyPair> mPairs;
        ct::Vector<uint64_t> mDeadPairs;
        bool mUseTree;
        double (*mClock)();
        StepProfile mProfile;
        float mNarrowMs;
        ct::HashMap<uint64_t, StoredImpulses> mImpulseMap;
        ct::HashMap<uint64_t, ContactState> mContactStates;
        ct::Vector<uint64_t> mStaleKeys;
        uint32_t mStepStamp;
        uint32_t mNextBodyId;
        ct::Vector<uint32_t> mFreeBodyIds; 
        ct::HashMap<Body *, unsigned char> mTouchingActive; 
        mutable ct::Vector<RayCastHit> mRayScratch; 
        ct::Vector<BulletSweep> mBulletSweeps; 
        int mVelocityIterations;
    };

    void Explode(World &world, const glm::vec2 &center, float radius, float force, float falloff);

    bool Slice(World &world, Body *body, const glm::vec2 &point, const glm::vec2 &normal,
              Body **outPositive, Body **outNegative, float separationSpeed = 0.0f);

} 