#include "k2d/AudioEngine.h"

#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/UserData.h"

#include "audio/miniaudio.h"
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace k2d
{
namespace
{
float clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}
} // namespace

struct AudioEngine::Impl
{
    struct Sound
    {
        SoundId id = 0;
        ct::String path;
        ct::Vector<unsigned char> bytes;
        bool music = false;
    };

    struct Voice
    {
        VoiceId id = 0;
        SoundId sound = 0;
        bool loop = false;
        bool music = false;
        bool hasDecoder = false;
        bool paused = false;
        bool removeWhenStopped = false;
        float volume = 1.0f;
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
    bool masterMuted = false;
    bool sfxMuted = false;
    bool musicMuted = false;
    std::unordered_map<SoundId, Sound> sounds;
    ct::Vector<Voice*> voices;
};

namespace
{
unsigned long long milliseconds(float seconds)
{
    return seconds > 0.0f ? static_cast<unsigned long long>(seconds * 1000.0f + 0.5f) : 0;
}

void applyMixer(AudioEngine::Impl* impl)
{
    if (!impl || !impl->ready)
        return;
    ma_engine_set_volume(&impl->engine, impl->masterMuted ? 0.0f : impl->masterVolume);
    if (impl->groupsReady)
    {
        ma_sound_group_set_volume(&impl->sfxGroup, impl->sfxMuted ? 0.0f : impl->sfxVolume);
        ma_sound_group_set_volume(&impl->musicGroup, impl->musicMuted ? 0.0f : impl->musicVolume);
    }
}

AudioEngine::Impl::Voice* findVoice(AudioEngine::Impl* impl, AudioEngine::VoiceId id)
{
    if (!impl || id <= 0)
        return nullptr;
    for (AudioEngine::Impl::Voice* voice : impl->voices)
        if (voice && voice->id == id)
            return voice;
    return nullptr;
}

const AudioEngine::Impl::Voice* findVoice(const AudioEngine::Impl* impl, AudioEngine::VoiceId id)
{
    if (!impl || id <= 0)
        return nullptr;
    for (const AudioEngine::Impl::Voice* voice : impl->voices)
        if (voice && voice->id == id)
            return voice;
    return nullptr;
}

void destroyVoice(AudioEngine::Impl* impl, AudioEngine::Impl::Voice* voice)
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

AudioEngine::Impl::Voice* createMusicVoice(AudioEngine::Impl* impl, AudioEngine::SoundId soundId, bool loop,
                                           float volume)
{
    if (!impl || !impl->ready)
        return nullptr;
    const auto found = impl->sounds.find(soundId);
    if (found == impl->sounds.end() || !found->second.music)
        return nullptr;

    AudioEngine::Impl::Voice* voice = new AudioEngine::Impl::Voice();
    voice->id = impl->nextVoice++;
    voice->sound = soundId;
    voice->loop = loop;
    voice->music = true;
    voice->volume = clamp(volume, 0.0f, 4.0f);
    ma_result result = MA_ERROR;
    if (!found->second.bytes.empty())
    {
        const ma_decoder_config config = ma_decoder_config_init_default();
        result =
            ma_decoder_init_memory(found->second.bytes.data(), found->second.bytes.size(), &config, &voice->decoder);
        if (result == MA_SUCCESS)
        {
            voice->hasDecoder = true;
            result =
                ma_sound_init_from_data_source(&impl->engine, &voice->decoder, 0, &impl->musicGroup, &voice->player);
        }
    }
    else
    {
        result = ma_sound_init_from_file(&impl->engine, found->second.path.c_str(), MA_SOUND_FLAG_STREAM,
                                         &impl->musicGroup, nullptr, &voice->player);
    }
    if (result != MA_SUCCESS)
    {
        if (voice->hasDecoder)
            ma_decoder_uninit(&voice->decoder);
        delete voice;
        return nullptr;
    }
    ma_sound_set_looping(&voice->player, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(&voice->player, MA_FALSE);
    ma_sound_set_volume(&voice->player, voice->volume);
    if (ma_sound_start(&voice->player) != MA_SUCCESS)
    {
        ma_sound_uninit(&voice->player);
        if (voice->hasDecoder)
            ma_decoder_uninit(&voice->decoder);
        delete voice;
        return nullptr;
    }
    impl->voices.push_back(voice);
    return voice;
}
} // namespace

AudioEngine::AudioEngine() : mImpl(new Impl())
{
}

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
    applyMixer(mImpl);
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

bool AudioEngine::Ready() const
{
    return mImpl && mImpl->ready;
}

AudioEngine::SoundId AudioEngine::LoadSound(const char* path)
{
    if (!mImpl || !path || !path[0])
        return 0;
    FileBuffer file;
    if (!FileSystem::Instance().LoadFile(path, file))
        return 0;
    const SoundId id = mImpl->nextSound++;
    Impl::Sound sound;
    sound.id = id;
    sound.bytes.resize(file.Size());
    if (file.Size() > 0)
        std::memcpy(sound.bytes.data(), file.Data(), file.Size());
    mImpl->sounds[id] = std::move(sound);
    return id;
}

AudioEngine::SoundId AudioEngine::LoadMusic(const char* path)
{
    const SoundId id = LoadSound(path);
    if (id)
        mImpl->sounds[id].music = true;
    return id;
}

AudioEngine::SoundId AudioEngine::LoadSoundMemory(const void* data, std::size_t size)
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

AudioEngine::SoundId AudioEngine::LoadMusicMemory(const void* data, std::size_t size)
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

    Impl::Voice* voice = new Impl::Voice();
    voice->id = mImpl->nextVoice++;
    voice->sound = sound;
    ma_result result;
    if (!found->second.bytes.empty())
    {
        const ma_decoder_config config = ma_decoder_config_init_default();
        result =
            ma_decoder_init_memory(found->second.bytes.data(), found->second.bytes.size(), &config, &voice->decoder);
        if (result == MA_SUCCESS)
        {
            voice->hasDecoder = true;
            result =
                ma_sound_init_from_data_source(&mImpl->engine, &voice->decoder, 0, &mImpl->sfxGroup, &voice->player);
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
    voice->volume = clamp(volume, 0.0f, 4.0f);
    ma_sound_set_pitch(&voice->player, clamp(pitch, 0.01f, 4.0f));
    ma_sound_set_spatialization_enabled(&voice->player, MA_FALSE);
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
    Impl::Voice* voice = createMusicVoice(mImpl, sound, loop, volume);
    if (!voice)
        return 0;
    StopMusic();
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
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found || ma_sound_stop(&found->player) != MA_SUCCESS)
        return false;
    found->paused = true;
    return true;
}

bool AudioEngine::Resume(VoiceId voice)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found || ma_sound_start(&found->player) != MA_SUCCESS)
        return false;
    found->paused = false;
    return true;
}

bool AudioEngine::IsPlaying(VoiceId voice) const
{
    const Impl::Voice* found = findVoice(mImpl, voice);
    return found && ma_sound_is_playing(&found->player) == MA_TRUE;
}

bool AudioEngine::SetVoiceVolume(VoiceId voice, float volume)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    ma_sound_set_volume(&found->player, clamp(volume, 0.0f, 4.0f));
    found->volume = clamp(volume, 0.0f, 4.0f);
    ma_sound_set_fade_in_milliseconds(&found->player, 1.0f, 1.0f, 0);
    return true;
}

bool AudioEngine::SetVoicePitch(VoiceId voice, float pitch)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    ma_sound_set_pitch(&found->player, clamp(pitch, 0.01f, 4.0f));
    return true;
}

bool AudioEngine::SetVoicePan(VoiceId voice, float pan)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    ma_sound_set_pan(&found->player, clamp(pan, -1.0f, 1.0f));
    return true;
}

bool AudioEngine::FadeIn(VoiceId voice, float seconds)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    ma_sound_set_fade_in_milliseconds(&found->player, 0.0f, 1.0f, milliseconds(seconds));
    return true;
}

bool AudioEngine::FadeOut(VoiceId voice, float seconds, bool stopWhenDone)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    const unsigned long long duration = milliseconds(seconds);
    if (stopWhenDone)
    {
        if (ma_sound_stop_with_fade_in_milliseconds(&found->player, duration) != MA_SUCCESS)
            return false;
        found->removeWhenStopped = true;
    }
    else
    {
        ma_sound_set_fade_in_milliseconds(&found->player, -1.0f, 0.0f, duration);
    }
    return true;
}

AudioEngine::VoiceId AudioEngine::CrossfadeMusic(SoundId sound, bool loop, float volume, float seconds)
{
    Impl::Voice* next = createMusicVoice(mImpl, sound, loop, volume);
    if (!next)
        return 0;
    const VoiceId previous = mImpl->musicVoice;
    mImpl->musicVoice = next->id;
    FadeIn(next->id, seconds);
    if (previous)
        FadeOut(previous, seconds, true);
    return next->id;
}

bool AudioEngine::SetListenerPosition(const Math::Vec2& position)
{
    if (!mImpl || !mImpl->ready)
        return false;
    ma_engine_listener_set_position(&mImpl->engine, 0, position.x, position.y, 0.0f);
    return true;
}

bool AudioEngine::SetVoicePosition(VoiceId voice, const Math::Vec2& position)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    ma_sound_set_position(&found->player, position.x, position.y, 0.0f);
    return true;
}

bool AudioEngine::SetVoiceSpatial(VoiceId voice, bool enabled, float minDistance, float maxDistance, float rolloff)
{
    Impl::Voice* found = findVoice(mImpl, voice);
    if (!found)
        return false;
    minDistance = clamp(minDistance, 0.0f, 1000000.0f);
    maxDistance = clamp(maxDistance, minDistance, 1000000.0f);
    ma_sound_set_spatialization_enabled(&found->player, enabled ? MA_TRUE : MA_FALSE);
    ma_sound_set_positioning(&found->player, ma_positioning_absolute);
    ma_sound_set_attenuation_model(&found->player, ma_attenuation_model_inverse);
    ma_sound_set_min_distance(&found->player, minDistance);
    ma_sound_set_max_distance(&found->player, maxDistance);
    ma_sound_set_rolloff(&found->player, clamp(rolloff, 0.0f, 100.0f));
    return true;
}

AudioEngine::VoiceId AudioEngine::PlayAt(SoundId sound, const Math::Vec2& position, float volume, float pitch,
                                         float minDistance, float maxDistance, float rolloff)
{
    const VoiceId voice = Play(sound, volume, pitch);
    if (!voice)
        return 0;
    SetVoicePosition(voice, position);
    SetVoiceSpatial(voice, true, minDistance, maxDistance, rolloff);
    return voice;
}

void AudioEngine::StopAll()
{
    if (!mImpl)
        return;
    for (Impl::Voice* voice : mImpl->voices)
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
    applyMixer(mImpl);
}

void AudioEngine::SetSfxVolume(float volume)
{
    if (!mImpl)
        return;
    mImpl->sfxVolume = clamp(volume, 0.0f, 4.0f);
    applyMixer(mImpl);
}

void AudioEngine::SetMusicVolume(float volume)
{
    if (!mImpl)
        return;
    mImpl->musicVolume = clamp(volume, 0.0f, 4.0f);
    applyMixer(mImpl);
}

float AudioEngine::MasterVolume() const
{
    return mImpl ? mImpl->masterVolume : 0.0f;
}
float AudioEngine::SfxVolume() const
{
    return mImpl ? mImpl->sfxVolume : 0.0f;
}
float AudioEngine::MusicVolume() const
{
    return mImpl ? mImpl->musicVolume : 0.0f;
}

void AudioEngine::SetMasterMuted(bool muted)
{
    if (!mImpl)
        return;
    mImpl->masterMuted = muted;
    applyMixer(mImpl);
}

void AudioEngine::SetSfxMuted(bool muted)
{
    if (!mImpl)
        return;
    mImpl->sfxMuted = muted;
    applyMixer(mImpl);
}

void AudioEngine::SetMusicMuted(bool muted)
{
    if (!mImpl)
        return;
    mImpl->musicMuted = muted;
    applyMixer(mImpl);
}

bool AudioEngine::MasterMuted() const
{
    return mImpl && mImpl->masterMuted;
}
bool AudioEngine::SfxMuted() const
{
    return mImpl && mImpl->sfxMuted;
}
bool AudioEngine::MusicMuted() const
{
    return mImpl && mImpl->musicMuted;
}

void AudioEngine::LoadSettings(const UserData& data)
{
    SetMasterVolume(data.getFloat("audio.masterVolume", 1.0f));
    SetSfxVolume(data.getFloat("audio.sfxVolume", 1.0f));
    SetMusicVolume(data.getFloat("audio.musicVolume", 1.0f));
    SetMasterMuted(data.getBool("audio.masterMuted", false));
    SetSfxMuted(data.getBool("audio.sfxMuted", false));
    SetMusicMuted(data.getBool("audio.musicMuted", false));
}

void AudioEngine::SaveSettings(UserData& data) const
{
    data.setFloat("audio.masterVolume", MasterVolume());
    data.setFloat("audio.sfxVolume", SfxVolume());
    data.setFloat("audio.musicVolume", MusicVolume());
    data.setBool("audio.masterMuted", MasterMuted());
    data.setBool("audio.sfxMuted", SfxMuted());
    data.setBool("audio.musicMuted", MusicMuted());
}

void AudioEngine::Update()
{
    if (!mImpl)
        return;
    for (auto it = mImpl->voices.begin(); it != mImpl->voices.end();)
    {
        Impl::Voice* voice = *it;
        if (!voice || (!voice->loop && ma_sound_at_end(&voice->player) == MA_TRUE) ||
            (voice->removeWhenStopped && !voice->paused && ma_sound_is_playing(&voice->player) != MA_TRUE))
        {
            destroyVoice(mImpl, voice);
            it = mImpl->voices.erase(it);
        }
        else
            ++it;
    }
}

AudioEngine& GetAudio()
{
    static AudioEngine audio;
    return audio;
}
} // namespace k2d
