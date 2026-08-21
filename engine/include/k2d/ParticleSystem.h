#pragma once

#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    enum class ParticleMode
    {
        OneShot,
        Persistent,
        Loop
    };

    struct ParticlePrefab
    {
        glm::vec2 velocity = glm::vec2(0.0f);
        float lifetime = 1.0f;
        float size = 1.0f;
        glm::vec4 color = glm::vec4(1.0f);
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
        // Pixel bounds in the atlas: x, y, width, height.
        // Zero width/height means the complete texture.
        glm::vec4 atlasBounds = glm::vec4(0.0f);
    };

    class BatchRenderer;
    class RenderQueue;
    class Texture;

    struct Particle
    {
        glm::vec2 position;
        glm::vec2 velocity;
        glm::vec4 color;
        float age;
        float lifetime;
        float size;
        float rotation;
        float angularVelocity;
        glm::vec4 atlasBounds;
    };

    class ParticleSystem
    {
    public:
        explicit ParticleSystem(size_t capacity = 256);

        void SetTexture(Texture *texture);
        Texture *GetTexture() const { return mTexture; }
        void SetGravity(const glm::vec2 &gravity) { mGravity = gravity; }
        void SetCapacity(size_t capacity);

        void SetMode(ParticleMode mode) { mMode = mode; }
        ParticleMode GetMode() const { return mMode; }
        void SetPrefab(const ParticlePrefab &prefab) { mPrefab = prefab; }
        const ParticlePrefab &GetPrefab() const { return mPrefab; }
        void SetEmitterPosition(const glm::vec2 &position) { mEmitterPosition = position; }
        void SetEmissionRate(float particlesPerSecond);
        float EmissionRate() const { return mEmissionRate; }
        void SetOneShotCount(size_t count) { mOneShotCount = count; }
        size_t OneShotCount() const { return mOneShotCount; }
        void Start();
        void Stop() { mPlaying = false; }
        void Reset();
        void Restart() { Reset(); }
        bool IsPlaying() const { return mPlaying; }
        bool IsFinished() const { return mFinished; }

        bool Emit(const glm::vec2 &position, const glm::vec2 &velocity,
                  float lifetime, float size, const glm::vec4 &color = glm::vec4(1.0f),
                  float rotation = 0.0f, float angularVelocity = 0.0f);
        bool Emit(const glm::vec2 &position, const glm::vec2 &velocity,
                  float lifetime, float size, const glm::vec4 &color,
                  float rotation, float angularVelocity,
                  const glm::vec4 &atlasBounds);
        bool Emit(const glm::vec2 &position, const ParticlePrefab &prefab);
        void Update(float deltaTime);
        // Unlit/debug path. Draws immediately through the low-level batcher.
        void Draw(BatchRenderer &batch) const;
        // Lit canvas path. Adds particles to the render queue so CanvasRenderer
        // applies point/directional lights and the existing shadow atlas.
        void Submit(RenderQueue &queue, int zIndex = 0, bool ySort = false,
                    BlendMode blendMode = BLEND_MIX) const;

        size_t ActiveCount() const { return mParticles.size(); }
        size_t Capacity() const { return mCapacity; }
        const Particle &Get(size_t index) const { return mParticles[index]; }

    private:
        Texture *mTexture;
        glm::vec2 mGravity;
        size_t mCapacity;
        ct::Vector<Particle> mParticles;
        ParticleMode mMode;
        ParticlePrefab mPrefab;
        glm::vec2 mEmitterPosition;
        float mEmissionRate;
        float mEmissionAccumulator;
        size_t mOneShotCount;
        bool mPlaying;
        bool mFinished;
    };

}
