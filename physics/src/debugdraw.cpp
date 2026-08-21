#include "kx/debugdraw.h"
#include "kx/world.h"

namespace kx
{

    namespace
    {

        Color ShapeColorForBody(const Body &body)
        {
            if (body.Type() == BodyType::Dynamic)
                return Color{80, 220, 120, 255};
            if (body.Type() == BodyType::Kinematic)
                return Color{80, 160, 255, 255};
            return Color{140, 140, 140, 255};
        }

        void DrawShape(const Shape &shape, const Transform &xf, Color color, DebugDraw &draw)
        {
            switch (shape.type)
            {
            case ShapeType::Circle:
                draw.DrawCircleShape(xf, shape.circle.radius, color);
                break;
            case ShapeType::Polygon:
                draw.DrawPolygonShape(xf, shape.polygon.vertices, shape.polygon.count, color);
                break;
            case ShapeType::Edge:
                draw.DrawSegment(xf.Transform(shape.edge.vertex1), xf.Transform(shape.edge.vertex2), color);
                break;
            }
        }

        void DrawBodyShapes(Body &body, DebugDraw &draw)
        {
            Color color = ShapeColorForBody(body);
            Transform xf = body.GetTransform();

            for (int i = 0; i < body.ShapeCount(); ++i)
                DrawShape(body.Shapes()[i], xf, color, draw);

            glm::vec2 origin = xf.Transform(glm::vec2(0.0f, 0.0f));
            glm::vec2 tip = xf.Transform(glm::vec2(20.0f, 0.0f));
            draw.DrawSegment(origin, tip, Color{255, 255, 255, 255});
        }

        AABB ComputeShapeAABB(const Shape &shape, const Transform &xf)
        {
            switch (shape.type)
            {
            case ShapeType::Circle:
                return shape.circle.ComputeAABB(xf);
            case ShapeType::Polygon:
                return shape.polygon.ComputeAABB(xf);
            case ShapeType::Edge:
            default:
                return shape.edge.ComputeAABB(xf);
            }
        }

        void DrawBodyAABB(Body &body, DebugDraw &draw)
        {
            if (body.ShapeCount() == 0)
                return;

            Transform xf = body.GetTransform();
            AABB aabb = ComputeShapeAABB(body.Shapes()[0], xf);

            for (int i = 1; i < body.ShapeCount(); ++i)
            {
                AABB shapeAabb = ComputeShapeAABB(body.Shapes()[i], xf);
                aabb.lowerBound = glm::min(aabb.lowerBound, shapeAabb.lowerBound);
                aabb.upperBound = glm::max(aabb.upperBound, shapeAabb.upperBound);
            }

            draw.DrawAABB(aabb.lowerBound, aabb.upperBound, Color{200, 200, 60, 160});
        }


        void DrawContact(const ContactInfo &c, DebugDraw &draw)
        {
            float radiusA = ShapeRadius(c.a->Shapes()[c.shapeIndexA]);
            float radiusB = ShapeRadius(c.b->Shapes()[c.shapeIndexB]);

            WorldManifold worldManifold;
            worldManifold.Initialize(&c.manifold, c.a->GetTransform(), radiusA, c.b->GetTransform(), radiusB);

            const Color pointColor{255, 40, 40, 255};
            const Color normalColor{255, 140, 40, 255};
            const float arrowLength = 15.0f;

            for (int32_t p = 0; p < c.manifold.pointCount; ++p)
            {
                draw.DrawPoint(worldManifold.points[p], 4.0f, pointColor);
                glm::vec2 tip = worldManifold.points[p] + arrowLength * worldManifold.normal;
                draw.DrawSegment(worldManifold.points[p], tip, normalColor);
            }
        }

    } // namespace

    void Draw(World &world, DebugDraw &draw, unsigned flags)
    {
        if (flags & DebugDrawJoints)
        {
            const ct::Vector<Joint *> &joints = world.Joints();
            for (size_t i = 0; i < joints.size(); ++i)
            {
                glm::vec2 a = joints[i]->AnchorA();
                glm::vec2 b = joints[i]->AnchorB();
                Color jointColor{120, 200, 255, 255};
                draw.DrawSegment(a, b, jointColor);
                draw.DrawPoint(a, 3.0f, jointColor);
                draw.DrawPoint(b, 3.0f, jointColor);
            }
        }

        if (flags == 0)
            return;

        const ct::Vector<Body *> &bodies = world.Bodies();

        if (flags & DebugDrawShapes)
            for (size_t i = 0; i < bodies.size(); ++i)
                DrawBodyShapes(*bodies[i], draw);

        if (flags & DebugDrawAABBs)
            for (size_t i = 0; i < bodies.size(); ++i)
                DrawBodyAABB(*bodies[i], draw);

        if (flags & DebugDrawContacts)
        {
            const ct::Vector<ContactInfo> &contacts = world.Contacts();
            for (size_t i = 0; i < contacts.size(); ++i)
                DrawContact(contacts[i], draw);
        }
    }

} // namespace kx
