#include <cstdio>
#include <cstdint>
#include <cmath>

#include <collide.h>

#include <kx/kx.h>

namespace
{

    struct Lcg
    {
        uint32_t state;
        explicit Lcg(uint32_t seed) : state(seed) {}

        uint32_t Next()
        {
            state = state * 1664525u + 1013904223u;
            return state;
        }

        float Float01()
        {
            return (float)(Next() >> 8) / (float)(1u << 24);
        }

        float Range(float lo, float hi)
        {
            return lo + Float01() * (hi - lo);
        }
    };

    const float kTol = 1e-4f;
    const int kCases = 15000;

    phys::Vec2 ToPhys(const glm::vec2 &v) { return phys::Vec2(v.x, v.y); }

    phys::Transform MakePhysTransform(const glm::vec2 &pos, float angle)
    {
        phys::Transform xf;
        xf.Set(ToPhys(pos), angle);
        return xf;
    }

    glm::vec2 RandVec2(Lcg &rng, float lo, float hi)
    {
        return glm::vec2(rng.Range(lo, hi), rng.Range(lo, hi));
    }

    void MakeRandomCircles(Lcg &rng, phys::CircleShape &pc, kx::Circle &kc)
    {
        float radius = rng.Range(0.1f, 3.0f);
        glm::vec2 center = RandVec2(rng, -1.0f, 1.0f);
        pc.center = ToPhys(center);
        pc.radius = radius;
        kc.center = center;
        kc.radius = radius;
    }

    void MakeRandomBox(Lcg &rng, phys::PolygonShape &pp, kx::Polygon &kp)
    {
        float hw = rng.Range(0.1f, 3.0f);
        float hh = rng.Range(0.1f, 3.0f);
        pp.SetAsBox(hw, hh);
        kp.SetAsBox(hw, hh);
    }

    bool MakeRandomEdge(Lcg &rng, phys::EdgeShape &pe, kx::Edge &ke)
    {
        glm::vec2 a = RandVec2(rng, -5.0f, 5.0f);
        glm::vec2 b = RandVec2(rng, -5.0f, 5.0f);
        if (glm::length(b - a) < 0.05f)
            return false;
        pe.SetTwoSided(ToPhys(a), ToPhys(b));
        ke.SetTwoSided(a, b);
        return true;
    }

    bool NearlyEqual(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
    bool NearlyEqual(const glm::vec2 &a, const phys::Vec2 &b, float tol)
    {
        return NearlyEqual(a.x, b.x, tol) && NearlyEqual(a.y, b.y, tol);
    }

    bool CompareManifolds(const kx::Manifold &km, const phys::Manifold &pm, char *reason, size_t reasonSize)
    {
        if (km.pointCount != pm.pointCount)
        {
            std::snprintf(reason, reasonSize, "pointCount kx=%d phys=%d", km.pointCount, pm.pointCount);
            return false;
        }

        if (km.pointCount == 0)
            return true;

        if (!NearlyEqual(km.localNormal, pm.localNormal, kTol))
        {
            std::snprintf(reason, reasonSize, "localNormal kx=(%f,%f) phys=(%f,%f)",
                          km.localNormal.x, km.localNormal.y, pm.localNormal.x, pm.localNormal.y);
            return false;
        }

        if (!NearlyEqual(km.localPoint, pm.localPoint, kTol))
        {
            std::snprintf(reason, reasonSize, "localPoint kx=(%f,%f) phys=(%f,%f)",
                          km.localPoint.x, km.localPoint.y, pm.localPoint.x, pm.localPoint.y);
            return false;
        }

        for (int32_t i = 0; i < km.pointCount; ++i)
        {
            if (!NearlyEqual(km.points[i].localPoint, pm.points[i].localPoint, kTol))
            {
                std::snprintf(reason, reasonSize, "point[%d].localPoint kx=(%f,%f) phys=(%f,%f)",
                              i, km.points[i].localPoint.x, km.points[i].localPoint.y,
                              pm.points[i].localPoint.x, pm.points[i].localPoint.y);
                return false;
            }
            if (km.points[i].id.key != pm.points[i].id.key)
            {
                std::snprintf(reason, reasonSize, "point[%d].id kx=%u phys=%u",
                              i, km.points[i].id.key, pm.points[i].id.key);
                return false;
            }
        }

        return true;
    }

    struct Result
    {
        int pass = 0;
        int fail = 0;
        int mismatchesPrinted = 0;
    };

    void Report(const char *name, const Result &r, int total)
    {
        std::printf("%-24s pass=%d fail=%d total=%d (%.4f%%)\n",
                    name, r.pass, r.fail, total, 100.0 * r.pass / total);
    }

} // namespace

int main()
{
    int totalFail = 0;

    {
        Result r;
        Lcg rng(1);
        for (int i = 0; i < kCases; ++i)
        {
            phys::CircleShape pcA, pcB;
            kx::Circle kcA, kcB;
            MakeRandomCircles(rng, pcA, kcA);
            MakeRandomCircles(rng, pcB, kcB);

            glm::vec2 posA = RandVec2(rng, -5.0f, 5.0f);
            glm::vec2 posB = RandVec2(rng, -5.0f, 5.0f);
            float angleA = rng.Range(-kx::kPi, kx::kPi);
            float angleB = rng.Range(-kx::kPi, kx::kPi);

            phys::Transform pxfA = MakePhysTransform(posA, angleA);
            phys::Transform pxfB = MakePhysTransform(posB, angleB);
            kx::Transform kxfA = kx::MakeTransform(posA, angleA);
            kx::Transform kxfB = kx::MakeTransform(posB, angleB);

            phys::Manifold pm;
            kx::Manifold km;
            phys::CollideCircles(&pm, pcA, pxfA, pcB, pxfB);
            kx::CollideCircles(&km, kcA, kxfA, kcB, kxfB);

            char reason[256];
            if (CompareManifolds(km, pm, reason, sizeof(reason)))
            {
                ++r.pass;
            }
            else
            {
                ++r.fail;
                if (r.mismatchesPrinted < 5)
                {
                    ++r.mismatchesPrinted;
                    std::printf("MISMATCH CollideCircles case %d: %s\n", i, reason);
                    std::printf("  circleA r=%f c=(%f,%f) posA=(%f,%f) angleA=%f\n",
                                kcA.radius, kcA.center.x, kcA.center.y, posA.x, posA.y, angleA);
                    std::printf("  circleB r=%f c=(%f,%f) posB=(%f,%f) angleB=%f\n",
                                kcB.radius, kcB.center.x, kcB.center.y, posB.x, posB.y, angleB);
                }
            }
        }
        Report("CollideCircles", r, kCases);
        totalFail += r.fail;
    }

    {
        Result r;
        Lcg rng(2);
        for (int i = 0; i < kCases; ++i)
        {
            phys::PolygonShape ppA;
            kx::Polygon kpA;
            MakeRandomBox(rng, ppA, kpA);

            phys::CircleShape pcB;
            kx::Circle kcB;
            MakeRandomCircles(rng, pcB, kcB);

            glm::vec2 posA = RandVec2(rng, -5.0f, 5.0f);
            glm::vec2 posB = RandVec2(rng, -5.0f, 5.0f);
            float angleA = rng.Range(-kx::kPi, kx::kPi);
            float angleB = rng.Range(-kx::kPi, kx::kPi);

            phys::Transform pxfA = MakePhysTransform(posA, angleA);
            phys::Transform pxfB = MakePhysTransform(posB, angleB);
            kx::Transform kxfA = kx::MakeTransform(posA, angleA);
            kx::Transform kxfB = kx::MakeTransform(posB, angleB);

            phys::Manifold pm;
            kx::Manifold km;
            phys::CollidePolygonAndCircle(&pm, ppA, pxfA, pcB, pxfB);
            kx::CollidePolygonAndCircle(&km, kpA, kxfA, kcB, kxfB);

            char reason[256];
            if (CompareManifolds(km, pm, reason, sizeof(reason)))
            {
                ++r.pass;
            }
            else
            {
                ++r.fail;
                if (r.mismatchesPrinted < 5)
                {
                    ++r.mismatchesPrinted;
                    std::printf("MISMATCH CollidePolygonAndCircle case %d: %s\n", i, reason);
                }
            }
        }
        Report("CollidePolygonAndCircle", r, kCases);
        totalFail += r.fail;
    }

    {
        Result r;
        Lcg rng(3);
        for (int i = 0; i < kCases; ++i)
        {
            phys::PolygonShape ppA, ppB;
            kx::Polygon kpA, kpB;
            MakeRandomBox(rng, ppA, kpA);
            MakeRandomBox(rng, ppB, kpB);

            glm::vec2 posA = RandVec2(rng, -5.0f, 5.0f);
            glm::vec2 posB = RandVec2(rng, -5.0f, 5.0f);
            float angleA = rng.Range(-kx::kPi, kx::kPi);
            float angleB = rng.Range(-kx::kPi, kx::kPi);

            phys::Transform pxfA = MakePhysTransform(posA, angleA);
            phys::Transform pxfB = MakePhysTransform(posB, angleB);
            kx::Transform kxfA = kx::MakeTransform(posA, angleA);
            kx::Transform kxfB = kx::MakeTransform(posB, angleB);

            phys::Manifold pm;
            kx::Manifold km;
            phys::CollidePolygons(&pm, ppA, pxfA, ppB, pxfB);
            kx::CollidePolygons(&km, kpA, kxfA, kpB, kxfB);

            char reason[256];
            if (CompareManifolds(km, pm, reason, sizeof(reason)))
            {
                ++r.pass;
            }
            else
            {
                ++r.fail;
                if (r.mismatchesPrinted < 5)
                {
                    ++r.mismatchesPrinted;
                    std::printf("MISMATCH CollidePolygons case %d: %s\n", i, reason);
                }
            }
        }
        Report("CollidePolygons", r, kCases);
        totalFail += r.fail;
    }

    {
        Result r;
        Lcg rng(4);
        int total = 0;
        while (total < kCases)
        {
            phys::EdgeShape peA;
            kx::Edge keA;
            if (!MakeRandomEdge(rng, peA, keA))
                continue;

            phys::CircleShape pcB;
            kx::Circle kcB;
            MakeRandomCircles(rng, pcB, kcB);

            glm::vec2 posB = RandVec2(rng, -5.0f, 5.0f);
            float angleB = rng.Range(-kx::kPi, kx::kPi);

            phys::Transform pxfA;
            pxfA.SetIdentity();
            kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);

            phys::Transform pxfB = MakePhysTransform(posB, angleB);
            kx::Transform kxfB = kx::MakeTransform(posB, angleB);

            phys::Manifold pm;
            kx::Manifold km;
            phys::CollideEdgeAndCircle(&pm, peA, pxfA, pcB, pxfB);
            kx::CollideEdgeAndCircle(&km, keA, kxfA, kcB, kxfB);

            char reason[256];
            if (CompareManifolds(km, pm, reason, sizeof(reason)))
            {
                ++r.pass;
            }
            else
            {
                ++r.fail;
                if (r.mismatchesPrinted < 5)
                {
                    ++r.mismatchesPrinted;
                    std::printf("MISMATCH CollideEdgeAndCircle case %d: %s\n", total, reason);
                }
            }
            ++total;
        }
        Report("CollideEdgeAndCircle", r, total);
        totalFail += r.fail;
    }

    {
        Result r;
        Lcg rng(5);
        int total = 0;
        while (total < kCases)
        {
            phys::EdgeShape peA;
            kx::Edge keA;
            if (!MakeRandomEdge(rng, peA, keA))
                continue;

            phys::PolygonShape ppB;
            kx::Polygon kpB;
            MakeRandomBox(rng, ppB, kpB);

            glm::vec2 posB = RandVec2(rng, -5.0f, 5.0f);
            float angleB = rng.Range(-kx::kPi, kx::kPi);

            phys::Transform pxfA;
            pxfA.SetIdentity();
            kx::Transform kxfA = kx::MakeTransform(glm::vec2(0.0f, 0.0f), 0.0f);

            phys::Transform pxfB = MakePhysTransform(posB, angleB);
            kx::Transform kxfB = kx::MakeTransform(posB, angleB);

            phys::Manifold pm;
            kx::Manifold km;
            phys::CollideEdgeAndPolygon(&pm, peA, pxfA, ppB, pxfB);
            kx::CollideEdgeAndPolygon(&km, keA, kxfA, kpB, kxfB);

            char reason[256];
            if (CompareManifolds(km, pm, reason, sizeof(reason)))
            {
                ++r.pass;
            }
            else
            {
                ++r.fail;
                if (r.mismatchesPrinted < 5)
                {
                    ++r.mismatchesPrinted;
                    std::printf("MISMATCH CollideEdgeAndPolygon case %d: %s\n", total, reason);
                }
            }
            ++total;
        }
        Report("CollideEdgeAndPolygon", r, total);
        totalFail += r.fail;
    }

    if (totalFail == 0)
    {
        std::printf("ALL PASS\n");
        return 0;
    }

    std::printf("TOTAL FAILURES: %d\n", totalFail);
    return 1;
}
