#pragma once

#include "k2d/Matrix2D.h"

#include <cstddef>

namespace k2d
{
class UserData;

// Small, engine-owned wrapper around miniaudio. Sound IDs describe loaded
// files; voice IDs describe individual playbacks and can overlap.
class AudioEngine
{
  public:
    struct Impl;
    using SoundId = int;
    using VoiceId = int;

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool Init();
    void Shutdown();
    bool Ready() const;

    SoundId LoadSound(const char* path);
    SoundId LoadMusic(const char* path);
    // The engine copies the encoded bytes, so callers may release their
    // buffer immediately after this returns.
    SoundId LoadSoundMemory(const void* data, std::size_t size);
    SoundId LoadMusicMemory(const void* data, std::size_t size);
    bool Unload(SoundId sound);
    void Clear();

    VoiceId Play(SoundId sound, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);
    VoiceId PlayMusic(SoundId sound, bool loop = true, float volume = 1.0f);
    bool Stop(VoiceId voice);
    bool Pause(VoiceId voice);
    bool Resume(VoiceId voice);
    bool IsPlaying(VoiceId voice) const;
    bool SetVoiceVolume(VoiceId voice, float volume);
    bool SetVoicePitch(VoiceId voice, float pitch);
    bool SetVoicePan(VoiceId voice, float pan);
    bool FadeIn(VoiceId voice, float seconds);
    bool FadeOut(VoiceId voice, float seconds, bool stopWhenDone = false);
    VoiceId CrossfadeMusic(SoundId sound, bool loop = true, float volume = 1.0f, float seconds = 1.0f);

    // 2D world audio. The listener is usually the active Camera2D. A
    // spatial voice follows its supplied position and attenuates between
    // minDistance and maxDistance in world units.
    bool SetListenerPosition(const Math::Vec2& position);
    bool SetVoicePosition(VoiceId voice, const Math::Vec2& position);
    bool SetVoiceSpatial(VoiceId voice, bool enabled, float minDistance = 64.0f, float maxDistance = 1024.0f,
                         float rolloff = 1.0f);
    VoiceId PlayAt(SoundId sound, const Math::Vec2& position, float volume = 1.0f, float pitch = 1.0f,
                   float minDistance = 64.0f, float maxDistance = 1024.0f, float rolloff = 1.0f);
    void StopAll();
    void StopMusic();

    void SetMasterVolume(float volume);
    void SetSfxVolume(float volume);
    void SetMusicVolume(float volume);
    float MasterVolume() const;
    float SfxVolume() const;
    float MusicVolume() const;
    void SetMasterMuted(bool muted);
    void SetSfxMuted(bool muted);
    void SetMusicMuted(bool muted);
    bool MasterMuted() const;
    bool SfxMuted() const;
    bool MusicMuted() const;

    // Settings are explicit: the host owns its UserData lifecycle while
    // AudioEngine owns the schema and keys. Call after UserData::load(),
    // then before UserData::save().
    void LoadSettings(const UserData& data);
    void SaveSettings(UserData& data) const;

    // Reclaims completed non-looping voices. Call once per frame.
    void Update();

    // Preferred camelCase API. PascalCase remains consistent with the
    // rest of the public engine API.
    bool init()
    {
        return Init();
    }
    void shutdown()
    {
        Shutdown();
    }
    bool ready() const
    {
        return Ready();
    }
    SoundId loadSound(const char* path)
    {
        return LoadSound(path);
    }
    SoundId loadMusic(const char* path)
    {
        return LoadMusic(path);
    }
    SoundId loadSoundMemory(const void* data, std::size_t size)
    {
        return LoadSoundMemory(data, size);
    }
    SoundId loadMusicMemory(const void* data, std::size_t size)
    {
        return LoadMusicMemory(data, size);
    }
    bool unload(SoundId sound)
    {
        return Unload(sound);
    }
    VoiceId play(SoundId sound, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f)
    {
        return Play(sound, volume, pitch, pan);
    }
    VoiceId playMusic(SoundId sound, bool loop = true, float volume = 1.0f)
    {
        return PlayMusic(sound, loop, volume);
    }
    bool stop(VoiceId voice)
    {
        return Stop(voice);
    }
    bool pause(VoiceId voice)
    {
        return Pause(voice);
    }
    bool resume(VoiceId voice)
    {
        return Resume(voice);
    }
    bool isPlaying(VoiceId voice) const
    {
        return IsPlaying(voice);
    }
    bool setVoiceVolume(VoiceId voice, float volume)
    {
        return SetVoiceVolume(voice, volume);
    }
    bool setVoicePitch(VoiceId voice, float pitch)
    {
        return SetVoicePitch(voice, pitch);
    }
    bool setVoicePan(VoiceId voice, float pan)
    {
        return SetVoicePan(voice, pan);
    }
    bool fadeIn(VoiceId voice, float seconds)
    {
        return FadeIn(voice, seconds);
    }
    bool fadeOut(VoiceId voice, float seconds, bool stopWhenDone = false)
    {
        return FadeOut(voice, seconds, stopWhenDone);
    }
    VoiceId crossfadeMusic(SoundId sound, bool loop = true, float volume = 1.0f, float seconds = 1.0f)
    {
        return CrossfadeMusic(sound, loop, volume, seconds);
    }
    bool setListenerPosition(const Math::Vec2& position)
    {
        return SetListenerPosition(position);
    }
    bool setVoicePosition(VoiceId voice, const Math::Vec2& position)
    {
        return SetVoicePosition(voice, position);
    }
    bool setVoiceSpatial(VoiceId voice, bool enabled, float minDistance = 64.0f, float maxDistance = 1024.0f,
                         float rolloff = 1.0f)
    {
        return SetVoiceSpatial(voice, enabled, minDistance, maxDistance, rolloff);
    }
    VoiceId playAt(SoundId sound, const Math::Vec2& position, float volume = 1.0f, float pitch = 1.0f,
                   float minDistance = 64.0f, float maxDistance = 1024.0f, float rolloff = 1.0f)
    {
        return PlayAt(sound, position, volume, pitch, minDistance, maxDistance, rolloff);
    }
    void stopAll()
    {
        StopAll();
    }
    void stopMusic()
    {
        StopMusic();
    }
    void setMasterVolume(float volume)
    {
        SetMasterVolume(volume);
    }
    void setSfxVolume(float volume)
    {
        SetSfxVolume(volume);
    }
    void setMusicVolume(float volume)
    {
        SetMusicVolume(volume);
    }
    void setMasterMuted(bool muted)
    {
        SetMasterMuted(muted);
    }
    void setSfxMuted(bool muted)
    {
        SetSfxMuted(muted);
    }
    void setMusicMuted(bool muted)
    {
        SetMusicMuted(muted);
    }
    bool masterMuted() const
    {
        return MasterMuted();
    }
    bool sfxMuted() const
    {
        return SfxMuted();
    }
    bool musicMuted() const
    {
        return MusicMuted();
    }
    void loadSettings(const UserData& data)
    {
        LoadSettings(data);
    }
    void saveSettings(UserData& data) const
    {
        SaveSettings(data);
    }
    void update()
    {
        Update();
    }

  private:
    Impl* mImpl;
};

AudioEngine& GetAudio();
} // namespace k2d
