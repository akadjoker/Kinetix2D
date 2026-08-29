#include "k2d/Physics2DSerializer.h"

#include "k2d/BoxCollider2D.h"
#include "k2d/ChainCollider2D.h"
#include "k2d/CharacterBody2D.h"
#include "k2d/CircleCollider2D.h"
#include "k2d/DistanceJoint2D.h"
#include "k2d/EdgeCollider2D.h"
#include "k2d/GameObject.h"
#include "k2d/GearJoint2D.h"
#include "k2d/Joint2D.h"
#include "k2d/MotorJoint2D.h"
#include "k2d/MouseJoint2D.h"
#include "k2d/PolygonCollider2D.h"
#include "k2d/RevoluteJoint2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Serializer.h"
#include "k2d/TileMapCollider2D.h"
#include "k2d/WheelJoint2D.h"

#include <cstring>

namespace k2d
{

namespace
{
ct::Json writeVec2(const Math::Vec2& v)
{
    ct::Json a = ct::Json::array();
    a.push_back(ct::Json(v.x));
    a.push_back(ct::Json(v.y));
    return a;
}

Math::Vec2 readVec2(const ct::Json& j, const Math::Vec2& fallback)
{
    if (!j.is_array() || j.size() < 2)
        return fallback;
    return Math::Vec2((float)j[0].as_double(fallback.x), (float)j[1].as_double(fallback.y));
}

ct::Json writePoints(const ct::Vector<Math::Vec2>& points)
{
    ct::Json a = ct::Json::array();
    for (size_t i = 0; i < points.size(); ++i)
        a.push_back(writeVec2(points[i]));
    return a;
}

void readPoints(const ct::Json& j, ct::Vector<Math::Vec2>& out)
{
    out.clear();
    if (!j.is_array())
        return;
    for (size_t i = 0; i < j.size(); ++i)
        out.push_back(readVec2(j[i], Math::Vec2(0.0f, 0.0f)));
}

const char* bodyTypeName(k2d::BodyType type)
{
    switch (type)
    {
    case k2d::BodyType::Static:
        return "static";
    case k2d::BodyType::Kinematic:
        return "kinematic";
    default:
        return "dynamic";
    }
}

k2d::BodyType bodyTypeFrom(const char* name)
{
    if (std::strcmp(name, "static") == 0)
        return k2d::BodyType::Static;
    if (std::strcmp(name, "kinematic") == 0)
        return k2d::BodyType::Kinematic;
    return k2d::BodyType::Dynamic;
}

Component* createRigidBody(GameObject& owner)
{
    return owner.addComponent<RigidBody2D>();
}

void writeRigidBody(const Component& component, ct::Json& data, Assets*)
{
    const RigidBody2D& body = static_cast<const RigidBody2D&>(component);
    data.set("bodyType", ct::Json(bodyTypeName(body.bodyType())));
    data.set("density", ct::Json((double)body.density()));
    data.set("friction", ct::Json((double)body.friction()));
    data.set("restitution", ct::Json((double)body.restitution()));
    data.set("linearDamping", ct::Json((double)body.linearDamping()));
    data.set("angularDamping", ct::Json((double)body.angularDamping()));
    data.set("gravityScale", ct::Json((double)body.gravityScale()));
    data.set("fixedRotation", ct::Json(body.fixedRotation()));
    data.set("bullet", ct::Json(body.bullet()));
}

void readRigidBody(Component& component, const ct::Json& data, Assets*)
{
    RigidBody2D& body = static_cast<RigidBody2D&>(component);
    body.setBodyType(bodyTypeFrom(data["bodyType"].as_cstr("dynamic")));
    body.setDensity((float)data["density"].as_double(1.0));
    body.setFriction((float)data["friction"].as_double(0.3));
    body.setRestitution((float)data["restitution"].as_double(0.0));
    body.setLinearDamping((float)data["linearDamping"].as_double(0.0));
    body.setAngularDamping((float)data["angularDamping"].as_double(0.0));
    body.setGravityScale((float)data["gravityScale"].as_double(1.0));
    body.setFixedRotation(data["fixedRotation"].as_bool(false));
    body.setBullet(data["bullet"].as_bool(false));
}

bool isRigidBody(const Component& component)
{
    return dynamic_cast<const RigidBody2D*>(&component) != nullptr;
}

Component* createCharacterBody(GameObject& owner)
{
    return owner.addComponent<CharacterBody2D>();
}

void writeCharacterBody(const Component& component, ct::Json& data, Assets*)
{
    const CharacterBody2D& body = static_cast<const CharacterBody2D&>(component);
    data.set("velocity", writeVec2(body.velocity()));
    data.set("safeMargin", ct::Json((double)body.safeMargin()));
    data.set("maxSlides", ct::Json((int64_t)body.maxSlides()));
    data.set("motionMode",
             ct::Json(body.motionMode() == CharacterBody2D::MotionMode::Grounded ? "grounded" : "floating"));
    data.set("upDirection", writeVec2(body.upDirection()));
    data.set("floorMaxAngle", ct::Json((double)body.floorMaxAngleDegrees()));
}

void readCharacterBody(Component& component, const ct::Json& data, Assets*)
{
    CharacterBody2D& body = static_cast<CharacterBody2D&>(component);
    body.setVelocity(readVec2(data["velocity"], Math::Vec2(0.0f, 0.0f)));
    body.setSafeMargin((float)data["safeMargin"].as_double(k2d::kLinearSlop));
    body.setMaxSlides((int)data["maxSlides"].as_int(4));
    body.setMotionMode(std::strcmp(data["motionMode"].as_cstr("floating"), "grounded") == 0
                           ? CharacterBody2D::MotionMode::Grounded
                           : CharacterBody2D::MotionMode::Floating);
    body.setUpDirection(readVec2(data["upDirection"], Math::Vec2(0.0f, -1.0f)));
    body.setFloorMaxAngleDegrees((float)data["floorMaxAngle"].as_double(45.0));
}

bool isCharacterBody(const Component& component)
{
    return dynamic_cast<const CharacterBody2D*>(&component) != nullptr;
}

void writeShared(const Collider2D& collider, ct::Json& data)
{
    data.set("offset", writeVec2(collider.offset()));
    data.set("sensor", ct::Json(collider.isSensor()));
    data.set("category", ct::Json((int64_t)collider.category()));
    data.set("mask", ct::Json((int64_t)collider.mask()));
}

void readShared(Collider2D& collider, const ct::Json& data)
{
    collider.setOffset(readVec2(data["offset"], Math::Vec2(0.0f, 0.0f)));
    collider.setSensor(data["sensor"].as_bool(false));
    collider.setFilter((uint16_t)data["category"].as_int(1), (uint16_t)data["mask"].as_int(0xFFFF));
}

Component* createBox(GameObject& owner)
{
    return owner.addComponent<BoxCollider2D>();
}

void writeBox(const Component& component, ct::Json& data, Assets*)
{
    const BoxCollider2D& box = static_cast<const BoxCollider2D&>(component);
    writeShared(box, data);
    data.set("size", writeVec2(box.size()));
}

void readBox(Component& component, const ct::Json& data, Assets*)
{
    BoxCollider2D& box = static_cast<BoxCollider2D&>(component);
    readShared(box, data);
    box.setSize(readVec2(data["size"], Math::Vec2(32.0f, 32.0f)));
}

bool isBox(const Component& component)
{
    return dynamic_cast<const BoxCollider2D*>(&component) != nullptr;
}

Component* createCircle(GameObject& owner)
{
    return owner.addComponent<CircleCollider2D>();
}

void writeCircle(const Component& component, ct::Json& data, Assets*)
{
    const CircleCollider2D& circle = static_cast<const CircleCollider2D&>(component);
    writeShared(circle, data);
    data.set("radius", ct::Json((double)circle.radius()));
}

void readCircle(Component& component, const ct::Json& data, Assets*)
{
    CircleCollider2D& circle = static_cast<CircleCollider2D&>(component);
    readShared(circle, data);
    circle.setRadius((float)data["radius"].as_double(16.0));
}

bool isCircle(const Component& component)
{
    return dynamic_cast<const CircleCollider2D*>(&component) != nullptr;
}

Component* createEdge(GameObject& owner)
{
    return owner.addComponent<EdgeCollider2D>();
}

void writeEdge(const Component& component, ct::Json& data, Assets*)
{
    const EdgeCollider2D& edge = static_cast<const EdgeCollider2D&>(component);
    writeShared(edge, data);
    data.set("start", writeVec2(edge.start()));
    data.set("end", writeVec2(edge.end()));
}

void readEdge(Component& component, const ct::Json& data, Assets*)
{
    EdgeCollider2D& edge = static_cast<EdgeCollider2D&>(component);
    readShared(edge, data);
    edge.setPoints(readVec2(data["start"], Math::Vec2(-32.0f, 0.0f)), readVec2(data["end"], Math::Vec2(32.0f, 0.0f)));
}

bool isEdge(const Component& component)
{
    return dynamic_cast<const EdgeCollider2D*>(&component) != nullptr;
}

Component* createPolygon(GameObject& owner)
{
    return owner.addComponent<PolygonCollider2D>();
}

void writePolygon(const Component& component, ct::Json& data, Assets*)
{
    const PolygonCollider2D& polygon = static_cast<const PolygonCollider2D&>(component);
    writeShared(polygon, data);
    data.set("points", writePoints(polygon.points()));
}

void readPolygon(Component& component, const ct::Json& data, Assets*)
{
    PolygonCollider2D& polygon = static_cast<PolygonCollider2D&>(component);
    readShared(polygon, data);
    ct::Vector<Math::Vec2> points;
    readPoints(data["points"], points);
    if (points.size() >= 3)
        polygon.setPoints(points.data(), (int)points.size());
}

bool isPolygon(const Component& component)
{
    return dynamic_cast<const PolygonCollider2D*>(&component) != nullptr;
}

Component* createChain(GameObject& owner)
{
    return owner.addComponent<ChainCollider2D>();
}

void writeChain(const Component& component, ct::Json& data, Assets*)
{
    const ChainCollider2D& chain = static_cast<const ChainCollider2D&>(component);
    writeShared(chain, data);
    data.set("points", writePoints(chain.points()));
    data.set("loop", ct::Json(chain.loop()));
}

void readChain(Component& component, const ct::Json& data, Assets*)
{
    ChainCollider2D& chain = static_cast<ChainCollider2D&>(component);
    readShared(chain, data);
    ct::Vector<Math::Vec2> points;
    readPoints(data["points"], points);
    if (points.size() >= 2)
        chain.setPoints(points.data(), (int)points.size());
    chain.setLoop(data["loop"].as_bool(false));
}

bool isChain(const Component& component)
{
    return dynamic_cast<const ChainCollider2D*>(&component) != nullptr;
}

Component* createTileMapCollider(GameObject& owner)
{
    return owner.addComponent<TileMapCollider2D>();
}

void writeTileMapCollider(const Component& component, ct::Json& data, Assets*)
{
    writeShared(static_cast<const TileMapCollider2D&>(component), data);
}

void readTileMapCollider(Component& component, const ct::Json& data, Assets*)
{
    readShared(static_cast<TileMapCollider2D&>(component), data);
}

bool isTileMapCollider(const Component& component)
{
    return dynamic_cast<const TileMapCollider2D*>(&component) != nullptr;
}

void writeJointShared(const Joint2D& joint, ct::Json& data)
{
    data.set("target", ct::Json(joint.targetName().c_str()));
    data.set("collideConnected", ct::Json(joint.collideConnected()));
}

void readJointShared(Joint2D& joint, const ct::Json& data)
{
    joint.setTargetName(data["target"].as_cstr(""));
    joint.setCollideConnected(data["collideConnected"].as_bool(false));
}

Component* createDistanceJoint(GameObject& owner)
{
    return owner.addComponent<DistanceJoint2D>();
}

void writeDistanceJoint(const Component& component, ct::Json& data, Assets*)
{
    const DistanceJoint2D& joint = static_cast<const DistanceJoint2D&>(component);
    writeJointShared(joint, data);
    data.set("localAnchorA", writeVec2(joint.localAnchorA()));
    data.set("localAnchorB", writeVec2(joint.localAnchorB()));
    data.set("length", ct::Json((double)joint.length()));
    data.set("minLength", ct::Json((double)joint.minLength()));
    data.set("maxLength", ct::Json((double)joint.maxLength()));
    data.set("springFrequency", ct::Json((double)joint.springFrequency()));
    data.set("springDamping", ct::Json((double)joint.springDamping()));
    data.set("lengthConfigured", ct::Json(joint.lengthConfigured()));
}

void readDistanceJoint(Component& component, const ct::Json& data, Assets*)
{
    DistanceJoint2D& joint = static_cast<DistanceJoint2D&>(component);
    readJointShared(joint, data);
    joint.setLocalAnchorA(readVec2(data["localAnchorA"], Math::Vec2(0.0f, 0.0f)));
    joint.setLocalAnchorB(readVec2(data["localAnchorB"], Math::Vec2(0.0f, 0.0f)));
    joint.setLength((float)data["length"].as_double(100.0));
    joint.setLengthRange((float)data["minLength"].as_double(100.0), (float)data["maxLength"].as_double(100.0));
    joint.setSpring((float)data["springFrequency"].as_double(0.0), (float)data["springDamping"].as_double(0.0));
    joint.setLengthConfigured(data["lengthConfigured"].as_bool(true));
}

bool isDistanceJoint(const Component& component)
{
    return dynamic_cast<const DistanceJoint2D*>(&component) != nullptr;
}

Component* createRevoluteJoint(GameObject& owner)
{
    return owner.addComponent<RevoluteJoint2D>();
}

void writeRevoluteJoint(const Component& component, ct::Json& data, Assets*)
{
    const RevoluteJoint2D& joint = static_cast<const RevoluteJoint2D&>(component);
    writeJointShared(joint, data);
    data.set("localAnchorA", writeVec2(joint.localAnchorA()));
    data.set("localAnchorB", writeVec2(joint.localAnchorB()));
    data.set("referenceAngle", ct::Json((double)joint.referenceAngle()));
    data.set("motorEnabled", ct::Json(joint.motorEnabled()));
    data.set("motorSpeed", ct::Json((double)joint.motorSpeed()));
    data.set("maxMotorTorque", ct::Json((double)joint.maxMotorTorque()));
    data.set("limitEnabled", ct::Json(joint.limitEnabled()));
    data.set("lowerAngle", ct::Json((double)joint.lowerAngle()));
    data.set("upperAngle", ct::Json((double)joint.upperAngle()));
    data.set("anchorsConfigured", ct::Json(joint.anchorsConfigured()));
}

void readRevoluteJoint(Component& component, const ct::Json& data, Assets*)
{
    RevoluteJoint2D& joint = static_cast<RevoluteJoint2D&>(component);
    readJointShared(joint, data);
    joint.setLocalAnchorA(readVec2(data["localAnchorA"], Math::Vec2(0.0f, 0.0f)));
    joint.setLocalAnchorB(readVec2(data["localAnchorB"], Math::Vec2(0.0f, 0.0f)));
    joint.setReferenceAngle((float)data["referenceAngle"].as_double(0.0));
    joint.setMotor(data["motorEnabled"].as_bool(false), (float)data["motorSpeed"].as_double(0.0),
                  (float)data["maxMotorTorque"].as_double(0.0));
    joint.setLimits(data["limitEnabled"].as_bool(false), (float)data["lowerAngle"].as_double(0.0),
                    (float)data["upperAngle"].as_double(0.0));
    joint.setAnchorsConfigured(data["anchorsConfigured"].as_bool(true));
}

bool isRevoluteJoint(const Component& component)
{
    return dynamic_cast<const RevoluteJoint2D*>(&component) != nullptr;
}

Component* createWheelJoint(GameObject& owner)
{
    return owner.addComponent<WheelJoint2D>();
}

void writeWheelJoint(const Component& component, ct::Json& data, Assets*)
{
    const WheelJoint2D& joint = static_cast<const WheelJoint2D&>(component);
    writeJointShared(joint, data);
    data.set("localAnchorA", writeVec2(joint.localAnchorA()));
    data.set("localAnchorB", writeVec2(joint.localAnchorB()));
    data.set("localAxisA", writeVec2(joint.localAxisA()));
    data.set("anchorsConfigured", ct::Json(joint.anchorsConfigured()));
    data.set("motorEnabled", ct::Json(joint.motorEnabled()));
    data.set("motorSpeed", ct::Json((double)joint.motorSpeed()));
    data.set("maxMotorTorque", ct::Json((double)joint.maxMotorTorque()));
    data.set("springFrequency", ct::Json((double)joint.springFrequency()));
    data.set("springDamping", ct::Json((double)joint.springDamping()));
}

void readWheelJoint(Component& component, const ct::Json& data, Assets*)
{
    WheelJoint2D& joint = static_cast<WheelJoint2D&>(component);
    readJointShared(joint, data);
    joint.setLocalAnchorA(readVec2(data["localAnchorA"], Math::Vec2(0.0f, 0.0f)));
    joint.setLocalAnchorB(readVec2(data["localAnchorB"], Math::Vec2(0.0f, 0.0f)));
    joint.setLocalAxisA(readVec2(data["localAxisA"], Math::Vec2(0.0f, 1.0f)));
    joint.setMotor(data["motorEnabled"].as_bool(false), (float)data["motorSpeed"].as_double(0.0),
                  (float)data["maxMotorTorque"].as_double(0.0));
    joint.setSpring((float)data["springFrequency"].as_double(4.0), (float)data["springDamping"].as_double(0.7));
    joint.setAnchorsConfigured(data["anchorsConfigured"].as_bool(true));
}

bool isWheelJoint(const Component& component)
{
    return dynamic_cast<const WheelJoint2D*>(&component) != nullptr;
}

Component* createMotorJoint(GameObject& owner)
{
    return owner.addComponent<MotorJoint2D>();
}

void writeMotorJoint(const Component& component, ct::Json& data, Assets*)
{
    const MotorJoint2D& joint = static_cast<const MotorJoint2D&>(component);
    writeJointShared(joint, data);
    data.set("linearOffset", writeVec2(joint.linearOffset()));
    data.set("angularOffset", ct::Json((double)joint.angularOffset()));
    data.set("maxForce", ct::Json((double)joint.maxForce()));
    data.set("maxTorque", ct::Json((double)joint.maxTorque()));
    data.set("correctionFactor", ct::Json((double)joint.correctionFactor()));
}

void readMotorJoint(Component& component, const ct::Json& data, Assets*)
{
    MotorJoint2D& joint = static_cast<MotorJoint2D&>(component);
    readJointShared(joint, data);
    joint.setLinearOffset(readVec2(data["linearOffset"], Math::Vec2(0.0f, 0.0f)));
    joint.setAngularOffset((float)data["angularOffset"].as_double(0.0));
    joint.setMaxForce((float)data["maxForce"].as_double(1.0));
    joint.setMaxTorque((float)data["maxTorque"].as_double(1.0));
    joint.setCorrectionFactor((float)data["correctionFactor"].as_double(0.3));
}

bool isMotorJoint(const Component& component)
{
    return dynamic_cast<const MotorJoint2D*>(&component) != nullptr;
}

Component* createMouseJoint(GameObject& owner)
{
    return owner.addComponent<MouseJoint2D>();
}

void writeMouseJoint(const Component& component, ct::Json& data, Assets*)
{
    const MouseJoint2D& joint = static_cast<const MouseJoint2D&>(component);
    data.set("collideConnected", ct::Json(joint.collideConnected()));
    data.set("target", writeVec2(joint.target()));
    data.set("maxForce", ct::Json((double)joint.maxForce()));
    data.set("springFrequency", ct::Json((double)joint.springFrequency()));
    data.set("springDamping", ct::Json((double)joint.springDamping()));
}

void readMouseJoint(Component& component, const ct::Json& data, Assets*)
{
    MouseJoint2D& joint = static_cast<MouseJoint2D&>(component);
    joint.setCollideConnected(data["collideConnected"].as_bool(false));
    joint.setTarget(readVec2(data["target"], Math::Vec2(0.0f, 0.0f)));
    joint.setMaxForce((float)data["maxForce"].as_double(1000.0));
    joint.setSpring((float)data["springFrequency"].as_double(5.0), (float)data["springDamping"].as_double(0.7));
}

bool isMouseJoint(const Component& component)
{
    return dynamic_cast<const MouseJoint2D*>(&component) != nullptr;
}

Component* createGearJoint(GameObject& owner)
{
    return owner.addComponent<GearJoint2D>();
}

void writeGearJoint(const Component& component, ct::Json& data, Assets*)
{
    const GearJoint2D& joint = static_cast<const GearJoint2D&>(component);
    writeJointShared(joint, data);
    data.set("jointATarget", ct::Json(joint.jointATargetName().c_str()));
    data.set("jointAIndex", ct::Json((int64_t)joint.jointAIndex()));
    data.set("jointBTarget", ct::Json(joint.jointBTargetName().c_str()));
    data.set("jointBIndex", ct::Json((int64_t)joint.jointBIndex()));
    data.set("ratio", ct::Json((double)joint.ratio()));
}

void readGearJoint(Component& component, const ct::Json& data, Assets*)
{
    GearJoint2D& joint = static_cast<GearJoint2D&>(component);
    readJointShared(joint, data);
    joint.setJointA(data["jointATarget"].as_cstr(""), (int)data["jointAIndex"].as_int(0));
    joint.setJointB(data["jointBTarget"].as_cstr(""), (int)data["jointBIndex"].as_int(0));
    joint.setRatio((float)data["ratio"].as_double(1.0));
}

bool isGearJoint(const Component& component)
{
    return dynamic_cast<const GearJoint2D*>(&component) != nullptr;
}
} // namespace

void RegisterPhysics2DSerializers()
{
    Serializer::RegisterType(ComponentType::RigidBody, "RigidBody2D", &createRigidBody, &writeRigidBody, &readRigidBody,
                             &isRigidBody);
    Serializer::RegisterType(ComponentType::CharacterBody, "CharacterBody2D", &createCharacterBody, &writeCharacterBody,
                             &readCharacterBody, &isCharacterBody);
    Serializer::RegisterType(ComponentType::Collider, "BoxCollider2D", &createBox, &writeBox, &readBox, &isBox);
    Serializer::RegisterType(ComponentType::Collider, "CircleCollider2D", &createCircle, &writeCircle, &readCircle,
                             &isCircle);
    Serializer::RegisterType(ComponentType::Collider, "EdgeCollider2D", &createEdge, &writeEdge, &readEdge, &isEdge);
    Serializer::RegisterType(ComponentType::Collider, "PolygonCollider2D", &createPolygon, &writePolygon, &readPolygon,
                             &isPolygon);
    Serializer::RegisterType(ComponentType::Collider, "ChainCollider2D", &createChain, &writeChain, &readChain,
                             &isChain);
    Serializer::RegisterType(ComponentType::Collider, "TileMapCollider2D", &createTileMapCollider,
                             &writeTileMapCollider, &readTileMapCollider, &isTileMapCollider);

    Serializer::RegisterType(ComponentType::Joint, "DistanceJoint2D", &createDistanceJoint, &writeDistanceJoint,
                             &readDistanceJoint, &isDistanceJoint);
    Serializer::RegisterType(ComponentType::Joint, "RevoluteJoint2D", &createRevoluteJoint, &writeRevoluteJoint,
                             &readRevoluteJoint, &isRevoluteJoint);
    Serializer::RegisterType(ComponentType::Joint, "WheelJoint2D", &createWheelJoint, &writeWheelJoint,
                             &readWheelJoint, &isWheelJoint);
    Serializer::RegisterType(ComponentType::Joint, "MotorJoint2D", &createMotorJoint, &writeMotorJoint,
                             &readMotorJoint, &isMotorJoint);
    Serializer::RegisterType(ComponentType::Joint, "MouseJoint2D", &createMouseJoint, &writeMouseJoint,
                             &readMouseJoint, &isMouseJoint);
    Serializer::RegisterType(ComponentType::Joint, "GearJoint2D", &createGearJoint, &writeGearJoint, &readGearJoint,
                             &isGearJoint);
}

} // namespace k2d
