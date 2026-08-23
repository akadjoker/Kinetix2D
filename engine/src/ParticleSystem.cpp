#include "k2d/ParticleSystem.h"

#include "k2d/Batch.h"
#include "k2d/Matrix2D.h"
#include "k2d/RenderQueue.h"
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

    ParticleSystem::ParticleSystem(size_t capacity)
        : mTexture(nullptr), mGravity(0.0f), mCapacity(capacity), mParticles(),
          mMode(ParticleMode::Persistent), mPrefab(), mEmitterPosition(0.0f),
          mEmissionRate(0.0f), mEmissionAccumulator(0.0f), mOneShotCount(1),
          mPlaying(false), mFinished(false), mEmitterShape(ParticleEmitterShape::Point),
          mEmitterSize(0.0f), mRandomState(0x12345678u)
    {
        mParticles.reserve(capacity);
    }

    void ParticleSystem::SetTexture(Texture *texture)
    {
        mTexture = texture;
    }

    void ParticleSystem::SetCapacity(size_t capacity)
    {
        mCapacity = capacity;
        while (mParticles.size() > mCapacity)
            mParticles.pop_back();
    }

    void ParticleSystem::SetEmissionRate(float particlesPerSecond)
    {
        mEmissionRate = particlesPerSecond > 0.0f ? particlesPerSecond : 0.0f;
    }

    bool ParticleSystem::Emit(const Math::Vec2 &position, const Math::Vec2 &velocity,
                              float lifetime, float size, const Color &color,
                              float rotation, float angularVelocity)
    {
        return Emit(position, velocity, lifetime, size, color, rotation,
                    angularVelocity, Math::Vec4(0.0f));
    }

    bool ParticleSystem::Emit(const Math::Vec2 &position, const Math::Vec2 &velocity,
                              float lifetime, float size, const Color &color,
                              float rotation, float angularVelocity,
                              const Math::Vec4 &atlasBounds)
    {
        if (mParticles.size() >= mCapacity || lifetime <= 0.0f || size <= 0.0f)
            return false;
        Particle particle;
        particle.position = position;
        particle.velocity = velocity;
        particle.colorStart = color;
        particle.colorEnd = color; 
        particle.color = color;
        particle.age = 0.0f;
        particle.lifetime = lifetime;
        particle.size = size;
        particle.startSize = size;
        particle.endSize = size; 
        particle.fadeIn = 0.0f;
        particle.fadeOut = 0.0f;
        particle.rotation = rotation;
        particle.angularVelocity = angularVelocity;
        particle.drag = 0.0f;
        particle.faceDirection = false;
        particle.faceDirectionOffsetDegrees = 0.0f;
        particle.atlasBounds = atlasBounds;
        mParticles.push_back(particle);
        return true;
    }

    bool ParticleSystem::Emit(const Math::Vec2 &position, const ParticlePrefab &prefab)
    {
        if (mParticles.size() >= mCapacity)
            return false;

        const float life = RandomRange(prefab.lifeMin, prefab.lifeMax);
        const float size = RandomRange(prefab.sizeMin, prefab.sizeMax);
        if (life <= 0.0f || size <= 0.0f)
            return false;

        Math::Vec2 dir = prefab.direction;
        const float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        dir = dirLen > 1e-6f ? dir / dirLen : Math::Vec2(0.0f, -1.0f);
        if (prefab.spreadDegrees > 0.0f)
        {
            const float half = prefab.spreadDegrees * 0.5f;
            const float angle = RandomRange(-half, half) * 0.01745329252f; 
            const float c = std::cos(angle), s = std::sin(angle);
            dir = Math::Vec2(dir.x * c - dir.y * s, dir.x * s + dir.y * c);
        }
        const float speed = RandomRange(prefab.speedMin, prefab.speedMax);

        Particle particle;
        particle.position = position;
        particle.velocity = dir * speed;
        particle.colorStart = prefab.colorStart;
        particle.colorEnd = prefab.colorEnd;
        particle.color = prefab.colorStart;
        particle.age = 0.0f;
        particle.lifetime = life;
        particle.size = size;
        particle.startSize = size;
        particle.endSize = prefab.endSize > 0.0f ? prefab.endSize : size;
        particle.fadeIn = prefab.fadeIn > 0.0f ? prefab.fadeIn : 0.0f;
        particle.fadeOut = prefab.fadeOut > 0.0f ? prefab.fadeOut : 0.0f;
        particle.rotation = RandomRange(prefab.rotationMin, prefab.rotationMax);
        particle.angularVelocity = RandomRange(prefab.angularVelocityMin, prefab.angularVelocityMax);
        particle.drag = prefab.drag;
        particle.faceDirection = prefab.faceDirection;
        particle.faceDirectionOffsetDegrees = prefab.faceDirectionOffsetDegrees;
        particle.atlasBounds = prefab.atlasBounds;

        if (particle.faceDirection)
            particle.rotation = std::atan2(particle.velocity.y, particle.velocity.x) * 57.29577951f +
                                 particle.faceDirectionOffsetDegrees;

        mParticles.push_back(particle);
        return true;
    }

    float ParticleSystem::Random01()
    {
        mRandomState = mRandomState * 1664525u + 1013904223u;
        return (float)(mRandomState & 0x00FFFFFFu) / 16777215.0f;
    }

    float ParticleSystem::RandomRange(float minValue, float maxValue)
    {
        if (maxValue <= minValue)
            return minValue;
        return minValue + Random01() * (maxValue - minValue);
    }

    Math::Vec2 ParticleSystem::EmitPosition()
    {
        if (mEmitterShape == ParticleEmitterShape::Point)
            return mEmitterPosition;
        if (mEmitterShape == ParticleEmitterShape::Rectangle)
        {
            return mEmitterPosition + Math::Vec2((Random01() * 2.0f - 1.0f) * mEmitterSize.x,
                                                (Random01() * 2.0f - 1.0f) * mEmitterSize.y);
        }

        float angle = Random01() * 6.28318530718f;
        float radius = std::sqrt(Random01()) * mEmitterSize.x;
        return mEmitterPosition + Math::Vec2(std::cos(angle), std::sin(angle)) * radius;
    }

    void ParticleSystem::Start()
    {
        mPlaying = true;
        mFinished = false;
        if (mMode == ParticleMode::OneShot && mParticles.empty())
        {
            for (size_t i = 0; i < mOneShotCount; ++i)
                Emit(EmitPosition(), mPrefab);
            mPlaying = false;
            mFinished = mParticles.empty();
        }
    }

    void ParticleSystem::Reset()
    {
        mParticles.clear();
        mEmissionAccumulator = 0.0f;
        mFinished = false;
        Start();
    }

    void ParticleSystem::Update(float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;
        if (mPlaying && mMode == ParticleMode::Loop && mEmissionRate > 0.0f)
        {
            mEmissionAccumulator += deltaTime * mEmissionRate;
            while (mEmissionAccumulator >= 1.0f)
            {
                if (!Emit(EmitPosition(), mPrefab))
                    break;
                mEmissionAccumulator -= 1.0f;
            }
        }
        for (size_t i = 0; i < mParticles.size();)
        {
            Particle &particle = mParticles[i];
            particle.age += deltaTime;
            if (particle.age >= particle.lifetime)
            {
                mParticles[i] = mParticles.back();
                mParticles.pop_back();
                continue;
            }
            particle.velocity += mGravity * deltaTime;
            if (particle.drag > 0.0f)
            {
                float dragFactor = 1.0f - particle.drag * deltaTime;
                if (dragFactor < 0.0f)
                    dragFactor = 0.0f;
                particle.velocity *= dragFactor;
            }
            particle.position += particle.velocity * deltaTime;

            if (particle.faceDirection)
                particle.rotation = std::atan2(particle.velocity.y, particle.velocity.x) * 57.29577951f +
                                     particle.faceDirectionOffsetDegrees;
            else
                particle.rotation += particle.angularVelocity * deltaTime;

            float life = particle.age / particle.lifetime;
            float lifeT = Clamp01(life);
            particle.size = particle.startSize + (particle.endSize - particle.startSize) * lifeT;
            particle.color = particle.colorStart + (particle.colorEnd - particle.colorStart) * lifeT;

            float alphaMul = 1.0f;
            if (particle.fadeIn > 0.0f && particle.age < particle.fadeIn)
                alphaMul *= particle.age / particle.fadeIn;
            if (particle.fadeOut > 0.0f && particle.lifetime - particle.age < particle.fadeOut)
                alphaMul *= (particle.lifetime - particle.age) / particle.fadeOut;
            particle.color.a *= alphaMul;
            ++i;
        }
        if (mMode == ParticleMode::OneShot && !mPlaying && mParticles.empty())
            mFinished = true;
    }

    void ParticleSystem::Draw(BatchRenderer &batch) const
    {
        if (!mTexture)
            return;
        for (size_t i = 0; i < mParticles.size(); ++i)
        {
            const Particle &particle = mParticles[i];
            Matrix2D transform = Matrix2D::TRS(particle.position, particle.rotation,
                                               Math::Vec2(particle.size, particle.size));
            batch.SetColor(particle.color.r, particle.color.g, particle.color.b, particle.color.a);
            batch.DrawTexture(mTexture->Id(), transform, 1.0f, 1.0f,
                              mTexture->Width(), mTexture->Height(), 0.5f, 0.5f,
                              particle.atlasBounds.x, particle.atlasBounds.y,
                              particle.atlasBounds.z, particle.atlasBounds.w);
        }
    }

    void ParticleSystem::Submit(RenderQueue &queue, int zIndex, bool ySort,
                                BlendMode blendMode) const
    {
        if (!mTexture || mParticles.empty())
            return;

        RenderItem &item = queue.AddItem(zIndex, ySort);
        item.blendMode = blendMode;
        for (size_t i = 0; i < mParticles.size(); ++i)
        {
            const Particle &particle = mParticles[i];
            Matrix2D transform = Matrix2D::TRS(particle.position, particle.rotation,
                                               Math::Vec2(particle.size, particle.size));

            item.commands.push_back(RenderCommand::MakeTransform(transform));
            RenderCommand command = RenderCommand::MakeRect(mTexture->Id(),
                                                             0.0f, 0.0f, 1.0f, 1.0f);
            command.srcX = particle.atlasBounds.x;
            command.srcY = particle.atlasBounds.y;
            command.srcW = particle.atlasBounds.z;
            command.srcH = particle.atlasBounds.w;
            command.texWidth = mTexture->Width();
            command.texHeight = mTexture->Height();
            command.pivotX = 0.5f;
            command.pivotY = 0.5f;
            command.color = particle.color;
            item.commands.push_back(command);
        }
    }

} 