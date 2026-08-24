#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstddef>

namespace zen
{
class VM;
}

namespace k2d
{

    class GameObject;

    class ZenRuntime
    {
    public:
        static ZenRuntime &instance();

        zen::VM &vm();

        std::size_t compileCount() const { return mCompileCount; }
        std::size_t cachedClassCount() const;

        unsigned int generation() const { return mGeneration; }

        void reset();
        bool invalidate(const char *path);
        bool recompile(const char *path);
        std::size_t refreshChangedFiles();

        // Detailed VM timers are opt-in because they instrument every script
        // callback. Call submitProfilerSamples once after update/render.
        void setVmProfiling(bool enabled);
        bool vmProfiling() const;
        void submitProfilerSamples();

        struct Impl;
        Impl &impl() { return *mImpl; }

    private:
        ZenRuntime();
        ~ZenRuntime();

        ZenRuntime(const ZenRuntime &) = delete;
        ZenRuntime &operator=(const ZenRuntime &) = delete;

        Impl *mImpl;
        std::size_t mCompileCount;
        unsigned int mGeneration;

        friend class ZenScriptComponent;
    };

}
