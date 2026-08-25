#include "k2d/AudioEngine.h"

#include "k2d/FileSystem.h"

#include "audio/miniaudio.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace k2d
{
    namespace
    {
        float clamp(float value, float minimum, float maximum)
        {
            return value < minimum ? minimum : (value > maximum ? maximum : value);
        }
    }

    struct AudioEngine::Impl
    {
        struct Sound
        {
            SoundId id = 0;
            std::string path;
            std::vector<unsigned char> bytes;
            bool music = false;
        };

        struct Voice
        {
            VoiceId id = 0;
            SoundId sound = 0;
            bool loop = false;
            bool music = false;
            bool hasDecoder = false;
            ma_decoder decoder{};
            ma_sound player{};
        };

        ma_engine engine{};
        ma_sound_group sfxGroup{};
        ma_sound_group musicGroup{};
        bool ready = false;
        bool groupsReady = false;
        SoundId nextSound = 1;
        VoiceId nextVoice = 1;
        VoiceId musicVoice = 0;
        float masterVolume = 1.0f;
        float sfxVolume = 1.0f;
        float musicVolume = 1.0f;
        std::unordered_map<SoundId, Sound> sounds;
        std::vector<Voice *> voices;
    };

    namespace
    {
        AudioEngine::Impl::Voice *findVoice(AudioEngine::Impl *impl, AudioEngine::VoiceId id)
        {
            if (!impl || id <= 0)
                return nullptr;
            for (AudioEngine::Impl::Voice *voice : impl->voices)
                if (voice && voice->id == id)
                    return voice;
            return nullptr;
        }

        const AudioEngine::Impl::Voice *findVoice(const AudioEngine::Impl *impl, AudioEngine::VoiceId id)
        {
            if (!impl || id <= 0)
                return nullptr;
            for (const AudioEngine::Impl::Voice *voice : impl->voices)
                if (voice && voice->id == id)
                    return voice;
            return nullptr;
        }

        void destroyVoice(AudioEngine::Impl *impl, AudioEngine::Impl::Voice *voice)
        {
            if (!voice)
                return;
            if (impl && impl->musicVoice == voice->id)
                impl->musicVoice = 0;
            ma_sound_stop(&voice->player);
            ma_sound_uninit(&voice->player);
            if (voice->hasDecoder)
                ma_decoder_uninit(&voice->decoder);
            delete voice;
        }
    }

    AudioEngine::AudioEngine() : mImpl(new Impl()) {}

    AudioEngine::~AudioEngine()
    {
        Shutdown();
        delete mImpl;
        mImpl = nullptr;
    }

    bool AudioEngine::Init()
    {
        if (!mImpl)
            return false;
        if (mImpl->ready)
            return true;

        ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &mImpl->engine) != MA_SUCCESS)
            return false;
        if (ma_sound_group_init(&mImpl->engine, 0, nullptr, &mImpl->sfxGroup) != MA_SUCCESS)
        {
            ma_engine_uninit(&mImpl->engine);
            return false;
        }
        if (ma_sound_group_init(&mImpl->engine, 0, nullptr, &mImpl->musicGroup) != MA_SUCCESS)
        {
            ma_sound_group_uninit(&mImpl->sfxGroup);
            ma_engine_uninit(&mImpl->engine);
            return false;
        }

        mImpl->groupsReady = true;
        mImpl->ready = true;
        SetMasterVolume(mImpl->masterVolume);
        SetSfxVolume(mImpl->sfxVolume);
        SetMusicVolume(mImpl->musicVolume);
        return true;
    }

    void AudioEngine::Shutdown()
    {
        if (!mImpl)
            return;
        StopAll();
        Clear();
        if (!mImpl->ready)
            return;
        if (mImpl->groupsReady)
        {
            ma_sound_group_uninit(&mImpl->musicGroup);
            ma_sound_group_uninit(&mImpl->sfxGroup);
            mImpl->groupsReady = false;
        }
        ma_engine_uninit(&mImpl->engine);
        mImpl->ready = false;
    }

    bool AudioEngine::Ready() const { return mImpl && mImpl->ready; }

    AudioEngine::SoundId AudioEngine::LoadSound(const char *path)
    {
        if (!mImpl || !path || !path[0])
            return 0;
        ct::String resolved;
        if (!FileSystem::Instance().Resolve(path, resolved))
            return 0;
        const SoundId id = mImpl->nextSound++;
        mImpl->sounds[id] = {id, resolved.c_str(), {}, false};
        return id;
    }

    AudioEngine::SoundId AudioEngine::LoadMusic(const char *path)
    {
        const SoundId id = LoadSound(path);
        if (id)
            mImpl->sounds[id].music = true;
        return id;
    }

    AudioEngine::SoundId AudioEngine::LoadSoundMemory(const void *data, std::size_t size)
    {
        if (!mImpl || !data || size == 0)
            return 0;
        const SoundId id = mImpl->nextSound++;
        Impl::Sound sound;
        sound.id = id;
        sound.bytes.resize(size);
        std::memcpy(sound.bytes.data(), data, size);
        mImpl->sounds[id] = std::move(sound);
        return id;
    }

    AudioEngine::SoundId AudioEngine::LoadMusicMemory(const void *data, std::size_t size)
    {
        const SoundId id = LoadSoundMemory(data, size);
        if (id)
            mImpl->sounds[id].music = true;
        return id;
    }

    bool AudioEngine::Unload(SoundId sound)
    {
        if (!mImpl || sound <= 0)
            return false;
        for (auto it = mImpl->voices.begin(); it != mImpl->voices.end();)
        {
            if (*it && (*it)->sound == sound)
            {
                destroyVoice(mImpl, *it);
                it = mImpl->voices.erase(it);
            }
            else
                ++it;
        }
        return mImpl->sounds.erase(sound) != 0;
    }

    void AudioEngine::Clear()
    {
        if (!mImpl)
            return;
        StopAll();
        mImpl->sounds.clear();
        mImpl->nextSound = 1;
    }

    AudioEngine::VoiceId AudioEngine::Play(SoundId sound, float volume, float pitch, float pan)
    {
        if (!mImpl || !mImpl->ready)
            return 0;
        const auto found = mImpl->sounds.find(sound);
        if (found == mImpl->sounds.end() || found->second.music)
            return 0;

        Impl::Voice *voice = new Impl::Voice();
        voice->id = mImpl->nextVoice++;
        voice->sound = sound;
        ma_result result;
        if (!found->second.bytes.empty())
        {
            const ma_decoder_config config = ma_decoder_config_init_default();
            result = ma_decoder_init_memory(found->second.bytes.data(), found->second.bytes.size(),
                                            &config, &voice->decoder);
            if (result == MA_SUCCESS)
            {
                voice->hasDecoder = true;
                result = ma_sound_init_from_data_source(&mImpl->engine, &voice->decoder, 0,
                                                        &mImpl->sfxGroup, &voice->player);
            }
        }
        else
        {
            result = ma_sound_init_from_file(&mImpl->engine, found->second.path.c_str(), MA_SOUND_FLAG_DECODE,
                                             &mImpl->sfxGroup, nullptr, &voice->player);
        }
        if (result != MA_SUCCESS)
        {
            if (voice->hasDecoder)
                ma_decoder_uninit(&voice->decoder);
            delete voice;
            return 0;
        }
        ma_sound_set_volume(&voice->player, clamp(volume, 0.0f, 4.0f));
        ma_sound_set_pitch(&voice->player, clamp(pitch, 0.01f, 4.0f));
        ma_sound_set_pan(&voice->player, clamp(pan, -1.0f, 1.0f));
        if (ma_sound_start(&voice->player) != MA_SUCCESS)
        {
            ma_sound_uninit(&voice->player);
            if (voice->hasDecoder)
                ma_decoder_uninit(&voice->decoder);
            delete voice;
            return 0;
        }
        mImpl->voices.push_back(voice);
        return voice->id;
    }

    AudioEngine::VoiceId AudioEngine::PlayMusic(SoundId sound, bool loop, float volume)
    {
        if (!mImpl || !mImpl->ready)
            return 0;
        const auto found = mImpl->sounds.find(sound);
        if (found == mImpl->sounds.end() || !found->second.music)
            return 0;

        StopMusic();
        Impl::Voice *voice = new Impl::Voice();
        voice->id = mImpl->nextVoice++;
        voice->sound = sound;
        voice->loop = loop;
        voice->music = true;
        ma_result result;
        if (!found->second.bytes.empty())
        {
            const ma_decoder_config config = ma_decoder_config_init_default();
            result = ma_decoder_init_memory(found->second.bytes.data(), found->second.bytes.size(),
                                            &config, &voice->decoder);
            if (result == MA_SUCCESS)
            {
                voice->hasDecoder = true;
                result = ma_sound_init_from_data_source(&mImpl->engine, &voice->decoder, 0,
                                                        &mImpl->musicGroup, &voice->player);
            }
        }
        else
        {
            result = ma_sound_init_from_file(&mImpl->engine, found->second.path.c_str(), MA_SOUND_FLAG_STREAM,
                                             &mImpl->musicGroup, nullptr, &voice->player);
        }
        if (result != MA_SUCCESS)
        {
            if (voice->hasDecoder)
                ma_decoder_uninit(&voice->decoder);
            delete voice;
            return 0;
        }
        ma_sound_set_looping(&voice->player, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&voice->player, clamp(volume, 0.0f, 4.0f));
        if (ma_sound_start(&voice->player) != MA_SUCCESS)
        {
            ma_sound_uninit(&voice->player);
            if (voice->hasDecoder)
                ma_decoder_uninit(&voice->decoder);
            delete voice;
            return 0;
        }
        mImpl->voices.push_back(voice);
        mImpl->musicVoice = voice->id;
        return voice->id;
    }

    bool AudioEngine::Stop(VoiceId voice)
    {
        if (!mImpl)
            return false;
        for (auto it = mImpl->voices.begin(); it != mImpl->voices.end(); ++it)
        {
            if (*it && (*it)->id == voice)
            {
                destroyVoice(mImpl, *it);
                mImpl->voices.erase(it);
                return true;
            }
        }
        return false;
    }

    bool AudioEngine::Pause(VoiceId voice)
    {
        Impl::Voice *found = findVoice(mImpl, voice);
        return found && ma_sound_stop(&found->player) == MA_SUCCESS;
    }

    bool AudioEngine::Resume(VoiceId voice)
    {
        Impl::Voice *found = findVoice(mImpl, voice);
        return found && ma_sound_start(&found->player) == MA_SUCCESS;
    }

    bool AudioEngine::IsPlaying(VoiceId voice) const
    {
        const Impl::Voice *found = findVoice(mImpl, voice);
        return found && ma_sound_is_playing(&found->player) == MA_TRUE;
    }

    bool AudioEngine::SetVoiceVolume(VoiceId voice, float volume)
    {
        Impl::Voice *found = findVoice(mImpl, voice);
        if (!found)
            return false;
        ma_sound_set_volume(&found->player, clamp(volume, 0.0f, 4.0f));
        return true;
    }

    bool AudioEngine::SetVoicePitch(VoiceId voice, float pitch)
    {
        Impl::Voice *found = findVoice(mImpl, voice);
        if (!found)
            return false;
        ma_sound_set_pitch(&found->player, clamp(pitch, 0.01f, 4.0f));
        return true;
    }

    bool AudioEngine::SetVoicePan(VoiceId voice, float pan)
    {
        Impl::Voice *found = findVoice(mImpl, voice);
        if (!found)
            return false;
        ma_sound_set_pan(&found->player, clamp(pan, -1.0f, 1.0f));
        return true;
    }

    void AudioEngine::StopAll()
    {
        if (!mImpl)
            return;
        for (Impl::Voice *voice : mImpl->voices)
            destroyVoice(mImpl, voice);
        mImpl->voices.clear();
        mImpl->musicVoice = 0;
    }

    void AudioEngine::StopMusic()
    {
        if (mImpl && mImpl->musicVoice)
            Stop(mImpl->musicVoice);
    }

    void AudioEngine::SetMasterVolume(float volume)
    {
        if (!mImpl)
            return;
        mImpl->masterVolume = clamp(volume, 0.0f, 4.0f);
        if (mImpl->ready)
            ma_engine_set_volume(&mImpl->engine, mImpl->masterVolume);
    }

    void AudioEngine::SetSfxVolume(float volume)
    {
        if (!mImpl)
            return;
        mImpl->sfxVolume = clamp(volume, 0.0f, 4.0f);
        if (mImpl->groupsReady)
            ma_sound_group_set_volume(&mImpl->sfxGroup, mImpl->sfxVolume);
    }

    void AudioEngine::SetMusicVolume(float volume)
    {
        if (!mImpl)
            return;
        mImpl->musicVolume = clamp(volume, 0.0f, 4.0f);
        if (mImpl->groupsReady)
            ma_sound_group_set_volume(&mImpl->musicGroup, mImpl->musicVolume);
    }

    float AudioEngine::MasterVolume() const { return mImpl ? mImpl->masterVolume : 0.0f; }
    float AudioEngine::SfxVolume() const { return mImpl ? mImpl->sfxVolume : 0.0f; }
    float AudioEngine::MusicVolume() const { return mImpl ? mImpl->musicVolume : 0.0f; }

    void AudioEngine::Update()
    {
        if (!mImpl)
            return;
        for (auto it = mImpl->voices.begin(); it != mImpl->voices.end();)
        {
            Impl::Voice *voice = *it;
            if (!voice || (!voice->loop && ma_sound_at_end(&voice->player) == MA_TRUE))
            {
                destroyVoice(mImpl, voice);
                it = mImpl->voices.erase(it);
            }
            else
                ++it;
        }
    }

    AudioEngine &GetAudio()
    {
        static AudioEngine audio;
        return audio;
    }
}
