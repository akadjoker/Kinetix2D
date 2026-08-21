#include "kx/internal/collide.h"

namespace kx
{

    void CollideCircles(Manifold *manifold,
                        const Circle &circleA, const Transform &xfA,
                        const Circle &circleB, const Transform &xfB)
    {
        manifold->pointCount = 0;

        glm::vec2 pA = xfA.Transform(circleA.center);
        glm::vec2 pB = xfB.Transform(circleB.center);

        glm::vec2 d = pB - pA;
        float distSqr = Dot(d, d);
        float radius = circleA.radius + circleB.radius;
        if (distSqr > radius * radius)
            return;

        manifold->type = Manifold::kCircles;
        manifold->localPoint = circleA.center;
        manifold->localNormal = glm::vec2(0.0f, 0.0f);
        manifold->pointCount = 1;
        manifold->points[0].localPoint = circleB.center;
        manifold->points[0].id.key = 0;
    }

    void CollidePolygonAndCircle(Manifold *manifold,
                                 const Polygon &polygonA, const Transform &xfA,
                                 const Circle &circleB, const Transform &xfB)
    {
        manifold->pointCount = 0;

        glm::vec2 c = xfB.Transform(circleB.center);
        glm::vec2 cLocal = InvTransformPoint(xfA, c);

        int32_t normalIndex = 0;
        float separation = -3.402823466e+38F;
        float radius = polygonA.radius + circleB.radius;
        int32_t vertexCount = polygonA.count;
        const glm::vec2 *vertices = polygonA.vertices;
        const glm::vec2 *normals = polygonA.normals;

        for (int32_t i = 0; i < vertexCount; ++i)
        {
            float s = Dot(normals[i], cLocal - vertices[i]);
            if (s > radius)
                return;
            if (s > separation)
            {
                separation = s;
                normalIndex = i;
            }
        }

        int32_t vertIndex1 = normalIndex;
        int32_t vertIndex2 = vertIndex1 + 1 < vertexCount ? vertIndex1 + 1 : 0;
        glm::vec2 v1 = vertices[vertIndex1];
        glm::vec2 v2 = vertices[vertIndex2];

        if (separation < kEpsilon)
        {
            manifold->pointCount = 1;
            manifold->type = Manifold::kFaceA;
            manifold->localNormal = normals[normalIndex];
            manifold->localPoint = 0.5f * (v1 + v2);
            manifold->points[0].localPoint = circleB.center;
            manifold->points[0].id.key = 0;
            return;
        }

        float u1 = Dot(cLocal - v1, v2 - v1);
        float u2 = Dot(cLocal - v2, v1 - v2);

        if (u1 <= 0.0f)
        {
            if (DistanceSquared(cLocal, v1) > radius * radius)
                return;
            manifold->pointCount = 1;
            manifold->type = Manifold::kFaceA;
            manifold->localNormal = Normalize(cLocal - v1);
            manifold->localPoint = v1;
        }
        else if (u2 <= 0.0f)
        {
            if (DistanceSquared(cLocal, v2) > radius * radius)
                return;
            manifold->pointCount = 1;
            manifold->type = Manifold::kFaceA;
            manifold->localNormal = Normalize(cLocal - v2);
            manifold->localPoint = v2;
        }
        else
        {
            glm::vec2 faceCenter = 0.5f * (v1 + v2);
            float s = Dot(cLocal - faceCenter, normals[vertIndex1]);
            if (s > radius)
                return;
            manifold->pointCount = 1;
            manifold->type = Manifold::kFaceA;
            manifold->localNormal = normals[vertIndex1];
            manifold->localPoint = faceCenter;
        }

        manifold->points[0].localPoint = circleB.center;
        manifold->points[0].id.key = 0;
    }

    namespace
    {

        struct ClipVertex
        {
            glm::vec2 v;
            ContactID id;
        };

        Transform InvMulTransform(const Transform &A, const Transform &B)
        {
            float newA = A.a * B.a + A.b * B.b;
            float newB = A.c * B.a + A.d * B.b;
            glm::vec2 p = InvTransformPoint(A, glm::vec2(B.tx, B.ty));
            return Transform(newA, newB, -newB, newA, p.x, p.y);
        }

        int32_t ClipSegmentToLine(ClipVertex vOut[2], const ClipVertex vIn[2],
                                  const glm::vec2 &normal, float offset, int32_t vertexIndexA)
        {
            int32_t count = 0;

            float distance0 = Dot(normal, vIn[0].v) - offset;
            float distance1 = Dot(normal, vIn[1].v) - offset;

            if (distance0 <= 0.0f)
                vOut[count++] = vIn[0];
            if (distance1 <= 0.0f)
                vOut[count++] = vIn[1];

            if (distance0 * distance1 < 0.0f)
            {
                float interp = distance0 / (distance0 - distance1);
                vOut[count].v = vIn[0].v + interp * (vIn[1].v - vIn[0].v);

                vOut[count].id.cf.indexA = static_cast<uint8_t>(vertexIndexA);
                vOut[count].id.cf.indexB = vIn[0].id.cf.indexB;
                vOut[count].id.cf.typeA = ContactFeature::kVertex;
                vOut[count].id.cf.typeB = ContactFeature::kFace;
                ++count;
            }

            return count;
        }

        float FindMaxSeparation(int32_t *edgeIndex,
                                const Polygon &poly1, const Transform &xf1,
                                const Polygon &poly2, const Transform &xf2)
        {
            int32_t count1 = poly1.count;
            int32_t count2 = poly2.count;
            const glm::vec2 *n1s = poly1.normals;
            const glm::vec2 *v1s = poly1.vertices;
            const glm::vec2 *v2s = poly2.vertices;
            Transform xf = InvMulTransform(xf2, xf1);

            int32_t bestIndex = 0;
            float maxSeparation = -3.402823466e+38F;
            for (int32_t i = 0; i < count1; ++i)
            {
                glm::vec2 n = Rotate(xf, n1s[i]);
                glm::vec2 v1 = xf.Transform(v1s[i]);

                float si = 3.402823466e+38F;
                for (int32_t j = 0; j < count2; ++j)
                {
                    float sij = Dot(n, v2s[j] - v1);
                    if (sij < si)
                        si = sij;
                }

                if (si > maxSeparation)
                {
                    maxSeparation = si;
                    bestIndex = i;
                }
            }

            *edgeIndex = bestIndex;
            return maxSeparation;
        }

        void FindIncidentEdge(ClipVertex c[2],
                              const Polygon &poly1, const Transform &xf1, int32_t edge1,
                              const Polygon &poly2, const Transform &xf2)
        {
            const glm::vec2 *normals1 = poly1.normals;
            int32_t count2 = poly2.count;
            const glm::vec2 *vertices2 = poly2.vertices;
            const glm::vec2 *normals2 = poly2.normals;

            glm::vec2 normal1 = InvRotate(xf2, Rotate(xf1, normals1[edge1]));

            int32_t index = 0;
            float minDot = 3.402823466e+38F;
            for (int32_t i = 0; i < count2; ++i)
            {
                float dot = Dot(normal1, normals2[i]);
                if (dot < minDot)
                {
                    minDot = dot;
                    index = i;
                }
            }

            int32_t i1 = index;
            int32_t i2 = i1 + 1 < count2 ? i1 + 1 : 0;

            c[0].v = xf2.Transform(vertices2[i1]);
            c[0].id.cf.indexA = static_cast<uint8_t>(edge1);
            c[0].id.cf.indexB = static_cast<uint8_t>(i1);
            c[0].id.cf.typeA = ContactFeature::kFace;
            c[0].id.cf.typeB = ContactFeature::kVertex;

            c[1].v = xf2.Transform(vertices2[i2]);
            c[1].id.cf.indexA = static_cast<uint8_t>(edge1);
            c[1].id.cf.indexB = static_cast<uint8_t>(i2);
            c[1].id.cf.typeA = ContactFeature::kFace;
            c[1].id.cf.typeB = ContactFeature::kVertex;
        }

    } // namespace

    void CollidePolygons(Manifold *manifold,
                         const Polygon &polyA, const Transform &xfA,
                         const Polygon &polyB, const Transform &xfB)
    {
        manifold->pointCount = 0;
        float totalRadius = polyA.radius + polyB.radius;

        int32_t edgeA = 0;
        float separationA = FindMaxSeparation(&edgeA, polyA, xfA, polyB, xfB);
        if (separationA > totalRadius)
            return;

        int32_t edgeB = 0;
        float separationB = FindMaxSeparation(&edgeB, polyB, xfB, polyA, xfA);
        if (separationB > totalRadius)
            return;

        const Polygon *poly1;
        const Polygon *poly2;
        Transform xf1, xf2;
        int32_t edge1;
        uint8_t flip;
        const float kTol = 0.1f * kLinearSlop;

        if (separationB > separationA + kTol)
        {
            poly1 = &polyB;
            poly2 = &polyA;
            xf1 = xfB;
            xf2 = xfA;
            edge1 = edgeB;
            manifold->type = Manifold::kFaceB;
            flip = 1;
        }
        else
        {
            poly1 = &polyA;
            poly2 = &polyB;
            xf1 = xfA;
            xf2 = xfB;
            edge1 = edgeA;
            manifold->type = Manifold::kFaceA;
            flip = 0;
        }

        ClipVertex incidentEdge[2];
        FindIncidentEdge(incidentEdge, *poly1, xf1, edge1, *poly2, xf2);

        int32_t count1 = poly1->count;
        const glm::vec2 *vertices1 = poly1->vertices;

        int32_t iv1 = edge1;
        int32_t iv2 = edge1 + 1 < count1 ? edge1 + 1 : 0;

        glm::vec2 v11 = vertices1[iv1];
        glm::vec2 v12 = vertices1[iv2];

        glm::vec2 localTangent = Normalize(v12 - v11);
        glm::vec2 localNormal = Cross(localTangent, 1.0f);
        glm::vec2 planePoint = 0.5f * (v11 + v12);

        glm::vec2 tangent = Rotate(xf1, localTangent);
        glm::vec2 normal = Cross(tangent, 1.0f);

        v11 = xf1.Transform(v11);
        v12 = xf1.Transform(v12);

        float frontOffset = Dot(normal, v11);
        float sideOffset1 = -Dot(tangent, v11) + totalRadius;
        float sideOffset2 = Dot(tangent, v12) + totalRadius;

        ClipVertex clipPoints1[2];
        ClipVertex clipPoints2[2];
        int32_t np;

        np = ClipSegmentToLine(clipPoints1, incidentEdge, -tangent, sideOffset1, iv1);
        if (np < 2)
            return;

        np = ClipSegmentToLine(clipPoints2, clipPoints1, tangent, sideOffset2, iv2);
        if (np < 2)
            return;

        manifold->localNormal = localNormal;
        manifold->localPoint = planePoint;

        int32_t pointCount = 0;
        for (int32_t i = 0; i < kMaxManifoldPoints; ++i)
        {
            float separation = Dot(normal, clipPoints2[i].v) - frontOffset;

            if (separation <= totalRadius)
            {
                ManifoldPoint *cp = manifold->points + pointCount;
                cp->localPoint = InvTransformPoint(xf2, clipPoints2[i].v);
                cp->id = clipPoints2[i].id;
                if (flip)
                {
                    ContactFeature cf = cp->id.cf;
                    cp->id.cf.indexA = cf.indexB;
                    cp->id.cf.indexB = cf.indexA;
                    cp->id.cf.typeA = cf.typeB;
                    cp->id.cf.typeB = cf.typeA;
                }
                ++pointCount;
            }
        }

        manifold->pointCount = pointCount;
    }

    void CollideEdgeAndCircle(Manifold *manifold,
                              const Edge &edgeA, const Transform &xfA,
                              const Circle &circleB, const Transform &xfB)
    {
        manifold->pointCount = 0;

        glm::vec2 Q = InvTransformPoint(xfA, xfB.Transform(circleB.center));

        glm::vec2 A = edgeA.vertex1, B = edgeA.vertex2;
        glm::vec2 e = B - A;

        glm::vec2 n(e.y, -e.x);
        float offset = Dot(n, Q - A);

        if (edgeA.oneSided && offset < 0.0f)
            return;

        float u = Dot(e, B - Q);
        float v = Dot(e, Q - A);

        float radius = edgeA.radius + circleB.radius;

        ContactFeature cf;
        cf.indexB = 0;
        cf.typeB = ContactFeature::kVertex;

        if (v <= 0.0f)
        {
            glm::vec2 P = A;
            glm::vec2 d = Q - P;
            if (Dot(d, d) > radius * radius)
                return;

            if (edgeA.oneSided)
            {
                glm::vec2 A1 = edgeA.vertex0;
                glm::vec2 B1 = A;
                glm::vec2 e1 = B1 - A1;
                float u1 = Dot(e1, B1 - Q);
                if (u1 > 0.0f)
                    return;
            }

            cf.indexA = 0;
            cf.typeA = ContactFeature::kVertex;
            manifold->pointCount = 1;
            manifold->type = Manifold::kCircles;
            manifold->localNormal = glm::vec2(0.0f, 0.0f);
            manifold->localPoint = P;
            manifold->points[0].id.key = 0;
            manifold->points[0].id.cf = cf;
            manifold->points[0].localPoint = circleB.center;
            return;
        }

        if (u <= 0.0f)
        {
            glm::vec2 P = B;
            glm::vec2 d = Q - P;
            if (Dot(d, d) > radius * radius)
                return;

            if (edgeA.oneSided)
            {
                glm::vec2 B2 = edgeA.vertex3;
                glm::vec2 A2 = B;
                glm::vec2 e2 = B2 - A2;
                float v2 = Dot(e2, Q - A2);
                if (v2 > 0.0f)
                    return;
            }

            cf.indexA = 1;
            cf.typeA = ContactFeature::kVertex;
            manifold->pointCount = 1;
            manifold->type = Manifold::kCircles;
            manifold->localNormal = glm::vec2(0.0f, 0.0f);
            manifold->localPoint = P;
            manifold->points[0].id.key = 0;
            manifold->points[0].id.cf = cf;
            manifold->points[0].localPoint = circleB.center;
            return;
        }

        float den = Dot(e, e);
        glm::vec2 P = (1.0f / den) * (u * A + v * B);
        glm::vec2 d = Q - P;
        if (Dot(d, d) > radius * radius)
            return;

        if (offset < 0.0f)
            n = glm::vec2(-n.x, -n.y);
        n = Normalize(n);

        cf.indexA = 0;
        cf.typeA = ContactFeature::kFace;
        manifold->pointCount = 1;
        manifold->type = Manifold::kFaceA;
        manifold->localNormal = n;
        manifold->localPoint = A;
        manifold->points[0].id.key = 0;
        manifold->points[0].id.cf = cf;
        manifold->points[0].localPoint = circleB.center;
    }

    namespace
    {

        struct EPAxis
        {
            enum Type
            {
                kUnknown,
                kEdgeA,
                kEdgeB
            };

            glm::vec2 normal;
            Type type;
            int32_t index;
            float separation;
        };

        struct TempPolygon
        {
            glm::vec2 vertices[kMaxPolygonVertices];
            glm::vec2 normals[kMaxPolygonVertices];
            int32_t count;
        };

        struct ReferenceFace
        {
            int32_t i1, i2;
            glm::vec2 v1, v2;
            glm::vec2 normal;

            glm::vec2 sideNormal1;
            float sideOffset1;

            glm::vec2 sideNormal2;
            float sideOffset2;
        };

        EPAxis ComputeEdgeSeparation(const TempPolygon &polygonB, const glm::vec2 &v1, const glm::vec2 &normal1)
        {
            EPAxis axis;
            axis.type = EPAxis::kEdgeA;
            axis.index = -1;
            axis.separation = -3.402823466e+38F;
            axis.normal = glm::vec2(0.0f, 0.0f);

            glm::vec2 axes[2] = {normal1, -normal1};

            for (int32_t j = 0; j < 2; ++j)
            {
                float sj = 3.402823466e+38F;
                for (int32_t i = 0; i < polygonB.count; ++i)
                {
                    float si = Dot(axes[j], polygonB.vertices[i] - v1);
                    if (si < sj)
                        sj = si;
                }

                if (sj > axis.separation)
                {
                    axis.index = j;
                    axis.separation = sj;
                    axis.normal = axes[j];
                }
            }

            return axis;
        }

        EPAxis ComputePolygonSeparation(const TempPolygon &polygonB, const glm::vec2 &v1, const glm::vec2 &v2)
        {
            EPAxis axis;
            axis.type = EPAxis::kUnknown;
            axis.index = -1;
            axis.separation = -3.402823466e+38F;
            axis.normal = glm::vec2(0.0f, 0.0f);

            for (int32_t i = 0; i < polygonB.count; ++i)
            {
                glm::vec2 n = -polygonB.normals[i];

                float s1 = Dot(n, polygonB.vertices[i] - v1);
                float s2 = Dot(n, polygonB.vertices[i] - v2);
                float s = s1 < s2 ? s1 : s2;

                if (s > axis.separation)
                {
                    axis.type = EPAxis::kEdgeB;
                    axis.index = i;
                    axis.separation = s;
                    axis.normal = n;
                }
            }

            return axis;
        }

    } // namespace

    void CollideEdgeAndPolygon(Manifold *manifold,
                               const Edge &edgeA, const Transform &xfA,
                               const Polygon &polygonB, const Transform &xfB)
    {
        manifold->pointCount = 0;

        Transform xf = InvMulTransform(xfA, xfB);

        glm::vec2 centroidB = xf.Transform(polygonB.centroid);

        glm::vec2 v1 = edgeA.vertex1;
        glm::vec2 v2 = edgeA.vertex2;

        glm::vec2 edge1 = Normalize(v2 - v1);

        glm::vec2 normal1(edge1.y, -edge1.x);
        float offset1 = Dot(normal1, centroidB - v1);

        bool oneSided = edgeA.oneSided;
        if (oneSided && offset1 < 0.0f)
            return;

        TempPolygon tempPolygonB;
        tempPolygonB.count = polygonB.count;
        for (int32_t i = 0; i < polygonB.count; ++i)
        {
            tempPolygonB.vertices[i] = xf.Transform(polygonB.vertices[i]);
            tempPolygonB.normals[i] = Rotate(xf, polygonB.normals[i]);
        }

        float radius = polygonB.radius + edgeA.radius;

        EPAxis edgeAxis = ComputeEdgeSeparation(tempPolygonB, v1, normal1);
        if (edgeAxis.separation > radius)
            return;

        EPAxis polygonAxis = ComputePolygonSeparation(tempPolygonB, v1, v2);
        if (polygonAxis.separation > radius)
            return;

        const float kRelativeTol = 0.98f;
        const float kAbsoluteTol = 0.001f;

        EPAxis primaryAxis = polygonAxis.separation - radius > kRelativeTol * (edgeAxis.separation - radius) + kAbsoluteTol
                                  ? polygonAxis
                                  : edgeAxis;

        if (oneSided)
        {
            glm::vec2 edge0 = Normalize(v1 - edgeA.vertex0);
            glm::vec2 normal0(edge0.y, -edge0.x);
            bool convex1 = Cross(edge0, edge1) >= 0.0f;

            glm::vec2 edge2 = Normalize(edgeA.vertex3 - v2);
            glm::vec2 normal2(edge2.y, -edge2.x);
            bool convex2 = Cross(edge1, edge2) >= 0.0f;

            const float sinTol = 0.1f;
            bool side1 = Dot(primaryAxis.normal, edge1) <= 0.0f;

            if (side1)
            {
                if (convex1)
                {
                    if (Cross(primaryAxis.normal, normal0) > sinTol)
                        return;
                }
                else
                {
                    primaryAxis = edgeAxis;
                }
            }
            else
            {
                if (convex2)
                {
                    if (Cross(normal2, primaryAxis.normal) > sinTol)
                        return;
                }
                else
                {
                    primaryAxis = edgeAxis;
                }
            }
        }

        ClipVertex clipPoints[2];
        ReferenceFace ref;
        if (primaryAxis.type == EPAxis::kEdgeA)
        {
            manifold->type = Manifold::kFaceA;

            int32_t bestIndex = 0;
            float bestValue = Dot(primaryAxis.normal, tempPolygonB.normals[0]);
            for (int32_t i = 1; i < tempPolygonB.count; ++i)
            {
                float value = Dot(primaryAxis.normal, tempPolygonB.normals[i]);
                if (value < bestValue)
                {
                    bestValue = value;
                    bestIndex = i;
                }
            }

            int32_t i1 = bestIndex;
            int32_t i2 = i1 + 1 < tempPolygonB.count ? i1 + 1 : 0;

            clipPoints[0].v = tempPolygonB.vertices[i1];
            clipPoints[0].id.cf.indexA = 0;
            clipPoints[0].id.cf.indexB = static_cast<uint8_t>(i1);
            clipPoints[0].id.cf.typeA = ContactFeature::kFace;
            clipPoints[0].id.cf.typeB = ContactFeature::kVertex;

            clipPoints[1].v = tempPolygonB.vertices[i2];
            clipPoints[1].id.cf.indexA = 0;
            clipPoints[1].id.cf.indexB = static_cast<uint8_t>(i2);
            clipPoints[1].id.cf.typeA = ContactFeature::kFace;
            clipPoints[1].id.cf.typeB = ContactFeature::kVertex;

            ref.i1 = 0;
            ref.i2 = 1;
            ref.v1 = v1;
            ref.v2 = v2;
            ref.normal = primaryAxis.normal;
            ref.sideNormal1 = -edge1;
            ref.sideNormal2 = edge1;
        }
        else
        {
            manifold->type = Manifold::kFaceB;

            clipPoints[0].v = v2;
            clipPoints[0].id.cf.indexA = 1;
            clipPoints[0].id.cf.indexB = static_cast<uint8_t>(primaryAxis.index);
            clipPoints[0].id.cf.typeA = ContactFeature::kVertex;
            clipPoints[0].id.cf.typeB = ContactFeature::kFace;

            clipPoints[1].v = v1;
            clipPoints[1].id.cf.indexA = 0;
            clipPoints[1].id.cf.indexB = static_cast<uint8_t>(primaryAxis.index);
            clipPoints[1].id.cf.typeA = ContactFeature::kVertex;
            clipPoints[1].id.cf.typeB = ContactFeature::kFace;

            ref.i1 = primaryAxis.index;
            ref.i2 = ref.i1 + 1 < tempPolygonB.count ? ref.i1 + 1 : 0;
            ref.v1 = tempPolygonB.vertices[ref.i1];
            ref.v2 = tempPolygonB.vertices[ref.i2];
            ref.normal = tempPolygonB.normals[ref.i1];

            ref.sideNormal1 = glm::vec2(ref.normal.y, -ref.normal.x);
            ref.sideNormal2 = -ref.sideNormal1;
        }

        ref.sideOffset1 = Dot(ref.sideNormal1, ref.v1);
        ref.sideOffset2 = Dot(ref.sideNormal2, ref.v2);

        ClipVertex clipPoints1[2];
        ClipVertex clipPoints2[2];
        int32_t np;

        np = ClipSegmentToLine(clipPoints1, clipPoints, ref.sideNormal1, ref.sideOffset1, ref.i1);
        if (np < kMaxManifoldPoints)
            return;

        np = ClipSegmentToLine(clipPoints2, clipPoints1, ref.sideNormal2, ref.sideOffset2, ref.i2);
        if (np < kMaxManifoldPoints)
            return;

        if (primaryAxis.type == EPAxis::kEdgeA)
        {
            manifold->localNormal = ref.normal;
            manifold->localPoint = ref.v1;
        }
        else
        {
            manifold->localNormal = polygonB.normals[ref.i1];
            manifold->localPoint = polygonB.vertices[ref.i1];
        }

        int32_t pointCount = 0;
        for (int32_t i = 0; i < kMaxManifoldPoints; ++i)
        {
            float separation = Dot(ref.normal, clipPoints2[i].v - ref.v1);

            if (separation <= radius)
            {
                ManifoldPoint *cp = manifold->points + pointCount;

                if (primaryAxis.type == EPAxis::kEdgeA)
                {
                    cp->localPoint = InvTransformPoint(xf, clipPoints2[i].v);
                    cp->id = clipPoints2[i].id;
                }
                else
                {
                    cp->localPoint = clipPoints2[i].v;
                    cp->id.cf.typeA = clipPoints2[i].id.cf.typeB;
                    cp->id.cf.typeB = clipPoints2[i].id.cf.typeA;
                    cp->id.cf.indexA = clipPoints2[i].id.cf.indexB;
                    cp->id.cf.indexB = clipPoints2[i].id.cf.indexA;
                }

                ++pointCount;
            }
        }

        manifold->pointCount = pointCount;
    }

} // namespace kx
