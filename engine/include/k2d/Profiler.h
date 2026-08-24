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

    class Profiler
    {
    public:
        static const uint32_t MaxSamples = 32;
        static const uint32_t MaxDepth = 16;
        static const double RefreshSeconds;

        static Profiler &Get();

        void beginFrame();
        void endFrame();
        bool begin(const char *name);
        void end();
        void addSample(const char *name, float milliseconds, uint32_t calls = 1);

        const ProfileSample *samples() const;
        uint32_t sampleCount() const;
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
