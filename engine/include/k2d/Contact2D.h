#pragma once

#include <mathc.h>
#include <cstdint>

#include "k2d/Manifold2D.h"

namespace k2d
{

class RigidBody2D;

struct ContactInfo
{
    RigidBody2D* a;
    RigidBody2D* b;
    int shapeIndexA;
    int shapeIndexB;
    Manifold manifold;
    Math::Vec2 normal;
    Math::Vec2 tangent;
    Math::Vec2 rA[kMaxManifoldPoints];
    Math::Vec2 rB[kMaxManifoldPoints];
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

struct BodyPair
{
    RigidBody2D* a;
    RigidBody2D* b;
};

// One entry per touching shape pair, keyed the same way the broadphase pair
// cache is keyed (body ids + shape indices). Holds both the geometry needed
// to dispatch Begin/Persist/End events and the impulses carried over for
// warm-starting - the two hashmaps kx kept separate for the same lifetime.
struct PairContactState
{
    RigidBody2D* a = nullptr;
    RigidBody2D* b = nullptr;
    int shapeIndexA = -1;
    int shapeIndexB = -1;
    Manifold manifold;
    bool sensor = false;
    uint32_t stamp = 0;

    int impulseCount = 0;
    uint32_t idKey[kMaxManifoldPoints] = {0, 0};
    float normalImpulse[kMaxManifoldPoints] = {0.0f, 0.0f};
    float tangentImpulse[kMaxManifoldPoints] = {0.0f, 0.0f};
};

struct StepProfile
{
    float broadphase = 0.0f;
    float narrowphase = 0.0f;
    float solveVelocity = 0.0f;
    float solveVelocityJoints = 0.0f;
    float solveVelocityContacts = 0.0f;
    float solvePosition = 0.0f;
    float integrate = 0.0f;
};

struct RayCastHit
{
    RigidBody2D* body = nullptr;
    int shapeIndex = -1;
    Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
    float fraction = 1.0f;
};

// Result shared by kinematic movement and non-mutating placement tests.
// The normal always points away from the obstacle and toward the tested body.
struct MotionResult
{
    RigidBody2D* body = nullptr;
    int shapeIndexSelf = -1;
    int shapeIndexOther = -1;
    Math::Vec2 travel = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 remainder = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
    float fraction = 1.0f;
    bool hit = false;
};

struct BulletSweep
{
    RigidBody2D* body;
    Math::Vec2 prevCenter;
};

} // namespace k2d
