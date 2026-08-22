#include "kx/world.h"

namespace kx
{

    namespace
    {
        // Sutherland-Hodgman: recorta um poligono convexo (CCW) contra o semiplano
        // Dot(v-point,normal) >= 0, mantendo esse lado. maxOut deve ser pelo menos
        // inCount+2 (o pior caso: a reta entra e sai do poligono, acrescentando 2
        // vertices novos a lista de vertices originais que sobrevivem).
        int ClipPolygonToHalfPlane(const glm::vec2 *in, int inCount,
                                   const glm::vec2 &point, const glm::vec2 &normal,
                                   glm::vec2 *out, int maxOut)
        {
            int outCount = 0;
            for (int i = 0; i < inCount; ++i)
            {
                glm::vec2 a = in[i];
                glm::vec2 b = in[(i + 1) % inCount];
                float da = Dot(a - point, normal);
                float db = Dot(b - point, normal);

                if (da >= 0.0f)
                {
                    if (outCount < maxOut)
                        out[outCount++] = a;
                    if (db < 0.0f && outCount < maxOut)
                    {
                        float t = da / (da - db);
                        out[outCount++] = a + t * (b - a);
                    }
                }
                else if (db >= 0.0f)
                {
                    if (outCount < maxOut)
                    {
                        float t = da / (da - db);
                        out[outCount++] = a + t * (b - a);
                    }
                }
            }
            return outCount;
        }

        struct SlicePiece
        {
            ShapeType type;
            Circle circle;
            Edge edge;
            Polygon polygon;
            float density;
            Filter filter;
            bool isSensor;
            void *userData;
        };

        Body *BuildSlicedBody(World &world, const Body &source, ct::Vector<SlicePiece> &pieces)
        {
            Body *body = world.CreateBody(source.Type(), source.Position(), source.Angle());
            body->SetFriction(source.Friction());
            body->SetRestitution(source.Restitution());
            body->SetLinearDamping(source.LinearDamping());
            body->SetAngularDamping(source.AngularDamping());
            body->SetGravityScale(source.GravityScale());
            body->SetFixedRotation(source.FixedRotation());
            body->SetVelocity(source.Velocity());
            body->SetAngularVelocity(source.AngularVelocity());
            body->SetUserData(source.UserData());

            for (size_t i = 0; i < pieces.size(); ++i)
            {
                SlicePiece &p = pieces[i];
                int shapeIndex = body->ShapeCount();
                switch (p.type)
                {
                case ShapeType::Circle:
                    body->AddCircle(p.circle.center, p.circle.radius, p.density);
                    break;
                case ShapeType::Polygon:
                    body->AddPolygon(p.polygon.vertices, p.polygon.count, p.density);
                    break;
                case ShapeType::Edge:
                default:
                    body->AddEdge(p.edge.vertex1, p.edge.vertex2);
                    break;
                }
                // Filtro so existe ao nivel do corpo (ver nota em Slice); aplica-se o da
                // ultima shape processada. isSensor/userData sao mesmo por shape.
                body->SetFilter(p.filter.category, p.filter.mask, p.filter.group);
                body->SetSensor(shapeIndex, p.isSensor);
                body->SetShapeUserData(shapeIndex, p.userData);
            }

            return body;
        }
    } // namespace

    bool Slice(World &world, Body *body, const glm::vec2 &point, const glm::vec2 &normal,
              Body **outPositive, Body **outNegative, float separationSpeed)
    {
        if (outPositive)
            *outPositive = nullptr;
        if (outNegative)
            *outNegative = nullptr;

        glm::vec2 n = Normalize(normal);
        if (Dot(n, n) < 0.5f) // normal degenerada (quase zero)
            return false;

        Transform xf = body->GetTransform();
        glm::vec2 localPoint = InvTransformPoint(xf, point);
        // Transform deste motor e so rotacao+translacao (sem escala), por isso rodar um
        // vetor unitario com InvRotate mantem-no unitario.
        glm::vec2 localNormal = InvRotate(xf, n);

        ct::Vector<SlicePiece> positive;
        ct::Vector<SlicePiece> negative;

        const int kClipMax = kMaxPolygonVertices + 2;
        glm::vec2 posBuf[kClipMax];
        glm::vec2 negBuf[kClipMax];

        for (int s = 0; s < body->ShapeCount(); ++s)
        {
            const Shape &shape = body->Shapes()[s];

            if (shape.type == ShapeType::Polygon)
            {
                const Polygon &poly = shape.polygon;
                int posCount = ClipPolygonToHalfPlane(poly.vertices, poly.count, localPoint, localNormal, posBuf, kClipMax);
                int negCount = ClipPolygonToHalfPlane(poly.vertices, poly.count, localPoint, -localNormal, negBuf, kClipMax);

                if (posCount >= 3)
                {
                    SlicePiece piece;
                    piece.type = ShapeType::Polygon;
                    piece.polygon.Set(posBuf, posCount < kMaxPolygonVertices ? posCount : kMaxPolygonVertices);
                    piece.polygon.radius = poly.radius;
                    piece.density = shape.density;
                    piece.filter = shape.filter;
                    piece.isSensor = shape.isSensor;
                    piece.userData = shape.userData;
                    positive.push_back(piece);
                }
                if (negCount >= 3)
                {
                    SlicePiece piece;
                    piece.type = ShapeType::Polygon;
                    piece.polygon.Set(negBuf, negCount < kMaxPolygonVertices ? negCount : kMaxPolygonVertices);
                    piece.polygon.radius = poly.radius;
                    piece.density = shape.density;
                    piece.filter = shape.filter;
                    piece.isSensor = shape.isSensor;
                    piece.userData = shape.userData;
                    negative.push_back(piece);
                }
                continue;
            }

            // Circle/Edge nao sao cortadas — ficam inteiras do lado do seu centro (ou
            // ponto medio, no caso da edge).
            glm::vec2 mid = shape.type == ShapeType::Circle
                                ? shape.circle.center
                                : 0.5f * (shape.edge.vertex1 + shape.edge.vertex2);
            float d = Dot(mid - localPoint, localNormal);

            SlicePiece piece;
            piece.type = shape.type;
            piece.circle = shape.circle;
            piece.edge = shape.edge;
            piece.density = shape.density;
            piece.filter = shape.filter;
            piece.isSensor = shape.isSensor;
            piece.userData = shape.userData;
            (d >= 0.0f ? positive : negative).push_back(piece);
        }

        if (positive.empty() || negative.empty())
            return false; // a reta nao atravessa o corpo: nada para cortar

        Body *posBody = BuildSlicedBody(world, *body, positive);
        Body *negBody = BuildSlicedBody(world, *body, negative);

        if (separationSpeed > 0.0f)
        {
            posBody->SetVelocity(posBody->Velocity() + n * separationSpeed);
            negBody->SetVelocity(negBody->Velocity() - n * separationSpeed);
        }

        world.Destroy(body);

        if (outPositive)
            *outPositive = posBody;
        if (outNegative)
            *outNegative = negBody;
        return true;
    }

} // namespace kx
