#pragma once

#include <k2d/Matrix2D.h>

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
class Collider2D;
class GameObject;
}

namespace k2d::editor
{

// The rectangle (x, y, w, h) the object's visible parts and colliders cover, in
// the frame of its global transform; false when it has none.
bool objectBounds(const GameObject& object, Math::Vec4& rect);

// Where the physics places the object's colliders: its global position and
// rotation, without the scale, which the shapes take into their points.
Matrix2D bodyFrame(const GameObject& object);
Math::Vec2 bodyScale(const GameObject& object);

// The points of a collider an editor drags, in the body frame: the vertices of
// polygons, chains and edges, the corner of a box, a point on a circle.
int colliderPointCount(const Collider2D& collider);
Math::Vec2 colliderPoint(const Collider2D& collider, const Math::Vec2& scale, int index);
void setColliderPoint(Collider2D& collider, const Math::Vec2& scale, int index, const Math::Vec2& point,
                      ct::Vector<Math::Vec2>& scratch);

// Polygons close on their first point, and so do chains that loop.
bool colliderClosed(const Collider2D& collider);
// Polygons and chains take points and give them up, down to what keeps them a
// shape: three for a polygon, two for a chain.
bool colliderEditable(const Collider2D& collider);
bool insertColliderPoint(Collider2D& collider, const Math::Vec2& scale, int after, const Math::Vec2& point,
                         ct::Vector<Math::Vec2>& scratch);
bool removeColliderPoint(Collider2D& collider, int index, ct::Vector<Math::Vec2>& scratch);

} // namespace k2d::editor
