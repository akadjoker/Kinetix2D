#include "k2d/Physics2DSerializer.h"

#include "k2d/BoxCollider2D.h"
#include "k2d/ChainCollider2D.h"
#include "k2d/CharacterBody2D.h"
#include "k2d/CircleCollider2D.h"
#include "k2d/EdgeCollider2D.h"
#include "k2d/GameObject.h"
#include "k2d/PolygonCollider2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Serializer.h"

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

const char* bodyTypeName(kx::BodyType type)
{
    switch (type)
    {
    case kx::BodyType::Static:
        return "static";
    case kx::BodyType::Kinematic:
        return "kinematic";
    default:
        return "dynamic";
    }
}

kx::BodyType bodyTypeFrom(const char* name)
{
    if (std::strcmp(name, "static") == 0)
        return kx::BodyType::Static;
    if (std::strcmp(name, "kinematic") == 0)
        return kx::BodyType::Kinematic;
    return kx::BodyType::Dynamic;
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
    body.setSafeMargin((float)data["safeMargin"].as_double(kx::kLinearSlop));
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
}

} // namespace k2d
