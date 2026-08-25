#pragma once

#include <cstddef>

namespace k2d
{
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

        AudioEngine(const AudioEngine &) = delete;
        AudioEngine &operator=(const AudioEngine &) = delete;

        bool Init();
        void Shutdown();
        bool Ready() const;

        SoundId LoadSound(const char *path);
        SoundId LoadMusic(const char *path);
        // The engine copies the encoded bytes, so callers may release their
        // buffer immediately after this returns.
        SoundId LoadSoundMemory(const void *data, std::size_t size);
        SoundId LoadMusicMemory(const void *data, std::size_t size);
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
        void StopAll();
        void StopMusic();

        void SetMasterVolume(float volume);
        void SetSfxVolume(float volume);
        void SetMusicVolume(float volume);
        float MasterVolume() const;
        float SfxVolume() const;
        float MusicVolume() const;

        // Reclaims completed non-looping voices. Call once per frame.
        void Update();

        // Preferred camelCase API. PascalCase remains consistent with the
        // rest of the public engine API.
        bool init() { return Init(); }
        void shutdown() { Shutdown(); }
        bool ready() const { return Ready(); }
        SoundId loadSound(const char *path) { return LoadSound(path); }
        SoundId loadMusic(const char *path) { return LoadMusic(path); }
        SoundId loadSoundMemory(const void *data, std::size_t size) { return LoadSoundMemory(data, size); }
        SoundId loadMusicMemory(const void *data, std::size_t size) { return LoadMusicMemory(data, size); }
        bool unload(SoundId sound) { return Unload(sound); }
        VoiceId play(SoundId sound, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f)
        { return Play(sound, volume, pitch, pan); }
        VoiceId playMusic(SoundId sound, bool loop = true, float volume = 1.0f)
        { return PlayMusic(sound, loop, volume); }
        bool stop(VoiceId voice) { return Stop(voice); }
        bool pause(VoiceId voice) { return Pause(voice); }
        bool resume(VoiceId voice) { return Resume(voice); }
        bool isPlaying(VoiceId voice) const { return IsPlaying(voice); }
        bool setVoiceVolume(VoiceId voice, float volume) { return SetVoiceVolume(voice, volume); }
        bool setVoicePitch(VoiceId voice, float pitch) { return SetVoicePitch(voice, pitch); }
        bool setVoicePan(VoiceId voice, float pan) { return SetVoicePan(voice, pan); }
        void stopAll() { StopAll(); }
        void stopMusic() { StopMusic(); }
        void setMasterVolume(float volume) { SetMasterVolume(volume); }
        void setSfxVolume(float volume) { SetSfxVolume(volume); }
        void setMusicVolume(float volume) { SetMusicVolume(volume); }
        void update() { Update(); }

    private:
        Impl *mImpl;
    };

    AudioEngine &GetAudio();
}
