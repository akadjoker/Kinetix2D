#include "k2d/Scene.h"

#include "k2d/Collider2D.h"
#include "k2d/GameObject.h"
#include "k2d/Joint2D.h"
#include "k2d/Profiler.h"
#include "k2d/RigidBody2D.h"
#include "k2d/TileMapComponent.h"

#include <cmath>

namespace k2d
{

namespace
{
constexpr float kDegToRad = 0.01745329251994329577f;
constexpr float kRadToDeg = 57.29577951308232088f;
}

void Scene::clearBodies()
{
    while (!mBodies.empty())
        destroyBody(mBodies.back());
    mAccumulator = 0.0f;
    mHasDirtyBodies = false;
}

void Scene::setSimulationEnabled(bool enabled)
{
    clearBodies();
    mSimulationEnabled = enabled;
    if (enabled)
        collectBodies(mRoot);
}

void Scene::setCollisionCallback(CollisionCallback callback, void *user)
{
    mCollisionCallback = callback;
    mCollisionCallbackUser = user;
}

void Scene::markBodyDirty(RigidBody2D &rigidBody)
{
    rigidBody.mNeedsRebuild = true;
    mHasDirtyBodies = true;
}

// Deferred to the next step's rebuild pass so sibling colliders added in the
// same frame exist by the time the body's shapes are built.
void Scene::attachBody(RigidBody2D &rigidBody)
{
    if (rigidBody.inWorld())
        return;
    GameObject *object = rigidBody.owner();
    if (!object)
        return;
    createBody(*object, rigidBody);
}

void Scene::detachBody(RigidBody2D &rigidBody)
{
    if (rigidBody.inWorld())
        destroyBody(&rigidBody);
}

void Scene::collectBodies(GameObject &object)
{
    const size_t count = object.componentCount<RigidBody2D>();
    for (size_t i = 0; i < count; ++i)
    {
        RigidBody2D *rigidBody = object.getComponentAt<RigidBody2D>(i);
        if (rigidBody && !rigidBody->inWorld())
            createBody(object, *rigidBody);
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        collectBodies(*object.child(i));
}

void Scene::createBody(GameObject &object, RigidBody2D &rigidBody)
{
    rigidBody.mPosition = object.globalPosition();
    rigidBody.mAngle = object.rotationDegrees() * kDegToRad;
    rigidBody.mProxyId = kNullNode;
    rigidBody.mProxyPosition = rigidBody.mPosition;

    if (!mFreeBodyIds.empty())
    {
        rigidBody.mId = mFreeBodyIds.back();
        mFreeBodyIds.pop_back();
    }
    else
    {
        rigidBody.mId = mNextBodyId++;
    }

    rigidBody.mBodyIndex = mBodies.size();
    mBodies.push_back(&rigidBody);

    attachColliders(object, rigidBody);
    rigidBody.RecomputeMass();
}

void Scene::attachColliders(GameObject &object, RigidBody2D &rigidBody)
{
    const Math::Vec2 scale = object.scale();
    const float scaleX = std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f;
    const float scaleY = std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f;

    const size_t count = object.componentCount<Collider2D>();
    for (size_t i = 0; i < count; ++i)
    {
        Collider2D *collider = object.getComponentAt<Collider2D>(i);
        if (!collider)
            continue;

        const int shapesBefore = rigidBody.ShapeCount();
        collider->addTo(rigidBody, rigidBody.density(), scaleX, scaleY);
        const int shapesAfter = rigidBody.ShapeCount();

        collider->mShapeIndex = shapesAfter > shapesBefore ? shapesBefore : -1;
        collider->mShapeCount = shapesAfter - shapesBefore;
        collider->mDirty = false;
        for (int shape = shapesBefore; shape < shapesAfter; ++shape)
        {
            rigidBody.SetSensor(shape, collider->isSensor());
            rigidBody.SetShapeUserData(shape, collider);
            rigidBody.SetShapeFilter(shape, collider->category(), collider->mask());
        }
    }

    if (rigidBody.ShapeCount() == 0)
    {
        const float half = 16.0f;
        rigidBody.AddBox(half * scaleX, half * scaleY, Math::Vec2(0.0f, 0.0f), rigidBody.density());
    }
}

void Scene::buildBodyShapes(RigidBody2D &rigidBody)
{
    GameObject *object = rigidBody.owner();
    if (!object)
        return;

    if (rigidBody.mBodyIndex < mBodies.size() && mBodies[rigidBody.mBodyIndex] == &rigidBody)
    {
        rebuildBody(rigidBody);
        return;
    }

    rigidBody.mNeedsRebuild = false;
    rigidBody.ClearShapes();
    attachColliders(*object, rigidBody);
    rigidBody.RecomputeMass();
}

void Scene::rebuildBody(RigidBody2D &rigidBody)
{
    GameObject *object = rigidBody.owner();
    if (!object)
        return;

    rigidBody.mNeedsRebuild = false;

    // The shapes are about to change underneath any contact still touching
    // this body, so its warm-start impulses and pair cache must not survive
    // the rebuild - a stale impulse applied to the new geometry's very
    // different manifold can inject a large spurious velocity. This must run
    // BEFORE the shapes go: the end-event dispatch indexes into Shapes().
    removeBodyContactEvents(&rigidBody);
    rigidBody.ClearShapes();
    mKeyScratch.clear();
    for (auto &entry : mPairs)
        if (entry.value.a == &rigidBody || entry.value.b == &rigidBody)
            mKeyScratch.push_back(entry.key);
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairs.erase(mKeyScratch[i]);

    if (rigidBody.mProxyId != kNullNode)
    {
        for (size_t i = 0; i < mMoveBuffer.size(); ++i)
            if (mMoveBuffer[i] == rigidBody.mProxyId)
                mMoveBuffer[i] = kNullNode;
        mTree.DestroyProxy(rigidBody.mProxyId);
        rigidBody.mProxyId = kNullNode;
    }

    attachColliders(*object, rigidBody);
    rigidBody.RecomputeMass();
}

void Scene::rebuildDirtyBodies()
{
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D *rigidBody = mBodies[i];
        if (rigidBody && rigidBody->mNeedsRebuild)
            rebuildBody(*rigidBody);
    }
    mHasDirtyBodies = false;
}

void Scene::pushTransforms()
{
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D *rigidBody = mBodies[i];
        if (!rigidBody)
            continue;
        GameObject *object = rigidBody->owner();
        if (!object)
            continue;

        if (rigidBody->bodyType() == BodyType::Dynamic)
        {
            // The solver owns a dynamic body's position, but a fixed-rotation
            // body is one it may not turn, so its angle belongs to whoever set
            // it - an agent facing along its path, most of all.
            if (rigidBody->fixedRotation())
                rigidBody->SetAngle(object->globalRotationDegrees() * kDegToRad);
            continue;
        }

        rigidBody->SetPosition(object->globalPosition());
        rigidBody->SetAngle(object->rotationDegrees() * kDegToRad);
    }
}

void Scene::pullTransforms()
{
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D *rigidBody = mBodies[i];
        if (!rigidBody || rigidBody->bodyType() == BodyType::Static)
            continue;

        GameObject *object = rigidBody->owner();
        if (!object)
            continue;

        const Math::Vec2 worldPosition = rigidBody->Position();
        const float worldAngle = rigidBody->Angle() * kRadToDeg;
        const bool keepRotation = rigidBody->bodyType() == BodyType::Dynamic && rigidBody->fixedRotation();

        GameObject *parent = object->parent();
        if (parent && parent->parent())
        {
            // The full inverse, not just the parent's position: a rotated or
            // scaled parent otherwise leaves the body drawn away from where it
            // simulates, and the angle needs the parent's rotation removed too.
            const Math::Vec2 localPosition = parent->globalTransform().AffineInverse().Transform(worldPosition);
            if (keepRotation)
                object->setPosition(localPosition);
            else
                object->setPositionAndRotation(localPosition, worldAngle - parent->globalRotationDegrees());
        }
        else if (keepRotation)
        {
            object->setPosition(worldPosition);
        }
        else
        {
            object->setPositionAndRotation(worldPosition, worldAngle);
        }
    }
}

void Scene::resolveJoints()
{
    for (size_t i = 0; i < mJoints.size(); ++i)
        mJoints[i]->resolve();
}

void Scene::stepBodies(float deltaTime)
{
    if (!mSimulationEnabled)
        return;

    ProfileScope profileScope("scene.step_bodies");
    if (mHasDirtyBodies)
        rebuildDirtyBodies();
    resolveJoints();

    if (deltaTime > 0.0f && !mBodies.empty())
    {
        pushTransforms();

        if (mFixedStep <= 0.0f)
        {
            step(deltaTime);
        }
        else
        {
            mAccumulator += deltaTime;
            const float maxAccumulated = mFixedStep * 8.0f;
            if (mAccumulator > maxAccumulated)
                mAccumulator = maxAccumulated;
            while (mAccumulator >= mFixedStep)
            {
                step(mFixedStep);
                mAccumulator -= mFixedStep;
            }
        }

        pullTransforms();
    }

    mParticles.step(deltaTime);
}

}
