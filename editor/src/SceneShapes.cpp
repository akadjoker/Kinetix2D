#include "SceneShapes.h"

#include <k2d/BoxCollider2D.h>
#include <k2d/CapsuleShape.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/CircleShape.h>
#include <k2d/EdgeCollider2D.h>
#include <k2d/GameObject.h>
#include <k2d/Line2D.h>
#include <k2d/NinePatchComponent.h>
#include <k2d/Polygon2D.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/RectShape.h>
#include <k2d/SpriteComponent.h>
#include <k2d/TileMapComponent.h>

#include <cmath>

namespace k2d::editor
{

namespace
{

constexpr float kMinimumExtent = 1.0f;

struct Extent
{
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    bool any = false;

    void add(const Math::Vec2& p)
    {
        if (!any)
        {
            minX = maxX = p.x;
            minY = maxY = p.y;
            any = true;
            return;
        }
        minX = p.x < minX ? p.x : minX;
        minY = p.y < minY ? p.y : minY;
        maxX = p.x > maxX ? p.x : maxX;
        maxY = p.y > maxY ? p.y : maxY;
    }

    void addRect(float x, float y, float w, float h)
    {
        add(Math::Vec2(x, y));
        add(Math::Vec2(x + w, y + h));
    }

    void addPoints(const ct::Vector<Math::Vec2>& points, float pad)
    {
        for (const Math::Vec2& p : points)
            addRect(p.x - pad, p.y - pad, pad * 2.0f, pad * 2.0f);
    }
};

template <class T> const T* activeComponent(const GameObject& object)
{
    const T* component = object.getComponent<T>();
    return component && component->active() ? component : nullptr;
}

void addCollider(Extent& extent, const Collider2D& collider, const Math::Vec2& scale, const Matrix2D& toLocal)
{
    const Math::Vec2 o = collider.offset();
    const auto addBody = [&](float x, float y) { extent.add(toLocal.Transform(x, y)); };
    switch (collider.componentClass())
    {
    case ComponentClass::BoxCollider2D:
    {
        const Math::Vec2 size = static_cast<const BoxCollider2D&>(collider).size();
        const float hw = size.x * 0.5f * scale.x, hh = size.y * 0.5f * scale.y;
        addBody(o.x - hw, o.y - hh);
        addBody(o.x + hw, o.y - hh);
        addBody(o.x + hw, o.y + hh);
        addBody(o.x - hw, o.y + hh);
        break;
    }
    case ComponentClass::CircleCollider2D:
    {
        const float r = static_cast<const CircleCollider2D&>(collider).radius() * (scale.x > scale.y ? scale.x : scale.y);
        addBody(o.x - r, o.y - r);
        addBody(o.x + r, o.y - r);
        addBody(o.x + r, o.y + r);
        addBody(o.x - r, o.y + r);
        break;
    }
    default:
        for (int i = 0, count = colliderPointCount(collider); i < count; ++i)
            extent.add(toLocal.Transform(colliderPoint(collider, scale, i)));
        break;
    }
}

void copyPoints(const ct::Vector<Math::Vec2>& points, ct::Vector<Math::Vec2>& scratch)
{
    scratch.clear();
    for (const Math::Vec2& p : points)
        scratch.push_back(p);
}

void storePoints(Collider2D& collider, const ct::Vector<Math::Vec2>& points)
{
    if (collider.componentClass() == ComponentClass::PolygonCollider2D)
        static_cast<PolygonCollider2D&>(collider).setPoints(points.data(), static_cast<int>(points.size()));
    else if (collider.componentClass() == ComponentClass::ChainCollider2D)
        static_cast<ChainCollider2D&>(collider).setPoints(points.data(), static_cast<int>(points.size()));
}

const ct::Vector<Math::Vec2>* pointList(const Collider2D& collider)
{
    if (collider.componentClass() == ComponentClass::PolygonCollider2D)
        return &static_cast<const PolygonCollider2D&>(collider).points();
    if (collider.componentClass() == ComponentClass::ChainCollider2D)
        return &static_cast<const ChainCollider2D&>(collider).points();
    return nullptr;
}

Math::Vec2 toCollider(const Collider2D& collider, const Math::Vec2& scale, const Math::Vec2& point)
{
    const Math::Vec2 o = collider.offset();
    return Math::Vec2((point.x - o.x) / scale.x, (point.y - o.y) / scale.y);
}

} // namespace

bool objectBounds(const GameObject& object, Math::Vec4& rect)
{
    Extent extent;
    if (const SpriteComponent* sprite = activeComponent<SpriteComponent>(object))
    {
        const Math::Vec2 size = sprite->size();
        if (size.x > 0.0f && size.y > 0.0f)
            extent.addRect(sprite->renderOffset().x - sprite->pivot().x * size.x,
                           sprite->renderOffset().y - sprite->pivot().y * size.y, size.x, size.y);
    }
    if (const NinePatchComponent* patch = activeComponent<NinePatchComponent>(object))
        extent.addRect(-patch->pivot().x * patch->size().x, -patch->pivot().y * patch->size().y, patch->size().x,
                       patch->size().y);
    if (const RectShape* shape = activeComponent<RectShape>(object))
        extent.addRect(-shape->size().x * 0.5f, -shape->size().y * 0.5f, shape->size().x, shape->size().y);
    if (const CapsuleShape* shape = activeComponent<CapsuleShape>(object))
        extent.addRect(-shape->size().x * 0.5f, -shape->size().y * 0.5f, shape->size().x, shape->size().y);
    if (const CircleShape* shape = activeComponent<CircleShape>(object))
        extent.addRect(-shape->radius(), -shape->radius(), shape->radius() * 2.0f, shape->radius() * 2.0f);
    if (const Polygon2D* polygon = activeComponent<Polygon2D>(object))
        extent.addPoints(polygon->polygon(), 0.0f);
    if (const Line2D* line = activeComponent<Line2D>(object))
        extent.addPoints(line->points(), line->width() * 0.5f);
    if (const TileMapComponent* tiles = activeComponent<TileMapComponent>(object))
        extent.addRect(0.0f, 0.0f, tiles->columns() * tiles->cellWidth(), tiles->rows() * tiles->cellHeight());

    const std::size_t colliders = object.componentCount<Collider2D>();
    if (colliders > 0)
    {
        const Matrix2D toLocal = object.globalTransform().AffineInverse() * bodyFrame(object);
        const Math::Vec2 scale = bodyScale(object);
        for (std::size_t i = 0; i < colliders; ++i)
            if (const Collider2D* collider = object.getComponentAt<Collider2D>(i))
                if (collider->active())
                    addCollider(extent, *collider, scale, toLocal);
    }

    if (!extent.any)
        return false;
    rect = Math::Vec4(extent.minX, extent.minY, extent.maxX - extent.minX, extent.maxY - extent.minY);
    return true;
}

Matrix2D bodyFrame(const GameObject& object)
{
    return Matrix2D::TRS(object.globalPosition(), object.globalRotationDegrees(), Math::Vec2(1.0f, 1.0f));
}

Math::Vec2 bodyScale(const GameObject& object)
{
    const Math::Vec2 scale = object.scale();
    return Math::Vec2(std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f,
                      std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f);
}

int colliderPointCount(const Collider2D& collider)
{
    if (const ct::Vector<Math::Vec2>* points = pointList(collider))
        return static_cast<int>(points->size());
    switch (collider.componentClass())
    {
    case ComponentClass::EdgeCollider2D:
        return 2;
    case ComponentClass::BoxCollider2D:
    case ComponentClass::CircleCollider2D:
        return 1;
    default:
        return 0;
    }
}

Math::Vec2 colliderPoint(const Collider2D& collider, const Math::Vec2& scale, int index)
{
    const Math::Vec2 o = collider.offset();
    Math::Vec2 p(0.0f, 0.0f);
    if (const ct::Vector<Math::Vec2>* points = pointList(collider))
        p = (*points)[static_cast<std::size_t>(index)];
    else if (collider.componentClass() == ComponentClass::EdgeCollider2D)
    {
        const EdgeCollider2D& edge = static_cast<const EdgeCollider2D&>(collider);
        p = index == 0 ? edge.start() : edge.end();
    }
    else if (collider.componentClass() == ComponentClass::BoxCollider2D)
        p = static_cast<const BoxCollider2D&>(collider).size() * 0.5f;
    else if (collider.componentClass() == ComponentClass::CircleCollider2D)
    {
        const float r = static_cast<const CircleCollider2D&>(collider).radius() * (scale.x > scale.y ? scale.x : scale.y);
        return Math::Vec2(o.x + r, o.y);
    }
    return Math::Vec2(p.x * scale.x + o.x, p.y * scale.y + o.y);
}

void setColliderPoint(Collider2D& collider, const Math::Vec2& scale, int index, const Math::Vec2& point,
                      ct::Vector<Math::Vec2>& scratch)
{
    const Math::Vec2 p = toCollider(collider, scale, point);
    if (const ct::Vector<Math::Vec2>* points = pointList(collider))
    {
        copyPoints(*points, scratch);
        scratch[static_cast<std::size_t>(index)] = p;
        storePoints(collider, scratch);
    }
    else if (collider.componentClass() == ComponentClass::EdgeCollider2D)
    {
        EdgeCollider2D& edge = static_cast<EdgeCollider2D&>(collider);
        edge.setPoints(index == 0 ? p : edge.start(), index == 0 ? edge.end() : p);
    }
    else if (collider.componentClass() == ComponentClass::BoxCollider2D)
    {
        const float w = std::fabs(p.x) * 2.0f, h = std::fabs(p.y) * 2.0f;
        static_cast<BoxCollider2D&>(collider).setSize(
            Math::Vec2(w > kMinimumExtent ? w : kMinimumExtent, h > kMinimumExtent ? h : kMinimumExtent));
    }
    else if (collider.componentClass() == ComponentClass::CircleCollider2D)
    {
        const Math::Vec2 o = collider.offset();
        const float r = std::sqrt((point.x - o.x) * (point.x - o.x) + (point.y - o.y) * (point.y - o.y)) /
                        (scale.x > scale.y ? scale.x : scale.y);
        static_cast<CircleCollider2D&>(collider).setRadius(r > kMinimumExtent ? r : kMinimumExtent);
    }
}

bool colliderClosed(const Collider2D& collider)
{
    if (collider.componentClass() == ComponentClass::ChainCollider2D)
        return static_cast<const ChainCollider2D&>(collider).loop();
    return collider.componentClass() == ComponentClass::PolygonCollider2D;
}

bool colliderEditable(const Collider2D& collider)
{
    return pointList(collider) != nullptr;
}

bool insertColliderPoint(Collider2D& collider, const Math::Vec2& scale, int after, const Math::Vec2& point,
                         ct::Vector<Math::Vec2>& scratch)
{
    const ct::Vector<Math::Vec2>* points = pointList(collider);
    if (!points || after < 0 || after >= static_cast<int>(points->size()))
        return false;
    scratch.clear();
    for (int i = 0; i < static_cast<int>(points->size()); ++i)
    {
        scratch.push_back((*points)[static_cast<std::size_t>(i)]);
        if (i == after)
            scratch.push_back(toCollider(collider, scale, point));
    }
    storePoints(collider, scratch);
    return true;
}

bool removeColliderPoint(Collider2D& collider, int index, ct::Vector<Math::Vec2>& scratch)
{
    const ct::Vector<Math::Vec2>* points = pointList(collider);
    const int minimum = collider.componentClass() == ComponentClass::PolygonCollider2D ? 3 : 2;
    if (!points || index < 0 || index >= static_cast<int>(points->size()) ||
        static_cast<int>(points->size()) <= minimum)
        return false;
    scratch.clear();
    for (int i = 0; i < static_cast<int>(points->size()); ++i)
        if (i != index)
            scratch.push_back((*points)[static_cast<std::size_t>(i)]);
    storePoints(collider, scratch);
    return true;
}

} // namespace k2d::editor
