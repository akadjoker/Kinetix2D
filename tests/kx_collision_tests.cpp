#include <collide.h>

#include <kx/kx.h>

#include <cstdio>
#include <cstdint>
#include <cmath>

namespace
{

    int gPass = 0;
    int gFail = 0;

    void Check(bool cond, const char *name)
    {
        if (cond)
        {
            ++gPass;
            std::printf("[PASS] %s\n", name);
        }
        else
        {
            ++gFail;
            std::printf("[FAIL] %s\n", name);
        }
    }

    phys::Vec2 ToPhys(const glm::vec2 &v) { return phys::Vec2(v.x, v.y); }

    phys::Transform PhysXf(const glm::vec2 &pos, float angle)
    {
        phys::Transform xf;
        xf.Set(ToPhys(pos), angle);
        return xf;
    }

    bool Near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }
    bool Near(const glm::vec2 &a, const phys::Vec2 &b, float tol = 1e-4f) { return Near(a.x, b.x, tol) && Near(a.y, b.y, tol); }

    bool ManifoldsMatch(const kx::Manifold &km, const phys::Manifold &pm, float tol = 1e-4f)
    {
        if (km.pointCount != pm.pointCount)
            return false;
        if (km.pointCount == 0)
            return true;
        if (!Near(km.localNormal, pm.localNormal, tol))
            return false;
        if (!Near(km.localPoint, pm.localPoint, tol))
            return false;
        for (int32_t i = 0; i < km.pointCount; ++i)
        {
            if (!Near(km.points[i].localPoint, pm.points[i].localPoint, tol))
                return false;
            if (km.points[i].id.key != pm.points[i].id.key)
                return false;
        }
        return true;
    }

    bool ManifoldInvariants(const kx::Manifold &m)
    {
        if (m.pointCount < 0 || m.pointCount > kx::kMaxManifoldPoints)
            return false;
        if (m.pointCount == 0)
            return true;
        if (m.type != kx::Manifold::kCircles)
        {
            float len = std::sqrt(m.localNormal.x * m.localNormal.x + m.localNormal.y * m.localNormal.y);
            if (std::fabs(len - 1.0f) > 1e-4f)
                return false;
        }
        return true;
    }

} 

int main()
{
    {
        kx::Circle a, b;
        a.center = glm::vec2(0.0f, 0.0f);
        a.radius = 1.0f;
        b.center = glm::vec2(0.0f, 0.0f);
        b.radius = 1.5f;

        phys::CircleShape pa, pb;
        pa.center = ToPhys(a.center);
        pa.radius = a.radius;
        pb.center = ToPhys(b.center);
        pb.radius = b.radius;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(2.5f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(2.5f, 0.0f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollideCircles(&km, a, kxfA, b, kxfB);
        phys::CollideCircles(&pm, pa, pxfA, pb, pxfB);

        Check(km.pointCount == 1, "circle-circle exactly touching: pointCount==1");
        Check(ManifoldsMatch(km, pm), "circle-circle exactly touching matches phys");
    }

    {
        kx::Circle a, b;
        a.center = glm::vec2(0.0f, 0.0f);
        a.radius = 1.0f;
        b.center = glm::vec2(0.0f, 0.0f);
        b.radius = 1.5f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);

        kx::Manifold km;
        kx::CollideCircles(&km, a, kxfA, b, kxfB);

        Check(km.pointCount == 1, "circle-circle coincident centers: pointCount==1");
        Check(Near(km.localNormal, phys::Vec2(0.0f, 0.0f)) == false || (km.localNormal.x == 0.0f && km.localNormal.y == 0.0f),
              "circle-circle coincident centers: degenerate localNormal is zero placeholder");
    }

    {
        kx::Circle a, b;
        a.center = glm::vec2(0.0f, 0.0f);
        a.radius = 1.0f;
        b.center = glm::vec2(0.0f, 0.0f);
        b.radius = 1.0f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(2.0001f, 0.0f), 0.0f);

        kx::Manifold km;
        kx::CollideCircles(&km, a, kxfA, b, kxfB);
        Check(km.pointCount == 0, "circle-circle separated by epsilon: pointCount==0");
    }

    {
        kx::Circle a, b;
        a.center = glm::vec2(0.0f, 0.0f);
        a.radius = 1.0f;
        b.center = glm::vec2(0.0f, 0.0f);
        b.radius = 5.0f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.5f, 0.0f), 0.0f);

        kx::Manifold km;
        kx::CollideCircles(&km, a, kxfA, b, kxfB);
        Check(km.pointCount == 1, "circle-circle deep overlap (center inside): pointCount==1");
    }

    {
        kx::Polygon poly;
        poly.SetAsBox(1.0f, 1.0f);
        kx::Circle circle;
        circle.center = glm::vec2(0.0f, 0.0f);
        circle.radius = 0.5f;

        phys::PolygonShape pp;
        pp.SetAsBox(1.0f, 1.0f);
        phys::CircleShape pc;
        pc.center = phys::Vec2(0.0f, 0.0f);
        pc.radius = 0.5f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygonAndCircle(&km, poly, kxfA, circle, kxfB);
        phys::CollidePolygonAndCircle(&pm, pp, pxfA, pc, pxfB);

        Check(km.pointCount == 1, "circle on polygon centroid: pointCount==1");
        Check(ManifoldsMatch(km, pm), "circle on polygon centroid matches phys");
    }

    {
        kx::Polygon poly;
        poly.SetAsBox(1.0f, 1.0f);
        kx::Circle circle;
        circle.center = glm::vec2(0.0f, 0.0f);
        circle.radius = 0.3f;

        phys::PolygonShape pp;
        pp.SetAsBox(1.0f, 1.0f);
        phys::CircleShape pc;
        pc.center = phys::Vec2(0.0f, 0.0f);
        pc.radius = 0.3f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);

        glm::vec2 cornerPos(1.5f, 1.5f);
        kx::Transform kxfB = kx::MakeTransform(cornerPos, 0.0f);
        phys::Transform pxfB = PhysXf(cornerPos, 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygonAndCircle(&km, poly, kxfA, circle, kxfB);
        phys::CollidePolygonAndCircle(&pm, pp, pxfA, pc, pxfB);

        Check(ManifoldsMatch(km, pm), "circle on polygon vertex/corner region matches phys");

        glm::vec2 facePos(1.2f, 0.0f);
        kx::Transform kxfC = kx::MakeTransform(facePos, 0.0f);
        phys::Transform pxfC = PhysXf(facePos, 0.0f);

        kx::Manifold km2;
        phys::Manifold pm2;
        kx::CollidePolygonAndCircle(&km2, poly, kxfA, circle, kxfC);
        phys::CollidePolygonAndCircle(&pm2, pp, pxfA, pc, pxfC);

        Check(ManifoldsMatch(km2, pm2), "circle on polygon edge/face region matches phys");
    }

    {
        kx::Polygon a, b;
        a.SetAsBox(1.0f, 1.0f);
        b.SetAsBox(1.0f, 1.0f);
        phys::PolygonShape pa, pb;
        pa.SetAsBox(1.0f, 1.0f);
        pb.SetAsBox(1.0f, 1.0f);

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(1.9f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(1.9f, 0.0f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygons(&km, a, kxfA, b, kxfB);
        phys::CollidePolygons(&pm, pa, pxfA, pb, pxfB);

        Check(km.pointCount == 2, "box-box face-face aligned: pointCount==2");
        Check(ManifoldsMatch(km, pm), "box-box face-face aligned matches phys");
    }

    {
        kx::Polygon a, b;
        a.SetAsBox(1.0f, 1.0f);
        b.SetAsBox(1.0f, 1.0f);
        phys::PolygonShape pa, pb;
        pa.SetAsBox(1.0f, 1.0f);
        pb.SetAsBox(1.0f, 1.0f);

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(1.9f, 1.9f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(1.9f, 1.9f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygons(&km, a, kxfA, b, kxfB);
        phys::CollidePolygons(&pm, pa, pxfA, pb, pxfB);

        Check(ManifoldsMatch(km, pm), "box-box corner-corner diagonal matches phys");
    }

    {
        kx::Polygon big, small;
        big.SetAsBox(5.0f, 5.0f);
        small.SetAsBox(0.5f, 0.5f);
        phys::PolygonShape pbig, psmall;
        pbig.SetAsBox(5.0f, 5.0f);
        psmall.SetAsBox(0.5f, 0.5f);

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.5f, 0.5f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(0.5f, 0.5f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygons(&km, big, kxfA, small, kxfB);
        phys::CollidePolygons(&pm, pbig, pxfA, psmall, pxfB);

        Check(km.pointCount > 0, "small box fully inside big box: pointCount>0");
        Check(ManifoldsMatch(km, pm), "small box fully inside big box matches phys");
    }

    {
        kx::Polygon a, b;
        a.SetAsBox(1.0f, 1.0f);
        b.SetAsBox(1.0f, 1.0f);
        phys::PolygonShape pa, pb;
        pa.SetAsBox(1.0f, 1.0f);
        pb.SetAsBox(1.0f, 1.0f);

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygons(&km, a, kxfA, b, kxfB);
        phys::CollidePolygons(&pm, pa, pxfA, pb, pxfB);

        Check(ManifoldsMatch(km, pm), "identical overlapping boxes matches phys");
        Check(ManifoldInvariants(km), "identical overlapping boxes invariants hold");
    }

    {
        kx::Polygon a, b;
        a.SetAsBox(1.0f, 1.0f);
        b.SetAsBox(0.7f, 0.7f);
        phys::PolygonShape pa, pb;
        pa.SetAsBox(1.0f, 1.0f);
        pb.SetAsBox(0.7f, 0.7f);

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(1.4f, 0.0f), kx::kPi * 0.25f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(1.4f, 0.0f), kx::kPi * 0.25f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollidePolygons(&km, a, kxfA, b, kxfB);
        phys::CollidePolygons(&pm, pa, pxfA, pb, pxfB);

        Check(ManifoldsMatch(km, pm), "45-degree rotated box vs axis-aligned box matches phys");
    }

    {
        kx::Edge edge;
        edge.SetTwoSided(glm::vec2(-5.0f, 0.0f), glm::vec2(5.0f, 0.0f));
        kx::Circle circle;
        circle.center = glm::vec2(0.0f, 0.0f);
        circle.radius = 1.0f;

        phys::EdgeShape pe;
        pe.SetTwoSided(phys::Vec2(-5.0f, 0.0f), phys::Vec2(5.0f, 0.0f));
        phys::CircleShape pc;
        pc.center = phys::Vec2(0.0f, 0.0f);
        pc.radius = 1.0f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);

        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.0f, 0.8f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(0.0f, 0.8f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollideEdgeAndCircle(&km, edge, kxfA, circle, kxfB);
        phys::CollideEdgeAndCircle(&pm, pe, pxfA, pc, pxfB);

        Check(km.pointCount == 1, "edge-circle interior region hit: pointCount==1");
        Check(ManifoldsMatch(km, pm), "edge-circle interior region matches phys");

        kx::Transform kxfC = kx::MakeTransform(glm::vec2(6.0f, 0.0f), 0.0f);
        phys::Transform pxfC = PhysXf(glm::vec2(6.0f, 0.0f), 0.0f);
        kx::Manifold km2;
        phys::Manifold pm2;
        kx::CollideEdgeAndCircle(&km2, edge, kxfA, circle, kxfC);
        phys::CollideEdgeAndCircle(&pm2, pe, pxfA, pc, pxfC);
        Check(ManifoldsMatch(km2, pm2), "edge-circle beyond endpoint B (voronoi region) matches phys");

        kx::Transform kxfD = kx::MakeTransform(glm::vec2(-6.0f, 0.0f), 0.0f);
        phys::Transform pxfD = PhysXf(glm::vec2(-6.0f, 0.0f), 0.0f);
        kx::Manifold km3;
        phys::Manifold pm3;
        kx::CollideEdgeAndCircle(&km3, edge, kxfA, circle, kxfD);
        phys::CollideEdgeAndCircle(&pm3, pe, pxfA, pc, pxfD);
        Check(ManifoldsMatch(km3, pm3), "edge-circle beyond endpoint A (voronoi region) matches phys");

        kx::Transform kxfE = kx::MakeTransform(glm::vec2(5.0f, 0.0f), 0.0f);
        phys::Transform pxfE = PhysXf(glm::vec2(5.0f, 0.0f), 0.0f);
        kx::Manifold km4;
        phys::Manifold pm4;
        kx::CollideEdgeAndCircle(&km4, edge, kxfA, circle, kxfE);
        phys::CollideEdgeAndCircle(&pm4, pe, pxfA, pc, pxfE);
        Check(ManifoldsMatch(km4, pm4), "edge-circle exactly at endpoint matches phys");
    }

    {
        kx::Edge edge;
        edge.SetTwoSided(glm::vec2(-5.0f, 0.0f), glm::vec2(5.0f, 0.0f));
        kx::Polygon poly;
        poly.SetAsBox(1.0f, 1.0f);

        phys::EdgeShape pe;
        pe.SetTwoSided(phys::Vec2(-5.0f, 0.0f), phys::Vec2(5.0f, 0.0f));
        phys::PolygonShape pp;
        pp.SetAsBox(1.0f, 1.0f);

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);

        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.0f, 0.5f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(0.0f, 0.5f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollideEdgeAndPolygon(&km, edge, kxfA, poly, kxfB);
        phys::CollideEdgeAndPolygon(&pm, pe, pxfA, pp, pxfB);

        Check(ManifoldsMatch(km, pm), "polygon crossing edge segment matches phys");
        Check(km.pointCount == 2, "polygon resting flush on edge: pointCount==2");

        kx::Transform kxfC = kx::MakeTransform(glm::vec2(0.0f, 1.0f), 0.0f);
        phys::Transform pxfC = PhysXf(glm::vec2(0.0f, 1.0f), 0.0f);
        kx::Manifold km2;
        phys::Manifold pm2;
        kx::CollideEdgeAndPolygon(&km2, edge, kxfA, poly, kxfC);
        phys::CollideEdgeAndPolygon(&pm2, pe, pxfA, pp, pxfC);
        Check(ManifoldsMatch(km2, pm2), "polygon parallel resting on edge matches phys");
    }

    {
        kx::Circle a, b;
        a.center = glm::vec2(0.0f, 0.0f);
        a.radius = 0.01f;
        b.center = glm::vec2(0.0f, 0.0f);
        b.radius = 0.01f;

        phys::CircleShape pa, pb;
        pa.center = phys::Vec2(0.0f, 0.0f);
        pa.radius = 0.01f;
        pb.center = phys::Vec2(0.0f, 0.0f);
        pb.radius = 0.01f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(0.015f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(0.015f, 0.0f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollideCircles(&km, a, kxfA, b, kxfB);
        phys::CollideCircles(&pm, pa, pxfA, pb, pxfB);
        Check(ManifoldsMatch(km, pm), "tiny circles (radius 0.01) matches phys");
    }

    {
        kx::Circle a, b;
        a.center = glm::vec2(0.0f, 0.0f);
        a.radius = 100.0f;
        b.center = glm::vec2(0.0f, 0.0f);
        b.radius = 100.0f;

        phys::CircleShape pa, pb;
        pa.center = phys::Vec2(0.0f, 0.0f);
        pa.radius = 100.0f;
        pb.center = phys::Vec2(0.0f, 0.0f);
        pb.radius = 100.0f;

        kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);
        kx::Transform kxfB = kx::MakeTransform(glm::vec2(150.0f, 0.0f), 0.0f);
        phys::Transform pxfA = PhysXf(glm::vec2(0.0f, 0.0f), 0.0f);
        phys::Transform pxfB = PhysXf(glm::vec2(150.0f, 0.0f), 0.0f);

        kx::Manifold km;
        phys::Manifold pm;
        kx::CollideCircles(&km, a, kxfA, b, kxfB);
        phys::CollideCircles(&pm, pa, pxfA, pb, pxfB);
        Check(ManifoldsMatch(km, pm), "large circles (radius 100) matches phys");
        Check(ManifoldInvariants(km), "large circles invariants hold");
    }

    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}