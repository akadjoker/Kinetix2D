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
        unsigned int PackColor(const glm::vec4 &color)
        {
            auto channel = [](float value) -> unsigned int {
                if (value <= 0.0f)
                    return 0u;
                if (value >= 1.0f)
                    return 255u;
                return (unsigned int)std::lround(value * 255.0f);
            };
            return channel(color.r) | (channel(color.g) << 8) |
                   (channel(color.b) << 16) | (channel(color.a) << 24);
        }
    }

    ParticleSystem::ParticleSystem(size_t capacity)
        : mTexture(nullptr), mGravity(0.0f), mCapacity(capacity), mParticles(),
          mMode(ParticleMode::Persistent), mPrefab(), mEmitterPosition(0.0f),
          mEmissionRate(0.0f), mEmissionAccumulator(0.0f), mOneShotCount(1),
          mPlaying(false), mFinished(false)
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

    bool ParticleSystem::Emit(const glm::vec2 &position, const glm::vec2 &velocity,
                              float lifetime, float size, const glm::vec4 &color,
                              float rotation, float angularVelocity)
    {
        return Emit(position, velocity, lifetime, size, color, rotation,
                    angularVelocity, glm::vec4(0.0f));
    }

    bool ParticleSystem::Emit(const glm::vec2 &position, const glm::vec2 &velocity,
                              float lifetime, float size, const glm::vec4 &color,
                              float rotation, float angularVelocity,
                              const glm::vec4 &atlasBounds)
    {
        if (mParticles.size() >= mCapacity || lifetime <= 0.0f || size <= 0.0f)
            return false;
        Particle particle;
        particle.position = position;
        particle.velocity = velocity;
        particle.color = color;
        particle.age = 0.0f;
        particle.lifetime = lifetime;
        particle.size = size;
        particle.rotation = rotation;
        particle.angularVelocity = angularVelocity;
        particle.atlasBounds = atlasBounds;
        mParticles.push_back(particle);
        return true;
    }

    bool ParticleSystem::Emit(const glm::vec2 &position, const ParticlePrefab &prefab)
    {
        return Emit(position, prefab.velocity, prefab.lifetime, prefab.size,
                    prefab.color, prefab.rotation, prefab.angularVelocity,
                    prefab.atlasBounds);
    }

    void ParticleSystem::Start()
    {
        mPlaying = true;
        mFinished = false;
        if (mMode == ParticleMode::OneShot && mParticles.empty())
        {
            for (size_t i = 0; i < mOneShotCount; ++i)
                Emit(mEmitterPosition, mPrefab);
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
                if (!Emit(mEmitterPosition, mPrefab))
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
            particle.position += particle.velocity * deltaTime;
            particle.rotation += particle.angularVelocity * deltaTime;
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
                                               glm::vec2(particle.size, particle.size));
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
                                               glm::vec2(particle.size, particle.size));

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
            command.color = PackColor(particle.color);
            item.commands.push_back(command);
        }
    }

} // namespace k2d
