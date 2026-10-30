#pragma once

#include <cstdint>

namespace k2d
{

    struct ProfileSample
    {
        static const uint32_t HistorySize = 120;

        char name[48];
        float milliseconds;
        float display;
        float average;
        float maximum;
        float history[HistorySize];
        uint32_t historyCount;
        uint32_t historyCursor;
        uint32_t calls;
        uint32_t displayCalls;
    };

    // A count the frame adds up (draw calls, objects culled...), shown as it
    // was at the last refresh.
    struct ProfileCounter
    {
        char name[48];
        uint64_t value;
        uint64_t display;
    };

    class Profiler
    {
    public:
        static const uint32_t MaxSamples = 32;
        static const uint32_t MaxCounters = 16;
        static const uint32_t MaxDepth = 16;
        static const double RefreshSeconds;

        static Profiler &Get();

        void beginFrame();
        void endFrame();
        bool begin(const char *name);
        void end();
        void addSample(const char *name, float milliseconds, uint32_t calls = 1);
        void addCounter(const char *name, uint64_t value);

        const ProfileSample *samples() const;
        uint32_t sampleCount() const;
        const ProfileCounter *counters() const { return mCounters; }
        uint32_t counterCount() const { return mCounterCount; }
        float frameMilliseconds() const;

        void SetEnabled(bool enabled) { mEnabled = enabled; }
        bool Enabled() const { return mEnabled; }

    private:
        Profiler();

        struct ActiveScope
        {
            uint32_t sample;
            uint64_t counter;
        };

        uint32_t findOrCreate(const char *name);

        ProfileSample mSamples[MaxSamples];
        ProfileCounter mCounters[MaxCounters];
        uint32_t mCounterCount;
        ActiveScope mStack[MaxDepth];
        uint64_t mFrameStart;
        uint64_t mFrequency;
        uint64_t mLastRefresh;
        float mFrameMilliseconds;
        float mDisplayFrameMilliseconds;
        uint32_t mSampleCount;
        uint32_t mDepth;
        bool mEnabled;
    };

    struct ProfileScope
    {
        explicit ProfileScope(const char *name);
        ~ProfileScope();

        ProfileScope(const ProfileScope &) = delete;
        ProfileScope &operator=(const ProfileScope &) = delete;

        bool mActive;
    };

}
