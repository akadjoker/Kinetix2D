#include "k2d/PhysicsWorld2D.h"

#include "k2d/Collider2D.h"
#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

    namespace
    {
        constexpr float kDegToRad = 0.01745329251994329577f;
        constexpr float kRadToDeg = 57.29577951308232088f;
        constexpr float kMinShapeExtent = 0.01f;

        PhysicsWorld2D *gActiveWorld = nullptr;
    }

    PhysicsWorld2D::PhysicsWorld2D(const Math::Vec2 &gravity)
        : mWorld(gravity), mBodies(), mFixedStep(1.0f / 60.0f), mAccumulator(0.0f),
          mCallback(nullptr), mCallbackUser(nullptr)
    {
    }

    PhysicsWorld2D::~PhysicsWorld2D()
    {
        clear();
        if (gActiveWorld == this)
            gActiveWorld = nullptr;
    }

    void PhysicsWorld2D::SetActive(PhysicsWorld2D *world)
    {
        gActiveWorld = world;
    }

    PhysicsWorld2D *PhysicsWorld2D::Active()
    {
        return gActiveWorld;
    }

    GameObject *PhysicsWorld2D::objectFor(const kx::Body *body)
    {
        return body ? static_cast<GameObject *>(body->UserData()) : nullptr;
    }

    void PhysicsWorld2D::setCollisionCallback(CollisionCallback callback, void *user)
    {
        mCallback = callback;
        mCallbackUser = user;
    }

    void PhysicsWorld2D::clear()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D *rigidBody = mBodies[i];
            if (!rigidBody)
                continue;
            if (rigidBody->mBody)
                mWorld.Destroy(rigidBody->mBody);
            rigidBody->mBody = nullptr;
            rigidBody->mWorld = nullptr;
        }
        mBodies.clear();

        for (size_t i = 0; i < mPending.size(); ++i)
            if (mPending[i])
                mPending[i]->mWorld = nullptr;
        mPending.clear();
        mAccumulator = 0.0f;
    }

    void PhysicsWorld2D::build(GameObject &root)
    {
        clear();
        collect(root);
        SetActive(this);
    }

    void PhysicsWorld2D::attach(GameObject &object)
    {
        collect(object);
    }

    void PhysicsWorld2D::detach(RigidBody2D &rigidBody)
    {
        for (size_t i = 0; i < mPending.size(); ++i)
        {
            if (mPending[i] == &rigidBody)
            {
                mPending.erase(mPending.begin() + i);
                break;
            }
        }

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            if (mBodies[i] != &rigidBody)
                continue;
            mBodies.erase(mBodies.begin() + i);
            break;
        }

        if (rigidBody.mBody)
            mWorld.Destroy(rigidBody.mBody);
        rigidBody.mBody = nullptr;
        rigidBody.mWorld = nullptr;
    }

    bool PhysicsWorld2D::markDirty(RigidBody2D &rigidBody)
    {
        rigidBody.mNeedsRebuild = true;
        return rigidBody.mBody != nullptr;
    }

    void PhysicsWorld2D::drainPending()
    {
        if (mPending.empty())
            return;

        ct::Vector<RigidBody2D *> pending;
        for (size_t i = 0; i < mPending.size(); ++i)
            pending.push_back(mPending[i]);
        mPending.clear();

        for (size_t i = 0; i < pending.size(); ++i)
        {
            RigidBody2D *rigidBody = pending[i];
            if (!rigidBody || rigidBody->mBody || !rigidBody->owner())
                continue;
            createBody(*rigidBody->owner(), *rigidBody);
        }
    }

    bool PhysicsWorld2D::needsRebuild(const RigidBody2D &rigidBody) const
    {
        if (rigidBody.mNeedsRebuild)
            return true;

        const GameObject *object = rigidBody.owner();
        if (!object)
            return false;

        const size_t count = object->componentCount<Collider2D>();
        if (count != rigidBody.mColliderCount)
            return true;

        for (size_t i = 0; i < count; ++i)
        {
            const Collider2D *collider = object->getComponentAt<Collider2D>(i);
            if (collider && collider->mDirty)
                return true;
        }
        return false;
    }

    void PhysicsWorld2D::rebuildChanged()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D *rigidBody = mBodies[i];
            if (rigidBody && rigidBody->mBody && needsRebuild(*rigidBody))
                rebuildBody(*rigidBody);
        }
    }

    void PhysicsWorld2D::rebuildBody(RigidBody2D &rigidBody)
    {
        GameObject *object = rigidBody.owner();
        if (!object || !rigidBody.mBody)
            return;

        const Math::Vec2 velocity = rigidBody.mBody->Velocity();
        const float angularVelocity = rigidBody.mBody->AngularVelocity();

        mWorld.Destroy(rigidBody.mBody);
        rigidBody.mBody = nullptr;
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            if (mBodies[i] == &rigidBody)
            {
                mBodies.erase(mBodies.begin() + i);
                break;
            }
        }

        rigidBody.mNeedsRebuild = false;
        createBody(*object, rigidBody);

        if (rigidBody.mBody)
        {
            rigidBody.mBody->SetVelocity(velocity);
            rigidBody.mBody->SetAngularVelocity(angularVelocity);
        }
    }

    void PhysicsWorld2D::collect(GameObject &object)
    {
        const size_t count = object.componentCount<RigidBody2D>();
        for (size_t i = 0; i < count; ++i)
        {
            RigidBody2D *rigidBody = object.getComponentAt<RigidBody2D>(i);
            if (!rigidBody || rigidBody->mBody)
                continue;
            createBody(object, *rigidBody);
        }

        for (size_t i = 0; i < object.childCount(); ++i)
            collect(*object.child(i));
    }

    void PhysicsWorld2D::createBody(GameObject &object, RigidBody2D &rigidBody)
    {
        const Math::Vec2 position = object.globalPosition();
        kx::Body *body = mWorld.CreateBody(rigidBody.bodyType(), position,
                                           object.rotationDegrees() * kDegToRad);
        if (!body)
            return;

        body->SetUserData(&object);
        body->SetFriction(rigidBody.friction());
        body->SetRestitution(rigidBody.restitution());
        body->SetLinearDamping(rigidBody.linearDamping());
        body->SetAngularDamping(rigidBody.angularDamping());
        body->SetGravityScale(rigidBody.gravityScale());
        body->SetBullet(rigidBody.bullet());
        body->SetContactCallback(&PhysicsWorld2D::onContact, this);

        rigidBody.mBody = body;
        rigidBody.mWorld = this;
        mBodies.push_back(&rigidBody);

        attachColliders(object, rigidBody);
        body->SetFixedRotation(rigidBody.fixedRotation());

        if (rigidBody.mHasPendingVelocity)
        {
            body->SetVelocity(rigidBody.mPendingVelocity);
            rigidBody.mHasPendingVelocity = false;
        }
        if (rigidBody.mHasPendingAngularVelocity)
        {
            body->SetAngularVelocity(rigidBody.mPendingAngularVelocity * kDegToRad);
            rigidBody.mHasPendingAngularVelocity = false;
        }
    }

    void PhysicsWorld2D::attachColliders(GameObject &object, RigidBody2D &rigidBody)
    {
        kx::Body *body = rigidBody.mBody;
        if (!body)
            return;

        const Math::Vec2 scale = object.scale();
        const float scaleX = std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f;
        const float scaleY = std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f;

        const size_t count = object.componentCount<Collider2D>();
        rigidBody.mColliderCount = count;
        for (size_t i = 0; i < count; ++i)
        {
            Collider2D *collider = object.getComponentAt<Collider2D>(i);
            if (!collider)
                continue;

            const int shapesBefore = body->ShapeCount();
            switch (collider->shape())
            {
            case ColliderShape::Circle:
            {
                const float radius = collider->radius() * (scaleX > scaleY ? scaleX : scaleY);
                body->AddCircle(collider->offset(),
                                radius > kMinShapeExtent ? radius : kMinShapeExtent,
                                rigidBody.density());
                break;
            }
            case ColliderShape::Polygon:
            {
                const ct::Vector<Math::Vec2> &points = collider->points();
                if (points.size() >= 3)
                {
                    ct::Vector<Math::Vec2> scaled;
                    for (size_t p = 0; p < points.size(); ++p)
                        scaled.push_back(Math::Vec2(points[p].x * scaleX + collider->offset().x,
                                                    points[p].y * scaleY + collider->offset().y));
                    body->AddPolygon(scaled.data(), (int)scaled.size(), rigidBody.density());
                }
                break;
            }
            default:
            {
                float halfWidth = collider->size().x * 0.5f * scaleX;
                float halfHeight = collider->size().y * 0.5f * scaleY;
                if (halfWidth < kMinShapeExtent)
                    halfWidth = kMinShapeExtent;
                if (halfHeight < kMinShapeExtent)
                    halfHeight = kMinShapeExtent;
                body->AddBox(halfWidth, halfHeight, collider->offset(), rigidBody.density());
                break;
            }
            }

            const int index = body->ShapeCount() > shapesBefore ? body->ShapeCount() - 1 : -1;
            collider->mShapeIndex = index;
            collider->mDirty = false;
            if (index >= 0)
            {
                body->SetSensor(index, collider->isSensor());
                body->SetShapeUserData(index, collider);
                body->SetShapeFilter(index, collider->category(), collider->mask());
            }
        }

        if (body->ShapeCount() == 0)
        {
            const float half = 16.0f;
            const int index = body->AddBox(half * scaleX, half * scaleY, Math::Vec2(0.0f, 0.0f),
                                           rigidBody.density());
            (void)index;
        }
    }

    void PhysicsWorld2D::pushTransforms()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D *rigidBody = mBodies[i];
            if (!rigidBody || !rigidBody->mBody)
                continue;
            GameObject *object = objectFor(rigidBody->mBody);
            if (!object)
                continue;

            if (rigidBody->bodyType() == kx::BodyType::Dynamic)
                continue;

            rigidBody->mBody->SetPosition(object->globalPosition());
            rigidBody->mBody->SetAngle(object->rotationDegrees() * kDegToRad);
        }
    }

    void PhysicsWorld2D::pullTransforms()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D *rigidBody = mBodies[i];
            if (!rigidBody || !rigidBody->mBody)
                continue;
            if (rigidBody->bodyType() == kx::BodyType::Static)
                continue;

            GameObject *object = objectFor(rigidBody->mBody);
            if (!object)
                continue;

            const Math::Vec2 worldPosition = rigidBody->mBody->Position();
            const float worldAngle = rigidBody->mBody->Angle() * kRadToDeg;

            GameObject *parent = object->parent();
            if (parent && parent->parent())
            {
                const Math::Vec2 parentPosition = parent->globalPosition();
                object->setPosition(Math::Vec2(worldPosition.x - parentPosition.x,
                                               worldPosition.y - parentPosition.y));
            }
            else
            {
                object->setPosition(worldPosition);
            }
            object->setRotationDegrees(worldAngle);
        }
    }

    void PhysicsWorld2D::step(float deltaTime)
    {
        drainPending();
        rebuildChanged();

        if (deltaTime <= 0.0f || mBodies.empty())
            return;

        pushTransforms();

        if (mFixedStep <= 0.0f)
        {
            mWorld.Step(deltaTime);
        }
        else
        {
            mAccumulator += deltaTime;
            const float maxAccumulated = mFixedStep * 8.0f;
            if (mAccumulator > maxAccumulated)
                mAccumulator = maxAccumulated;
            while (mAccumulator >= mFixedStep)
            {
                mWorld.Step(mFixedStep);
                mAccumulator -= mFixedStep;
            }
        }

        pullTransforms();
    }

    void PhysicsWorld2D::onContact(const kx::ContactEvent &event, void *context)
    {
        PhysicsWorld2D &world = *static_cast<PhysicsWorld2D *>(context);
        if (!world.mCallback || event.phase == kx::ContactPhase::Persist)
            return;

        CollisionInfo info;
        info.self = objectFor(event.self);
        info.other = objectFor(event.other);
        info.sensor = event.sensor;
        info.began = event.phase == kx::ContactPhase::Begin;
        if (event.manifold && event.manifold->pointCount > 0 && event.self && event.other)
        {
            const float radiusSelf = kx::ShapeRadius(event.self->Shapes()[event.shapeIndexSelf]);
            const float radiusOther = kx::ShapeRadius(event.other->Shapes()[event.shapeIndexOther]);
            kx::WorldManifold worldManifold;
            worldManifold.Initialize(event.manifold, event.self->GetTransform(), radiusSelf,
                                     event.other->GetTransform(), radiusOther);
            info.point = worldManifold.points[0];
            info.normal = worldManifold.normal;
        }

        if (info.self && info.other)
            world.mCallback(info, world.mCallbackUser);
    }

    GameObject *PhysicsWorld2D::raycast(const Math::Vec2 &origin, const Math::Vec2 &direction,
                                        float distance, Math::Vec2 *outPoint, Math::Vec2 *outNormal,
                                        const GameObject *ignore)
    {
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length < 0.0001f || distance <= 0.0f)
            return nullptr;

        const Math::Vec2 translation(direction.x / length * distance, direction.y / length * distance);

        const kx::Body *ignoreBody = nullptr;
        if (ignore)
        {
            for (size_t i = 0; i < mBodies.size(); ++i)
                if (mBodies[i] && mBodies[i]->mBody && objectFor(mBodies[i]->mBody) == ignore)
                    ignoreBody = mBodies[i]->mBody;
        }

        kx::RayCastHit hit;
        if (!mWorld.RayCastClosest(origin, translation, hit, 0xFFFF, false, ignoreBody))
            return nullptr;

        if (outPoint)
            *outPoint = hit.point;
        if (outNormal)
            *outNormal = hit.normal;
        return objectFor(hit.body);
    }

    GameObject *PhysicsWorld2D::objectAtPoint(const Math::Vec2 &point)
    {
        return objectFor(mWorld.BodyAtPoint(point, false));
    }

    void PhysicsWorld2D::overlapCircle(const Math::Vec2 &center, float radius,
                                       ct::Vector<GameObject *> &out)
    {
        out.clear();
        if (radius <= 0.0f)
            return;

        ct::Vector<kx::Body *> bodies;
        mWorld.QueryCircle(center, radius, bodies);
        for (size_t i = 0; i < bodies.size(); ++i)
            if (GameObject *object = objectFor(bodies[i]))
                out.push_back(object);
    }

}
