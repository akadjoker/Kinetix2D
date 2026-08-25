#pragma once

#include "k2d/AudioEngine.h"
#include "k2d/Component.h"

#include <string>

namespace k2d
{
class AudioPlayer final : public Component
{
  public:
    static const ComponentType Type = ComponentType::AudioPlayer;

    AudioPlayer();

    void setSource(const char* path);
    const char* source() const
    {
        return mSource.c_str();
    }
    void setMusic(bool music);
    bool music() const
    {
        return mMusic;
    }
    void setAutoplay(bool autoplay)
    {
        mAutoplay = autoplay;
    }
    bool autoplay() const
    {
        return mAutoplay;
    }
    void setLoop(bool loop)
    {
        mLoop = loop;
    }
    bool loop() const
    {
        return mLoop;
    }
    void setVolume(float volume);
    float volume() const
    {
        return mVolume;
    }
    void setPitch(float pitch);
    float pitch() const
    {
        return mPitch;
    }
    void setPan(float pan);
    float pan() const
    {
        return mPan;
    }
    void setSpatial(bool spatial);
    bool spatial() const
    {
        return mSpatial;
    }
    void setMinDistance(float distance);
    float minDistance() const
    {
        return mMinDistance;
    }
    void setMaxDistance(float distance);
    float maxDistance() const
    {
        return mMaxDistance;
    }
    void setRolloff(float rolloff);
    float rolloff() const
    {
        return mRolloff;
    }

    AudioEngine::VoiceId play();
    bool stop();
    bool pause();
    bool resume();
    bool playing() const;

  protected:
    void onStart() override;
    void onDisable() override;
    void onDestroy() override;

  private:
    void releaseSound();
    AudioEngine::SoundId load();

    std::string mSource;
    AudioEngine::SoundId mSound;
    AudioEngine::VoiceId mVoice;
    float mVolume;
    float mPitch;
    float mPan;
    float mMinDistance;
    float mMaxDistance;
    float mRolloff;
    bool mMusic;
    bool mAutoplay;
    bool mLoop;
    bool mSpatial;

  protected:
    void onUpdate(float deltaTime) override;
};
} // namespace k2d
