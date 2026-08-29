#include "k2d/Scene.h"

#include "k2d/CanvasRenderer.h"
#include "k2d/Collider2D.h"
#include "k2d/Profiler.h"
#include "k2d/RigidBody2D.h"
#include "k2d/TileMapComponent.h"

#include <kx/debugdraw.h>
#include <kx/tilemapcollider.h>
#include <kx/world.h>

#include <cmath>
#include <limits>

namespace k2d
{

namespace
{
constexpr float kDegToRad = 0.01745329251994329577f;
constexpr float kRadToDeg = 57.29577951308232088f;

class PhysicsRenderDebugDraw final : public kx::DebugDraw
{
  public:
    struct Primitive
    {
        ct::Vector<Math::Vec2> points;
        Color color;
    };

    void DrawCircleShape(const kx::Transform& xf, float radius, kx::Color color) override
    {
        if (radius <= 0.0f)
            return;
        constexpr int segments = 32;
        Math::Vec2 points[segments];
        for (int i = 0; i < segments; ++i)
        {
            const float angle = (float)i * 6.28318530717958647692f / (float)segments;
            points[i] = xf.Transform(Math::Vec2(std::cos(angle) * radius, std::sin(angle) * radius));
        }
        addLoop(points, segments, toColor(color), 1.5f);
        addSegment(xf.Transform(Math::Vec2(0.0f, 0.0f)), xf.Transform(Math::Vec2(radius, 0.0f)), toColor(color), 1.0f);
    }

    void DrawPolygonShape(const kx::Transform& xf, const Math::Vec2* verts, int count, kx::Color color) override
    {
        if (!verts || count < 2)
            return;
        ct::Vector<Math::Vec2> points;
        points.resize((size_t)count);
        for (int i = 0; i < count; ++i)
            points[(size_t)i] = xf.Transform(verts[i]);
        addLoop(points.data(), count, toColor(color), 1.5f);
    }

    void DrawSegment(const Math::Vec2& a, const Math::Vec2& b, kx::Color color) override
    {
        addSegment(a, b, toColor(color), 1.5f);
    }

    void DrawPoint(const Math::Vec2& p, float size, kx::Color color) override
    {
        const float half = size > 0.0f ? size * 0.5f : 0.5f;
        Primitive& primitive = makePrimitive(toColor(color));
        primitive.points.push_back(Math::Vec2(p.x - half, p.y - half));
        primitive.points.push_back(Math::Vec2(p.x + half, p.y - half));
        primitive.points.push_back(Math::Vec2(p.x + half, p.y + half));
        primitive.points.push_back(Math::Vec2(p.x - half, p.y - half));
        primitive.points.push_back(Math::Vec2(p.x + half, p.y + half));
        primitive.points.push_back(Math::Vec2(p.x - half, p.y + half));
    }

    void DrawAABB(const Math::Vec2& lower, const Math::Vec2& upper, kx::Color color) override
    {
        const Math::Vec2 points[4] = {Math::Vec2(lower.x, lower.y), Math::Vec2(upper.x, lower.y),
                                      Math::Vec2(upper.x, upper.y), Math::Vec2(lower.x, upper.y)};
        addLoop(points, 4, toColor(color), 1.0f);
    }

    void flush(RenderQueue& queue)
    {
        if (mPrimitives.empty())
            return;

        RenderItem& item = queue.AddItem(std::numeric_limits<int>::max());
        item.blendMode = BLEND_MIX;
        for (size_t i = 0; i < mPrimitives.size(); ++i)
        {
            Primitive& primitive = mPrimitives[i];
            if (primitive.points.empty())
                continue;
            RenderCommand command;
            command.type = RenderCommand::kPolygon;
            command.color = primitive.color;
            command.polygonPoints = &primitive.points;
            command.polygonPointCount = (unsigned int)primitive.points.size();
            item.commands.push_back(command);
        }
    }

  private:
    static Color toColor(kx::Color color)
    {
        return Color::FromBytes(color.r, color.g, color.b, color.a);
    }

    Primitive& makePrimitive(const Color& color)
    {
        mPrimitives.resize(mPrimitives.size() + 1);
        Primitive& primitive = mPrimitives.back();
        primitive.color = color;
        return primitive;
    }

    void addLoop(const Math::Vec2* points, int count, const Color& color, float width)
    {
        for (int i = 0; i < count; ++i)
            addSegment(points[i], points[(i + 1) % count], color, width);
    }

    void addSegment(const Math::Vec2& a, const Math::Vec2& b, const Color& color, float width)
    {
        Math::Vec2 direction = b - a;
        const float length = direction.Length();
        if (length < 0.0001f)
            return;
        direction /= length;
        const Math::Vec2 normal(-direction.y * width * 0.5f, direction.x * width * 0.5f);
        Primitive& primitive = makePrimitive(color);
        primitive.points.push_back(a - normal);
        primitive.points.push_back(a + normal);
        primitive.points.push_back(b + normal);
        primitive.points.push_back(a - normal);
        primitive.points.push_back(b + normal);
        primitive.points.push_back(b - normal);
    }

    ct::Vector<Primitive> mPrimitives;
};
} // namespace

struct ScenePhysics
{
    kx::World mWorld;
    ct::Vector<RigidBody2D*> mBodies;
    ct::Vector<RigidBody2D*> mPending;
    ct::Vector<RigidBody2D*> mDraining;
    ct::Vector<RigidBody2D*> mDirtyBodies;
    ct::Vector<kx::TileMapCollider*> mTileMaps;
    float mFixedStep;
    float mAccumulator;
    CollisionCallback mCallback;
    void* mCallbackUser;
    bool mEnabled;

    ScenePhysics()
        : mWorld(Math::Vec2(0.0f, 980.0f)), mFixedStep(1.0f / 60.0f), mAccumulator(0.0f), mCallback(nullptr),
          mCallbackUser(nullptr), mEnabled(false)
    {
    }

    void clear()
    {
        for (size_t i = 0; i < mTileMaps.size(); ++i)
            delete mTileMaps[i];
        mTileMaps.clear();

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D* rigidBody = mBodies[i];
            if (!rigidBody)
                continue;
            if (rigidBody->mBody)
                mWorld.Destroy(rigidBody->mBody);
            rigidBody->mBody = nullptr;
            rigidBody->mBodyIndex = RigidBody2D::InvalidWorldIndex;
        }
        mBodies.clear();

        for (size_t i = 0; i < mPending.size(); ++i)
            if (mPending[i])
                mPending[i]->mPendingIndex = RigidBody2D::InvalidWorldIndex;
        mPending.clear();
        mDirtyBodies.clear();
        mAccumulator = 0.0f;
    }

    // Deferred to the next step so sibling colliders added in the same frame exist when the body is built.
    void attach(RigidBody2D& rigidBody)
    {
        if (rigidBody.mBody || rigidBody.mPendingIndex != RigidBody2D::InvalidWorldIndex)
            return;
        rigidBody.mPendingIndex = mPending.size();
        mPending.push_back(&rigidBody);
    }

    void detach(RigidBody2D& rigidBody)
    {
        if (rigidBody.mPendingIndex < mPending.size() && mPending[rigidBody.mPendingIndex] == &rigidBody)
        {
            RigidBody2D* moved = mPending.back();
            mPending[rigidBody.mPendingIndex] = moved;
            moved->mPendingIndex = rigidBody.mPendingIndex;
            mPending.pop_back();
        }
        rigidBody.mPendingIndex = RigidBody2D::InvalidWorldIndex;

        if (rigidBody.mBodyIndex < mBodies.size() && mBodies[rigidBody.mBodyIndex] == &rigidBody)
        {
            RigidBody2D* moved = mBodies.back();
            mBodies[rigidBody.mBodyIndex] = moved;
            moved->mBodyIndex = rigidBody.mBodyIndex;
            mBodies.pop_back();
        }
        rigidBody.mBodyIndex = RigidBody2D::InvalidWorldIndex;

        if (rigidBody.mDirtyIndex < mDirtyBodies.size() && mDirtyBodies[rigidBody.mDirtyIndex] == &rigidBody)
        {
            RigidBody2D* moved = mDirtyBodies.back();
            mDirtyBodies[rigidBody.mDirtyIndex] = moved;
            moved->mDirtyIndex = rigidBody.mDirtyIndex;
            mDirtyBodies.pop_back();
        }
        rigidBody.mDirtyIndex = RigidBody2D::InvalidWorldIndex;

        if (rigidBody.mBody)
            mWorld.Destroy(rigidBody.mBody);
        rigidBody.mBody = nullptr;
    }

    void markDirty(RigidBody2D& rigidBody)
    {
        rigidBody.mNeedsRebuild = true;
        if (rigidBody.mBody && rigidBody.mDirtyIndex == RigidBody2D::InvalidWorldIndex)
        {
            rigidBody.mDirtyIndex = mDirtyBodies.size();
            mDirtyBodies.push_back(&rigidBody);
        }
    }

    void drainPending()
    {
        if (mPending.empty())
            return;

        mDraining.swap(mPending);
        mPending.clear();

        for (size_t i = 0; i < mDraining.size(); ++i)
        {
            RigidBody2D* rigidBody = mDraining[i];
            if (rigidBody)
                rigidBody->mPendingIndex = RigidBody2D::InvalidWorldIndex;
            if (!rigidBody || rigidBody->mBody || !rigidBody->owner())
                continue;
            createBody(*rigidBody->owner(), *rigidBody);
        }
        mDraining.clear();
    }

    bool needsRebuild(const RigidBody2D& rigidBody) const
    {
        if (rigidBody.mNeedsRebuild)
            return true;

        const GameObject* object = rigidBody.owner();
        if (!object)
            return false;

        const size_t count = object->componentCount<Collider2D>();
        if (count != rigidBody.mColliderCount)
            return true;

        for (size_t i = 0; i < count; ++i)
        {
            const Collider2D* collider = object->getComponentAt<Collider2D>(i);
            if (collider && collider->mDirty)
                return true;
        }
        return false;
    }

    void rebuildChanged()
    {
        for (size_t i = 0; i < mDirtyBodies.size(); ++i)
        {
            RigidBody2D* rigidBody = mDirtyBodies[i];
            rigidBody->mDirtyIndex = RigidBody2D::InvalidWorldIndex;
            if (rigidBody && rigidBody->mBody && needsRebuild(*rigidBody))
                rebuildBody(*rigidBody);
        }
        mDirtyBodies.clear();
    }

    void rebuildBody(RigidBody2D& rigidBody)
    {
        GameObject* object = rigidBody.owner();
        if (!object || !rigidBody.mBody)
            return;

        const Math::Vec2 velocity = rigidBody.mBody->Velocity();
        const float angularVelocity = rigidBody.mBody->AngularVelocity();

        mWorld.Destroy(rigidBody.mBody);
        rigidBody.mBody = nullptr;
        const size_t index = rigidBody.mBodyIndex;
        if (index < mBodies.size() && mBodies[index] == &rigidBody)
        {
            RigidBody2D* moved = mBodies.back();
            mBodies[index] = moved;
            moved->mBodyIndex = index;
            mBodies.pop_back();
        }
        rigidBody.mBodyIndex = RigidBody2D::InvalidWorldIndex;

        rigidBody.mNeedsRebuild = false;
        createBody(*object, rigidBody);

        if (rigidBody.mBody)
        {
            rigidBody.mBody->SetVelocity(velocity);
            rigidBody.mBody->SetAngularVelocity(angularVelocity);
        }
    }

    void collect(GameObject& object)
    {
        const size_t mapCount = object.componentCount<TileMapComponent>();
        for (size_t i = 0; i < mapCount; ++i)
        {
            TileMapComponent* tileMap = object.getComponentAt<TileMapComponent>(i);
            if (tileMap)
                createTileMapCollider(object, *tileMap);
        }

        const size_t count = object.componentCount<RigidBody2D>();
        for (size_t i = 0; i < count; ++i)
        {
            RigidBody2D* rigidBody = object.getComponentAt<RigidBody2D>(i);
            if (!rigidBody || rigidBody->mBody)
                continue;
            createBody(object, *rigidBody);
        }

        for (size_t i = 0; i < object.childCount(); ++i)
            collect(*object.child(i));
    }

    void createTileMapCollider(GameObject& object, TileMapComponent& tileMap)
    {
        if (tileMap.columns() <= 0 || tileMap.rows() <= 0)
            return;

        const Math::Vec2 scale = object.scale();
        const float scaleX = std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f;
        const float scaleY = std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f;
        kx::TileMapCollider* collider = new kx::TileMapCollider(mWorld);
        collider->SetMapSize(tileMap.columns(), tileMap.rows());
        collider->SetCellSize(Math::Vec2(tileMap.cellWidth() * scaleX, tileMap.cellHeight() * scaleY));
        collider->SetOffset(object.globalPosition());
        for (int y = 0; y < tileMap.rows(); ++y)
            for (int x = 0; x < tileMap.columns(); ++x)
                collider->SetSolid(x, y, tileMap.hasCollision(x, y));
        collider->Rebuild();

        const ct::Vector<kx::Body*>& bodies = collider->Bodies();
        for (size_t i = 0; i < bodies.size(); ++i)
        {
            bodies[i]->SetUserData(&object);
            bodies[i]->SetContactCallback(&ScenePhysics::onContact, this);
        }
        mTileMaps.push_back(collider);
    }

    void createBody(GameObject& object, RigidBody2D& rigidBody)
    {
        const Math::Vec2 position = object.globalPosition();
        kx::Body* body = mWorld.CreateBody(rigidBody.bodyType(), position, object.rotationDegrees() * kDegToRad);
        if (!body)
            return;

        body->SetUserData(&object);
        body->SetFriction(rigidBody.friction());
        body->SetRestitution(rigidBody.restitution());
        body->SetLinearDamping(rigidBody.linearDamping());
        body->SetAngularDamping(rigidBody.angularDamping());
        body->SetGravityScale(rigidBody.gravityScale());
        body->SetBullet(rigidBody.bullet());
        body->SetContactCallback(&ScenePhysics::onContact, this);

        rigidBody.mBody = body;
        rigidBody.mBodyIndex = mBodies.size();
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

    void attachColliders(GameObject& object, RigidBody2D& rigidBody)
    {
        kx::Body* body = rigidBody.mBody;
        if (!body)
            return;

        const Math::Vec2 scale = object.scale();
        const float scaleX = std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f;
        const float scaleY = std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f;

        const size_t count = object.componentCount<Collider2D>();
        rigidBody.mColliderCount = count;
        for (size_t i = 0; i < count; ++i)
        {
            Collider2D* collider = object.getComponentAt<Collider2D>(i);
            if (!collider)
                continue;

            const int shapesBefore = body->ShapeCount();
            collider->addTo(*body, rigidBody.density(), scaleX, scaleY);
            const int shapesAfter = body->ShapeCount();

            collider->mShapeIndex = shapesAfter > shapesBefore ? shapesBefore : -1;
            collider->mShapeCount = shapesAfter - shapesBefore;
            collider->mDirty = false;
            for (int shape = shapesBefore; shape < shapesAfter; ++shape)
            {
                body->SetSensor(shape, collider->isSensor());
                body->SetShapeUserData(shape, collider);
                body->SetShapeFilter(shape, collider->category(), collider->mask());
            }
        }

        if (body->ShapeCount() == 0)
        {
            const float half = 16.0f;
            const int index = body->AddBox(half * scaleX, half * scaleY, Math::Vec2(0.0f, 0.0f), rigidBody.density());
            (void)index;
        }
    }

    void pushTransforms()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D* rigidBody = mBodies[i];
            if (!rigidBody || !rigidBody->mBody)
                continue;
            GameObject* object = rigidBody->owner();
            if (!object)
                continue;

            if (rigidBody->bodyType() == kx::BodyType::Dynamic)
                continue;

            rigidBody->mBody->SetPosition(object->globalPosition());
            rigidBody->mBody->SetAngle(object->rotationDegrees() * kDegToRad);
        }
    }

    void pullTransforms()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D* rigidBody = mBodies[i];
            if (!rigidBody || !rigidBody->mBody)
                continue;
            if (rigidBody->bodyType() == kx::BodyType::Static)
                continue;

            GameObject* object = rigidBody->owner();
            if (!object)
                continue;

            const Math::Vec2 worldPosition = rigidBody->mBody->Position();
            const float worldAngle = rigidBody->mBody->Angle() * kRadToDeg;

            GameObject* parent = object->parent();
            if (parent && parent->parent())
            {
                const Math::Vec2 parentPosition = parent->globalPosition();
                const Math::Vec2 localPosition(worldPosition.x - parentPosition.x, worldPosition.y - parentPosition.y);
                object->setPositionAndRotation(localPosition, worldAngle);
            }
            else
            {
                object->setPositionAndRotation(worldPosition, worldAngle);
                continue;
            }
        }
    }

    void step(float deltaTime)
    {
        ProfileScope profileScope("physics.step");
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

    GameObject* raycast(const Math::Vec2& origin, const Math::Vec2& direction, float distance, Math::Vec2* outPoint,
                        Math::Vec2* outNormal, const GameObject* ignore)
    {
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length < 0.0001f || distance <= 0.0f)
            return nullptr;

        const Math::Vec2 translation(direction.x / length * distance, direction.y / length * distance);

        const kx::Body* ignoreBody = nullptr;
        if (ignore)
        {
            for (size_t i = 0; i < mBodies.size(); ++i)
                if (mBodies[i] && mBodies[i]->mBody && Scene::objectForBody(mBodies[i]->mBody) == ignore)
                    ignoreBody = mBodies[i]->mBody;
        }

        kx::RayCastHit hit;
        if (!mWorld.RayCastClosest(origin, translation, hit, 0xFFFF, false, ignoreBody))
            return nullptr;

        if (outPoint)
            *outPoint = hit.point;
        if (outNormal)
            *outNormal = hit.normal;
        return Scene::objectForBody(hit.body);
    }

    GameObject* objectAtPoint(const Math::Vec2& point)
    {
        return Scene::objectForBody(mWorld.BodyAtPoint(point, false));
    }

    void overlapCircle(const Math::Vec2& center, float radius, ct::Vector<GameObject*>& out)
    {
        out.clear();
        if (radius <= 0.0f)
            return;

        ct::Vector<kx::Body*> bodies;
        mWorld.QueryCircle(center, radius, bodies);
        for (size_t i = 0; i < bodies.size(); ++i)
            if (GameObject* object = Scene::objectForBody(bodies[i]))
                out.push_back(object);
    }

    static void onContact(const kx::ContactEvent& event, void* context)
    {
        ScenePhysics& physics = *static_cast<ScenePhysics*>(context);
        if (!physics.mCallback || event.phase == kx::ContactPhase::Persist)
            return;

        CollisionInfo info;
        info.self = Scene::objectForBody(event.self);
        info.other = Scene::objectForBody(event.other);
        info.hit = true;
        info.sensor = event.sensor;
        info.began = event.phase == kx::ContactPhase::Begin;
        if (event.manifold && event.manifold->pointCount > 0 && event.self && event.other)
        {
            const float radiusSelf = kx::ShapeRadius(event.self->Shapes()[event.shapeIndexSelf]);
            const float radiusOther = kx::ShapeRadius(event.other->Shapes()[event.shapeIndexOther]);
            kx::WorldManifold worldManifold;
            worldManifold.Initialize(event.manifold, event.self->GetTransform(), radiusSelf, event.other->GetTransform(),
                                     radiusOther);
            info.point = worldManifold.points[0];
            info.normal = worldManifold.normal;
        }

        if (info.self && info.other)
            physics.mCallback(info, physics.mCallbackUser);
    }
};

ScenePhysics *Scene::createPhysics()
{
    return new ScenePhysics();
}

void Scene::clearPhysics()
{
    if (mPhysics)
        mPhysics->clear();
}

void Scene::teardownPhysics()
{
    clearPhysics();
    delete mPhysics;
    mPhysics = nullptr;
}

void Scene::setSimulationEnabled(bool enabled)
{
    if (!mPhysics)
        return;
    mPhysics->clear();
    mPhysics->mEnabled = enabled;
    if (enabled)
        mPhysics->collect(mRoot);
}

bool Scene::simulationEnabled() const
{
    return mPhysics && mPhysics->mEnabled;
}

void Scene::setGravity(const Math::Vec2& gravity)
{
    if (mPhysics)
        mPhysics->mWorld.SetGravity(gravity);
}

Math::Vec2 Scene::gravity() const
{
    return mPhysics ? mPhysics->mWorld.Gravity() : Math::Vec2(0.0f, 0.0f);
}

void Scene::setFixedTimeStep(float seconds)
{
    if (mPhysics)
        mPhysics->mFixedStep = seconds > 0.0f ? seconds : 0.0f;
}

float Scene::fixedTimeStep() const
{
    return mPhysics ? mPhysics->mFixedStep : 0.0f;
}

void Scene::setCollisionCallback(CollisionCallback callback, void* user)
{
    if (!mPhysics)
        return;
    mPhysics->mCallback = callback;
    mPhysics->mCallbackUser = user;
}

GameObject* Scene::raycast(const Math::Vec2& origin, const Math::Vec2& direction, float distance, Math::Vec2* outPoint,
                           Math::Vec2* outNormal, const GameObject* ignore)
{
    return mPhysics ? mPhysics->raycast(origin, direction, distance, outPoint, outNormal, ignore) : nullptr;
}

GameObject* Scene::objectAtPoint(const Math::Vec2& point)
{
    return mPhysics ? mPhysics->objectAtPoint(point) : nullptr;
}

void Scene::overlapCircle(const Math::Vec2& center, float radius, ct::Vector<GameObject*>& out)
{
    if (mPhysics)
        mPhysics->overlapCircle(center, radius, out);
    else
        out.clear();
}

std::size_t Scene::physicsBodyCount() const
{
    return mPhysics ? mPhysics->mWorld.BodyCount() : 0;
}

std::size_t Scene::physicsContactCount() const
{
    return mPhysics ? mPhysics->mWorld.ContactCount() : 0;
}

kx::World* Scene::physicsWorld()
{
    return (mPhysics && mPhysics->mEnabled) ? &mPhysics->mWorld : nullptr;
}

void Scene::debugDrawPhysics(CanvasRenderer& canvas, unsigned flags)
{
    if (!mPhysics || flags == 0)
        return;

    PhysicsRenderDebugDraw draw;
    kx::Draw(mPhysics->mWorld, draw, flags);

    RenderQueue queue;
    draw.flush(queue);
    queue.Flush(canvas);
}

GameObject* Scene::objectForBody(const kx::Body* body)
{
    return body ? static_cast<GameObject*>(body->UserData()) : nullptr;
}

void Scene::markPhysicsDirty(RigidBody2D& rigidBody)
{
    if (mPhysics)
        mPhysics->markDirty(rigidBody);
}

void Scene::attachPhysicsBody(RigidBody2D& rigidBody)
{
    if (mPhysics)
        mPhysics->attach(rigidBody);
}

void Scene::detachPhysicsBody(RigidBody2D& rigidBody)
{
    if (mPhysics)
        mPhysics->detach(rigidBody);
}

void Scene::physicsStep(float deltaTime)
{
    if (mPhysics && mPhysics->mEnabled)
        mPhysics->step(deltaTime);
}

} // namespace k2d
