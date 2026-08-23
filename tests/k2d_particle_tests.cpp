#include <k2d/ParticleSystem.h>

#include <cmath>
#include <cstdio>

namespace
{
    bool Near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }

    float VecLength(const Math::Vec2 &v) { return std::sqrt(v.x * v.x + v.y * v.y); }
    float VecAngleDeg(const Math::Vec2 &v) { return std::atan2(v.y, v.x) * 57.29577951f; }

    bool TestPrefabRangesStayInBoundsAndVary()
    {
        k2d::ParticlePrefab prefab;
        prefab.direction = Math::Vec2(0.0f, -1.0f);
        prefab.spreadDegrees = 0.0f;
        prefab.speedMin = 4.0f;
        prefab.speedMax = 10.0f;
        prefab.lifeMin = 0.5f;
        prefab.lifeMax = 2.0f;
        prefab.sizeMin = 2.0f;
        prefab.sizeMax = 6.0f;
        prefab.rotationMin = -30.0f;
        prefab.rotationMax = 30.0f;
        prefab.angularVelocityMin = 10.0f;
        prefab.angularVelocityMax = 90.0f;

        k2d::ParticleSystem particles(64);
        particles.SetMode(k2d::ParticleMode::Persistent);
        particles.SetPrefab(prefab);

        for (int i = 0; i < 40; ++i)
            particles.Emit(Math::Vec2(0.0f), prefab);

        bool inBounds = true;
        bool speedVaries = false, lifeVaries = false, sizeVaries = false;
        bool rotationVaries = false, angularVelocityVaries = false;
        const float firstSpeed = VecLength(particles.Get(0).velocity);
        const float firstLife = particles.Get(0).lifetime;
        const float firstSize = particles.Get(0).size;
        const float firstRotation = particles.Get(0).rotation;
        const float firstAngularVelocity = particles.Get(0).angularVelocity;

        for (size_t i = 0; i < particles.ActiveCount(); ++i)
        {
            const k2d::Particle &p = particles.Get(i);
            const float speed = VecLength(p.velocity);
            if (speed < prefab.speedMin - 0.01f || speed > prefab.speedMax + 0.01f)
                inBounds = false;
            if (p.lifetime < prefab.lifeMin - 0.01f || p.lifetime > prefab.lifeMax + 0.01f)
                inBounds = false;
            if (p.size < prefab.sizeMin - 0.01f || p.size > prefab.sizeMax + 0.01f)
                inBounds = false;
            if (p.rotation < prefab.rotationMin - 0.01f || p.rotation > prefab.rotationMax + 0.01f)
                inBounds = false;
            if (p.angularVelocity < prefab.angularVelocityMin - 0.01f ||
                p.angularVelocity > prefab.angularVelocityMax + 0.01f)
                inBounds = false;

            if (!Near(speed, firstSpeed, 0.05f)) speedVaries = true;
            if (!Near(p.lifetime, firstLife, 0.005f)) lifeVaries = true;
            if (!Near(p.size, firstSize, 0.005f)) sizeVaries = true;
            if (!Near(p.rotation, firstRotation, 0.05f)) rotationVaries = true;
            if (!Near(p.angularVelocity, firstAngularVelocity, 0.05f)) angularVelocityVaries = true;
        }

        return inBounds && speedVaries && lifeVaries && sizeVaries &&
               rotationVaries && angularVelocityVaries;
    }

    bool TestSpreadConeBoundsDirection()
    {
        k2d::ParticlePrefab prefab;
        prefab.direction = Math::Vec2(1.0f, 0.0f);
        prefab.spreadDegrees = 90.0f; 
        prefab.speedMin = prefab.speedMax = 10.0f;
        prefab.lifeMin = prefab.lifeMax = 1.0f;
        prefab.sizeMin = prefab.sizeMax = 1.0f;

        k2d::ParticleSystem particles(64);
        for (int i = 0; i < 40; ++i)
            particles.Emit(Math::Vec2(0.0f), prefab);

        bool inCone = true;
        bool speedExact = true;
        bool varies = false;
        const float firstAngle = VecAngleDeg(particles.Get(0).velocity);

        for (size_t i = 0; i < particles.ActiveCount(); ++i)
        {
            const Math::Vec2 &v = particles.Get(i).velocity;
            const float angle = VecAngleDeg(v);
            if (angle < -45.5f || angle > 45.5f)
                inCone = false;
            if (!Near(VecLength(v), 10.0f, 0.05f))
                speedExact = false;
            if (!Near(angle, firstAngle, 0.5f))
                varies = true;
        }

        return inCone && speedExact && varies;
    }

    bool TestDragReducesSpeedOverTime()
    {
        k2d::ParticlePrefab prefab;
        prefab.direction = Math::Vec2(1.0f, 0.0f);
        prefab.speedMin = prefab.speedMax = 10.0f;
        prefab.lifeMin = prefab.lifeMax = 10.0f;
        prefab.sizeMin = prefab.sizeMax = 1.0f;
        prefab.drag = 2.0f;

        k2d::ParticleSystem particles(4);
        particles.Emit(Math::Vec2(0.0f), prefab);

        float previousSpeed = VecLength(particles.Get(0).velocity);
        bool alwaysDecreasing = true;
        for (int i = 0; i < 5; ++i)
        {
            particles.Update(0.1f);
            float speed = VecLength(particles.Get(0).velocity);
            if (speed >= previousSpeed || speed < 0.0f)
                alwaysDecreasing = false;
            previousSpeed = speed;
        }
        return alwaysDecreasing;
    }

    bool TestFaceDirectionTracksVelocity()
    {
        k2d::ParticlePrefab prefab;
        prefab.direction = Math::Vec2(0.0f, -1.0f);
        prefab.speedMin = prefab.speedMax = 5.0f;
        prefab.lifeMin = prefab.lifeMax = 10.0f;
        prefab.sizeMin = prefab.sizeMax = 1.0f;
        prefab.faceDirection = true;

        k2d::ParticleSystem particles(4);
        particles.SetGravity(Math::Vec2(0.0f, 20.0f)); 
        particles.Emit(Math::Vec2(0.0f), prefab);

        bool ok = true;
        for (int i = 0; i < 10; ++i)
        {
            particles.Update(0.05f);
            const k2d::Particle &p = particles.Get(0);
            float expected = VecAngleDeg(p.velocity);
            if (!Near(p.rotation, expected, 0.1f))
                ok = false;
        }
        return ok;
    }

    bool TestColorRampInterpolates()
    {
        k2d::ParticlePrefab prefab;
        prefab.direction = Math::Vec2(0.0f, -1.0f);
        prefab.speedMin = prefab.speedMax = 0.0f;
        prefab.lifeMin = prefab.lifeMax = 1.0f;
        prefab.sizeMin = prefab.sizeMax = 1.0f;
        prefab.colorStart = k2d::Color(1.0f, 0.0f, 0.0f, 1.0f);
        prefab.colorEnd = k2d::Color(0.0f, 0.0f, 1.0f, 0.0f);

        k2d::ParticleSystem particles(4);
        particles.Emit(Math::Vec2(0.0f), prefab);
        particles.Update(0.5f);

        const k2d::Color &c = particles.Get(0).color;
        return Near(c.r, 0.5f) && Near(c.g, 0.0f) && Near(c.b, 0.5f) && Near(c.a, 0.5f);
    }
}

int main()
{
    k2d::ParticleSystem particles(2);
    particles.SetGravity(Math::Vec2(0.0f, 10.0f));

    bool first = particles.Emit(Math::Vec2(0.0f), Math::Vec2(2.0f, 0.0f), 1.0f, 4.0f);
    bool second = particles.Emit(Math::Vec2(0.0f), Math::Vec2(0.0f), 0.25f, 2.0f);
    bool rejected = !particles.Emit(Math::Vec2(0.0f), Math::Vec2(0.0f), 1.0f, 1.0f);
    particles.Update(0.1f);

    bool motion = particles.ActiveCount() == 2 && particles.Get(0).position.x == 0.2f &&
                  particles.Get(0).position.y == 0.1f;
    particles.Update(0.2f);
    bool expiry = particles.ActiveCount() == 1;
    particles.Update(0.8f);
    bool allExpired = particles.ActiveCount() == 0;

    k2d::ParticlePrefab fire;
    fire.direction = Math::Vec2(0.0f, -1.0f);
    fire.speedMin = fire.speedMax = 2.0f;
    fire.lifeMin = fire.lifeMax = 0.2f;
    fire.sizeMin = fire.sizeMax = 3.0f;
    fire.atlasBounds = Math::Vec4(16.0f, 8.0f, 12.0f, 12.0f);

    k2d::ParticleSystem oneShot(4);
    oneShot.SetMode(k2d::ParticleMode::OneShot);
    oneShot.SetPrefab(fire);
    oneShot.SetOneShotCount(3);
    oneShot.SetEmitterPosition(Math::Vec2(5.0f, 6.0f));
    oneShot.Reset();
    bool oneShotStarted = oneShot.ActiveCount() == 3 && !oneShot.IsPlaying();
    bool atlasBounds = oneShot.Get(0).atlasBounds == fire.atlasBounds;
    oneShot.Update(0.2f);
    bool oneShotFinished = oneShot.IsFinished() && oneShot.ActiveCount() == 0;

    k2d::ParticleSystem loop(8);
    loop.SetMode(k2d::ParticleMode::Loop);
    k2d::ParticlePrefab smoke = fire;
    smoke.lifeMin = smoke.lifeMax = 1.0f;
    loop.SetPrefab(smoke);
    loop.SetEmitterPosition(Math::Vec2(2.0f, 3.0f));
    loop.SetEmissionRate(4.0f);
    loop.Start();
    loop.Update(0.5f);
    bool loopEmits = loop.ActiveCount() == 2 && loop.IsPlaying();
    loop.Stop();
    loop.Update(0.1f);
    bool loopStops = loop.ActiveCount() == 2 && !loop.IsPlaying();

    k2d::ParticleSystem persistent(2);
    persistent.SetMode(k2d::ParticleMode::Persistent);
    persistent.Emit(Math::Vec2(1.0f), fire);
    persistent.Reset();
    bool resetRestarts = persistent.ActiveCount() == 0 && persistent.IsPlaying();

    bool rangesVary = TestPrefabRangesStayInBoundsAndVary();
    bool spreadCone = TestSpreadConeBoundsDirection();
    bool dragDecays = TestDragReducesSpeedOverTime();
    bool faceDirection = TestFaceDirectionTracksVelocity();
    bool colorRamp = TestColorRampInterpolates();

    std::printf("emit=%s,%s capacity=%s motion=%s expiry=%s all_expired=%s one_shot=%s atlas=%s finished=%s loop=%s stop=%s reset=%s\n",
                first ? "pass" : "fail", second ? "pass" : "fail",
                rejected ? "pass" : "fail", motion ? "pass" : "fail",
                expiry ? "pass" : "fail", allExpired ? "pass" : "fail",
                oneShotStarted ? "pass" : "fail", atlasBounds ? "pass" : "fail",
                oneShotFinished ? "pass" : "fail", loopEmits ? "pass" : "fail",
                loopStops ? "pass" : "fail", resetRestarts ? "pass" : "fail");
    std::printf("ranges_vary=%s spread_cone=%s drag_decays=%s face_direction=%s color_ramp=%s\n",
                rangesVary ? "pass" : "fail", spreadCone ? "pass" : "fail",
                dragDecays ? "pass" : "fail", faceDirection ? "pass" : "fail",
                colorRamp ? "pass" : "fail");

    return first && second && rejected && motion && expiry && allExpired &&
           oneShotStarted && atlasBounds && oneShotFinished && loopEmits &&
           loopStops && resetRestarts && rangesVary && spreadCone && dragDecays &&
           faceDirection && colorRamp ? 0 : 1;
}