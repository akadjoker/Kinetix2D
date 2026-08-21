#include <k2d/ParticleSystem.h>

#include <cstdio>

int main()
{
    k2d::ParticleSystem particles(2);
    particles.SetGravity(glm::vec2(0.0f, 10.0f));

    bool first = particles.Emit(glm::vec2(0.0f), glm::vec2(2.0f, 0.0f), 1.0f, 4.0f);
    bool second = particles.Emit(glm::vec2(0.0f), glm::vec2(0.0f), 0.25f, 2.0f);
    bool rejected = !particles.Emit(glm::vec2(0.0f), glm::vec2(0.0f), 1.0f, 1.0f);
    particles.Update(0.1f);

    bool motion = particles.ActiveCount() == 2 && particles.Get(0).position.x == 0.2f &&
                  particles.Get(0).position.y == 0.1f;
    particles.Update(0.2f);
    bool expiry = particles.ActiveCount() == 1;
    particles.Update(0.8f);
    bool allExpired = particles.ActiveCount() == 0;

    k2d::ParticlePrefab fire;
    fire.velocity = glm::vec2(0.0f, -2.0f);
    fire.lifetime = 0.2f;
    fire.size = 3.0f;
    fire.atlasBounds = glm::vec4(16.0f, 8.0f, 12.0f, 12.0f);

    k2d::ParticleSystem oneShot(4);
    oneShot.SetMode(k2d::ParticleMode::OneShot);
    oneShot.SetPrefab(fire);
    oneShot.SetOneShotCount(3);
    oneShot.SetEmitterPosition(glm::vec2(5.0f, 6.0f));
    oneShot.Reset();
    bool oneShotStarted = oneShot.ActiveCount() == 3 && !oneShot.IsPlaying();
    bool atlasBounds = oneShot.Get(0).atlasBounds == fire.atlasBounds;
    oneShot.Update(0.2f);
    bool oneShotFinished = oneShot.IsFinished() && oneShot.ActiveCount() == 0;

    k2d::ParticleSystem loop(8);
    loop.SetMode(k2d::ParticleMode::Loop);
    k2d::ParticlePrefab smoke = fire;
    smoke.lifetime = 1.0f;
    loop.SetPrefab(smoke);
    loop.SetEmitterPosition(glm::vec2(2.0f, 3.0f));
    loop.SetEmissionRate(4.0f);
    loop.Start();
    loop.Update(0.5f);
    bool loopEmits = loop.ActiveCount() == 2 && loop.IsPlaying();
    loop.Stop();
    loop.Update(0.1f);
    bool loopStops = loop.ActiveCount() == 2 && !loop.IsPlaying();

    k2d::ParticleSystem persistent(2);
    persistent.SetMode(k2d::ParticleMode::Persistent);
    persistent.Emit(glm::vec2(1.0f), fire);
    persistent.Reset();
    bool resetRestarts = persistent.ActiveCount() == 0 && persistent.IsPlaying();

    std::printf("emit=%s,%s capacity=%s motion=%s expiry=%s all_expired=%s one_shot=%s atlas=%s finished=%s loop=%s stop=%s reset=%s\n",
                first ? "pass" : "fail", second ? "pass" : "fail",
                rejected ? "pass" : "fail", motion ? "pass" : "fail",
                expiry ? "pass" : "fail", allExpired ? "pass" : "fail",
                oneShotStarted ? "pass" : "fail", atlasBounds ? "pass" : "fail",
                oneShotFinished ? "pass" : "fail", loopEmits ? "pass" : "fail",
                loopStops ? "pass" : "fail", resetRestarts ? "pass" : "fail");
    return first && second && rejected && motion && expiry && allExpired &&
           oneShotStarted && atlasBounds && oneShotFinished && loopEmits &&
           loopStops && resetRestarts ? 0 : 1;
}
