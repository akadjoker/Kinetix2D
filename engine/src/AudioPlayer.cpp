#include "k2d/AudioPlayer.h"

namespace k2d
{
namespace
{
float clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}
}

AudioPlayer::AudioPlayer()
    : Component(Type), mSource(), mSound(0), mVoice(0), mVolume(1.0f), mPitch(1.0f), mPan(0.0f),
      mMusic(false), mAutoplay(false), mLoop(false)
{
}

void AudioPlayer::setSource(const char *path)
{
    const std::string source = path ? path : "";
    if (mSource == source)
        return;
    releaseSound();
    mSource = source;
}

void AudioPlayer::setMusic(bool music)
{
    if (mMusic == music)
        return;
    releaseSound();
    mMusic = music;
}

void AudioPlayer::setVolume(float volume)
{
    mVolume = clamp(volume, 0.0f, 4.0f);
    if (mVoice)
        GetAudio().SetVoiceVolume(mVoice, mVolume);
}

void AudioPlayer::setPitch(float pitch)
{
    mPitch = clamp(pitch, 0.01f, 4.0f);
    if (mVoice)
        GetAudio().SetVoicePitch(mVoice, mPitch);
}

void AudioPlayer::setPan(float pan)
{
    mPan = clamp(pan, -1.0f, 1.0f);
    if (mVoice && !mMusic)
        GetAudio().SetVoicePan(mVoice, mPan);
}

AudioEngine::SoundId AudioPlayer::load()
{
    if (mSound || mSource.empty())
        return mSound;
    mSound = mMusic ? GetAudio().LoadMusic(mSource.c_str()) : GetAudio().LoadSound(mSource.c_str());
    return mSound;
}

AudioEngine::VoiceId AudioPlayer::play()
{
    const AudioEngine::SoundId sound = load();
    if (!sound)
        return 0;
    mVoice = mMusic ? GetAudio().PlayMusic(sound, mLoop, mVolume) : GetAudio().Play(sound, mVolume, mPitch, mPan);
    return mVoice;
}

bool AudioPlayer::stop()
{
    const bool stopped = mVoice && GetAudio().Stop(mVoice);
    mVoice = 0;
    return stopped;
}

bool AudioPlayer::pause()
{
    return mVoice && GetAudio().Pause(mVoice);
}

bool AudioPlayer::resume()
{
    return mVoice && GetAudio().Resume(mVoice);
}

bool AudioPlayer::playing() const
{
    return mVoice && GetAudio().IsPlaying(mVoice);
}

void AudioPlayer::releaseSound()
{
    stop();
    if (mSound)
        GetAudio().Unload(mSound);
    mSound = 0;
}

void AudioPlayer::onStart()
{
    if (mAutoplay)
        play();
}

void AudioPlayer::onDisable()
{
    stop();
}

void AudioPlayer::onDestroy()
{
    releaseSound();
}
}
