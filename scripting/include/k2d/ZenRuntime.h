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
        std::size_t cachedInstanceCount() const;
        // Whether a script handle is cached for this component address. The
        // cache is what keeps a removed component's handle alive, so being
        // able to see it is what makes that testable.
        bool hasCachedInstance(const void *pointer) const;

        unsigned int generation() const { return mGeneration; }

        void reset();
        bool invalidate(const char *path);
        bool recompile(const char *path);
        std::size_t refreshChangedFiles();

        // Compiles a Zen source file to the portable VM bytecode format used
        // by exported builds. The native Kinetix bindings must be identical
        // when the resulting file is loaded.
        bool compileFileToBytecode(const char *sourcePath, const char *bytecodePath,
                                   bool stripDebug = true, ct::String *error = nullptr);
        bool compileSourceToBytecode(const char *source, const char *sourceName, const char *bytecodePath,
                                     bool stripDebug = true, ct::String *error = nullptr);

        // A Web export loads its complete script bundle once, then maps the
        // logical script paths stored in scenes to the classes it defines.
        bool loadBytecodeBundle(const char *bytecodePath, ct::String *error = nullptr);
        bool registerBytecodeScript(const char *scriptPath, const char *className,
                                    ct::String *error = nullptr);

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
