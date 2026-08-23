#include "k2d/Profiler.h"

#include <SDL.h>
#include <cstring>

namespace k2d
{

    const double Profiler::RefreshSeconds = 0.25;

    Profiler::Profiler()
        : mFrameStart(0), mFrequency(1), mLastRefresh(0),
          mFrameMilliseconds(0.0f), mDisplayFrameMilliseconds(0.0f),
          mSampleCount(0), mDepth(0), mEnabled(true)
    {
        std::memset(mSamples, 0, sizeof(mSamples));
        std::memset(mStack, 0, sizeof(mStack));
    }

    Profiler &Profiler::Get()
    {
        static Profiler profiler;
        return profiler;
    }

    void Profiler::beginFrame()
    {
        if (!mEnabled)
            return;

        if (mFrequency == 1)
            mFrequency = SDL_GetPerformanceFrequency();
        for (uint32_t i = 0; i < mSampleCount; ++i)
            mSamples[i].milliseconds = 0.0f;
        mDepth = 0;
        mFrameStart = SDL_GetPerformanceCounter();
    }

    void Profiler::endFrame()
    {
        if (!mEnabled)
            return;

        const uint64_t now = SDL_GetPerformanceCounter();
        mFrameMilliseconds = (float)((now - mFrameStart) * 1000.0 / mFrequency);
        const uint32_t frameSample = findOrCreate("Frame");
        if (frameSample < MaxSamples)
            mSamples[frameSample].milliseconds = mFrameMilliseconds;

        const bool refresh = (now - mLastRefresh) > (uint64_t)(RefreshSeconds * mFrequency);
        if (refresh)
        {
            mLastRefresh = now;
            mDisplayFrameMilliseconds = mFrameMilliseconds;
        }

        for (uint32_t i = 0; i < mSampleCount; ++i)
        {
            ProfileSample &sample = mSamples[i];
            if (refresh)
                sample.display = sample.milliseconds;
            sample.history[sample.historyCursor] = sample.milliseconds;
            sample.historyCursor = (sample.historyCursor + 1) % ProfileSample::HistorySize;
            if (sample.historyCount < ProfileSample::HistorySize)
                ++sample.historyCount;

            float total = 0.0f;
            sample.maximum = 0.0f;
            for (uint32_t j = 0; j < sample.historyCount; ++j)
            {
                total += sample.history[j];
                if (sample.history[j] > sample.maximum)
                    sample.maximum = sample.history[j];
            }
            sample.average = sample.historyCount ? total / sample.historyCount : 0.0f;
        }
    }

    uint32_t Profiler::findOrCreate(const char *name)
    {
        for (uint32_t i = 0; i < mSampleCount; ++i)
        {
            if (std::strcmp(mSamples[i].name, name) == 0)
                return i;
        }
        if (mSampleCount >= MaxSamples)
            return MaxSamples;

        std::strncpy(mSamples[mSampleCount].name, name, sizeof(mSamples[mSampleCount].name) - 1);
        mSamples[mSampleCount].name[sizeof(mSamples[mSampleCount].name) - 1] = '\0';
        return mSampleCount++;
    }

    bool Profiler::begin(const char *name)
    {
        if (!mEnabled)
            return false;
        if (!name)
            return false;
        if (mDepth >= MaxDepth)
            return false;
        if (mFrequency == 1)
            mFrequency = SDL_GetPerformanceFrequency();
        const uint32_t sample = findOrCreate(name);
        if (sample >= MaxSamples)
            return false;
        mStack[mDepth].sample = sample;
        mStack[mDepth].counter = SDL_GetPerformanceCounter();
        ++mDepth;
        return true;
    }

    void Profiler::end()
    {
        if (mDepth == 0)
            return;
        const uint64_t now = SDL_GetPerformanceCounter();
        const ActiveScope scope = mStack[--mDepth];
        mSamples[scope.sample].milliseconds += (float)((now - scope.counter) * 1000.0 / mFrequency);
    }

    void Profiler::addSample(const char *name, float milliseconds)
    {
        if (!mEnabled)
            return;
        if (!name)
            return;
        const uint32_t sample = findOrCreate(name);
        if (sample < MaxSamples)
            mSamples[sample].milliseconds += milliseconds;
    }

    const ProfileSample *Profiler::samples() const
    {
        return mSamples;
    }

    uint32_t Profiler::sampleCount() const
    {
        return mSampleCount;
    }

    float Profiler::frameMilliseconds() const
    {
        return mDisplayFrameMilliseconds;
    }

    ProfileScope::ProfileScope(const char *name) : mActive(Profiler::Get().begin(name))
    {
    }

    ProfileScope::~ProfileScope()
    {
        if (mActive)
            Profiler::Get().end();
    }

}