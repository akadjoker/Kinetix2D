#include "k2d/Profiler.h"

#include <platform.h>
#include <cstring>

namespace k2d
{

    const double Profiler::RefreshSeconds = 0.25;

    Profiler::Profiler()
        : mCounterCount(0), mFrameStart(0), mFrequency(1), mLastRefresh(0),
          mFrameMilliseconds(0.0f), mDisplayFrameMilliseconds(0.0f),
          mSampleCount(0), mDepth(0), mEnabled(true)
    {
        std::memset(mSamples, 0, sizeof(mSamples));
        std::memset(mCounters, 0, sizeof(mCounters));
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
            mFrequency = 1000000000ull;
        for (uint32_t i = 0; i < mSampleCount; ++i)
        {
            mSamples[i].milliseconds = 0.0f;
            mSamples[i].calls = 0;
        }
        for (uint32_t i = 0; i < mCounterCount; ++i)
            mCounters[i].value = 0;
        mDepth = 0;
        mFrameStart = time_nanos();
    }

    void Profiler::endFrame()
    {
        if (!mEnabled)
            return;

        const uint64_t now = time_nanos();
        mFrameMilliseconds = (float)((now - mFrameStart) * 1000.0 / mFrequency);
        const uint32_t frameSample = findOrCreate("Frame");
        if (frameSample < MaxSamples)
        {
            mSamples[frameSample].milliseconds = mFrameMilliseconds;
            mSamples[frameSample].calls = 1;
        }

        const bool refresh = (now - mLastRefresh) > (uint64_t)(RefreshSeconds * mFrequency);
        if (refresh)
        {
            mLastRefresh = now;
            mDisplayFrameMilliseconds = mFrameMilliseconds;
            for (uint32_t i = 0; i < mCounterCount; ++i)
                mCounters[i].display = mCounters[i].value;
        }

        for (uint32_t i = 0; i < mSampleCount; ++i)
        {
            ProfileSample &sample = mSamples[i];
            if (refresh)
            {
                sample.display = sample.milliseconds;
                sample.displayCalls = sample.calls;
            }
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
            mFrequency = 1000000000ull;
        const uint32_t sample = findOrCreate(name);
        if (sample >= MaxSamples)
            return false;
        mStack[mDepth].sample = sample;
        mStack[mDepth].counter = time_nanos();
        ++mDepth;
        return true;
    }

    void Profiler::end()
    {
        if (mDepth == 0)
            return;
        const uint64_t now = time_nanos();
        const ActiveScope scope = mStack[--mDepth];
        mSamples[scope.sample].milliseconds += (float)((now - scope.counter) * 1000.0 / mFrequency);
        ++mSamples[scope.sample].calls;
    }

    void Profiler::addSample(const char *name, float milliseconds, uint32_t calls)
    {
        if (!mEnabled)
            return;
        if (!name)
            return;
        const uint32_t sample = findOrCreate(name);
        if (sample < MaxSamples)
        {
            mSamples[sample].milliseconds += milliseconds;
            mSamples[sample].calls += calls;
        }
    }

    void Profiler::addCounter(const char *name, uint64_t value)
    {
        if (!mEnabled || !name)
            return;
        for (uint32_t i = 0; i < mCounterCount; ++i)
            if (std::strcmp(mCounters[i].name, name) == 0)
            {
                mCounters[i].value += value;
                return;
            }
        if (mCounterCount >= MaxCounters)
            return;
        ProfileCounter &counter = mCounters[mCounterCount++];
        std::strncpy(counter.name, name, sizeof(counter.name) - 1);
        counter.name[sizeof(counter.name) - 1] = '\0';
        counter.value = value;
        counter.display = 0;
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
