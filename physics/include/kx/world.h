#pragma once

#include <glm/glm.hpp>
#include <ct/hashmap.hpp>
#include <ct/pool.hpp>
#include <ct/vector.hpp>

#include "body.h"
#include "dynamictree.h"
#include "joint.h"
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
    };

    struct StoredImpulses
    {
        int count;
        uint32_t idKey[kMaxManifoldPoints];
        float normalImpulse[kMaxManifoldPoints];
        float tangentImpulse[kMaxManifoldPoints];
        uint32_t stamp;
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
        void Destroy(Body *body);

        Body *BodyAtPoint(const glm::vec2 &point) const;

        void AddJoint(Joint *joint);
        void DestroyJoint(Joint *joint);

        void Step(float dt);

        const glm::vec2 &Gravity() const { return mGravity; }

        const ct::Vector<Body *> &Bodies() const { return mBodies; }
        const ct::Vector<ContactInfo> &Contacts() const { return mContacts; }

        size_t BodyCount() const { return mBodies.size(); }
        size_t ContactCount() const { return mContacts.size(); }

        void SetVelocityIterations(int iterations) { mVelocityIterations = iterations; }

        int32_t TreeHeight() const { return mTree.GetHeight(); }

    private:
        void SyncProxies();
        void FindPairs(ct::Vector<Body *> &pairsA, ct::Vector<Body *> &pairsB);
        void UpdateContacts();
        void InitContactConstraints();
        void WarmStartContacts();
        void SolveContactVelocities();
        void SolveContactPositions();
        void StoreContactImpulses();

        static uint64_t ContactKey(const ContactInfo &c)
        {
            return (static_cast<uint64_t>(c.a->mId) << 35) |
                   (static_cast<uint64_t>(c.b->mId) << 6) |
                   (static_cast<uint64_t>(c.shapeIndexA) << 3) |
                   static_cast<uint64_t>(c.shapeIndexB);
        }

        glm::vec2 mGravity;
        ct::Pool<Body> mBodyPool;
        ct::Vector<Body *> mBodies;
        ct::Vector<ContactInfo> mContacts;
        ct::Vector<Joint *> mJoints;
        DynamicTree mTree;
        ct::Vector<int32_t> mMoveBuffer;
        ct::HashMap<uint64_t, StoredImpulses> mImpulseMap;
        ct::Vector<uint64_t> mStaleKeys;
        uint32_t mStepStamp;
        uint32_t mNextBodyId;
        int mVelocityIterations;
    };

} // namespace kx
