#include "k2d/ParticlePhysics2D.h"

#include "k2d/Matrix2D.h"
#include "k2d/RenderQueue.h"
#include "k2d/Scene.h"
#include "k2d/Texture.h"

#include <cmath>

namespace k2d
{

    namespace
    {
        float Clamp01(float value)
        {
            if (value < 0.0f)
                return 0.0f;
            if (value > 1.0f)
                return 1.0f;
            return value;
        }
    }

    ParticlePhysics2D::ParticlePhysics2D(Scene &scene)
        : mScene(scene), mParticles(), mGravity(0.0f, 980.0f), mCategoryMask(0xFFFF),
          mMaxParticles(512), mHitCallback(nullptr), mHitUser(nullptr), mNextId(1),
          mRandomState(0x9E3779B9u), mBlendMode(BLEND_MIX), mSubmitted()
    {
    }

    void ParticlePhysics2D::setMaxParticles(std::size_t maxParticles)
    {
        mMaxParticles = maxParticles;
        while (mParticles.size() > mMaxParticles)
            mParticles.pop_back();
    }

    void ParticlePhysics2D::setHitCallback(HitCallback callback, void *user)
    {
        mHitCallback = callback;
        mHitUser = user;
    }

    float ParticlePhysics2D::random01()
    {
        mRandomState = mRandomState * 1664525u + 1013904223u;
        return (float)(mRandomState & 0x00FFFFFFu) / 16777215.0f;
    }

    float ParticlePhysics2D::randomRange(float minValue, float maxValue)
    {
        if (maxValue <= minValue)
            return minValue;
        return minValue + random01() * (maxValue - minValue);
    }

    Math::Vec2 ParticlePhysics2D::randomDirection()
    {
        const float angle = random01() * 6.28318530718f;
        return Math::Vec2(std::cos(angle), std::sin(angle));
    }

    uint32_t ParticlePhysics2D::emit(const ParticleSpawn2D &spawn)
    {
        if (mParticles.size() >= mMaxParticles || spawn.life <= 0.0f)
            return InvalidId;

        PhysicsParticle2D particle;
        particle.position = spawn.position;
        particle.velocity = spawn.velocity;
        particle.radius = spawn.radius;
        particle.life = spawn.life;
        particle.restitution = spawn.restitution;
        particle.drag = spawn.drag;
        particle.initialLife = spawn.life;
        particle.userTag = spawn.userTag;
        particle.response = spawn.response;
        particle.texture = spawn.texture;
        particle.size = spawn.size;
        particle.colorStart = spawn.colorStart;
        particle.colorEnd = spawn.colorEnd;
        particle.zIndex = spawn.zIndex;
        particle.id = mNextId++;
        mParticles.push_back(particle);
        return particle.id;
    }

    void ParticlePhysics2D::explode(const Math::Vec2 &center, std::size_t count, float speedMin,
                                    float speedMax, float radiusMin, float radiusMax, float life,
                                    uint32_t userTag, ParticleResponse response, Texture *texture,
                                    const Math::Vec2 &size, const Color &colorStart, const Color &colorEnd,
                                    int zIndex)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            ParticleSpawn2D spawn;
            spawn.position = center;
            spawn.velocity = randomDirection() * randomRange(speedMin, speedMax);
            spawn.radius = randomRange(radiusMin, radiusMax);
            spawn.life = life;
            spawn.userTag = userTag;
            spawn.response = response;
            spawn.texture = texture;
            spawn.size = size;
            spawn.colorStart = colorStart;
            spawn.colorEnd = colorEnd;
            spawn.zIndex = zIndex;
            if (emit(spawn) == InvalidId)
                return;
        }
    }

    void ParticlePhysics2D::step(float deltaTime)
    {
        if (deltaTime <= 0.0f || mParticles.empty())
            return;

        for (std::size_t i = 0; i < mParticles.size(); ++i)
        {
            PhysicsParticle2D &particle = mParticles[i];
            particle.life -= deltaTime;
            if (particle.settled)
                continue;

            particle.velocity += mGravity * deltaTime;
            const float dragFactor = 1.0f - particle.drag * deltaTime;
            particle.velocity *= dragFactor > 0.0f ? dragFactor : 0.0f;

            const Math::Vec2 displacement = particle.velocity * deltaTime;
            const float travel = displacement.Length();
            if (travel <= 1e-6f)
                continue;

            Math::Vec2 point(0.0f, 0.0f);
            Math::Vec2 normal(0.0f, 0.0f);
            GameObject *other = mScene.raycast(particle.position, displacement / travel,
                                               travel + particle.radius, &point, &normal, nullptr,
                                               mCategoryMask);
            if (!other)
            {
                particle.position += displacement;
                continue;
            }

            if (mHitCallback)
            {
                ParticleHit2D hit;
                hit.particleId = particle.id;
                hit.userTag = particle.userTag;
                hit.point = point;
                hit.normal = normal;
                hit.incomingVelocity = particle.velocity;
                hit.other = other;
                mHitCallback(hit, mHitUser);
            }

            switch (particle.response)
            {
            case ParticleResponse::Kill:
                particle.life = 0.0f;
                break;
            case ParticleResponse::Bounce:
                particle.position = point + normal * particle.radius;
                particle.velocity -= normal * ((1.0f + particle.restitution) * particle.velocity.Dot(normal));
                break;
            case ParticleResponse::Stick:
                particle.position = point + normal * particle.radius;
                particle.velocity = Math::Vec2(0.0f, 0.0f);
                particle.settled = true;
                break;
            }
        }

        for (std::size_t i = 0; i < mParticles.size();)
        {
            if (mParticles[i].life <= 0.0f)
            {
                mParticles[i] = mParticles[mParticles.size() - 1];
                mParticles.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    Color ParticlePhysics2D::colorAt(std::size_t index) const
    {
        const PhysicsParticle2D &particle = mParticles[index];
        const float lifeT =
            particle.initialLife > 0.0f ? Clamp01(1.0f - particle.life / particle.initialLife) : 1.0f;
        return particle.colorStart + (particle.colorEnd - particle.colorStart) * lifeT;
    }

    void ParticlePhysics2D::submit(RenderQueue &queue) const
    {
        const std::size_t total = mParticles.size();
        if (total == 0)
            return;

        mSubmitted.resize(total);
        for (std::size_t i = 0; i < total; ++i)
            mSubmitted[i] = 0;

        for (std::size_t i = 0; i < total; ++i)
        {
            if (mSubmitted[i])
                continue;
            const Texture *texture = mParticles[i].texture;
            if (!texture)
            {
                mSubmitted[i] = 1;
                continue;
            }

            const int zIndex = mParticles[i].zIndex;
            RenderItem &item = queue.AddItem(zIndex, false);
            item.blendMode = mBlendMode;
            for (std::size_t j = i; j < total; ++j)
            {
                const PhysicsParticle2D &particle = mParticles[j];
                if (mSubmitted[j] || particle.texture != texture || particle.zIndex != zIndex)
                    continue;
                mSubmitted[j] = 1;

                item.commands.push_back(
                    RenderCommand::MakeTransform(Matrix2D::TRS(particle.position, 0.0f, particle.size)));
                RenderCommand command = RenderCommand::MakeRect(texture->Id(), 0.0f, 0.0f, 1.0f, 1.0f);
                command.texWidth = texture->Width();
                command.texHeight = texture->Height();
                command.pivotX = 0.5f;
                command.pivotY = 0.5f;
                command.color = colorAt(j);
                item.commands.push_back(command);
            }
        }
    }

}
