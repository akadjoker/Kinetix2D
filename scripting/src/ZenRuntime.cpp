#include "k2d/ZenRuntime.h"
#include "ZenRuntimeInternal.h"

#include <zen/compiler.h>
#include <zen/bytecode.h>
#include <zen/memory.h>
#include <zen/object.h>
#include <zen/vm.h>

#include <cstring>
#include <filesystem>
#include <system_error>

#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/Profiler.h"

#include <SDL.h>

namespace k2d
{

    namespace
    {
        void markRuntimeRoots(zen::GC *gc, void *ud)
        {
            ZenRuntime::Impl &impl = *static_cast<ZenRuntime::Impl *>(ud);

            for (size_t i = 0; i < impl.classes.size(); ++i)
            {
                zen::gc_mark_value(gc, impl.classes[i]->klass);
                zen::gc_mark_value(gc, impl.classes[i]->module);
            }
            for (const auto &entry : impl.instances)
                zen::gc_mark_value(gc, entry.value.value);
            for (size_t i = 0; i < impl.liveInstances.size(); ++i)
                zen::gc_mark_value(gc, *impl.liveInstances[i].value);
        }
    }

    ZenRuntime::ZenRuntime() : mImpl(new Impl()), mCompileCount(0), mGeneration(1)
    {
        mImpl->initialize();
        mImpl->vm.get_gc().extra_mark = &markRuntimeRoots;
        mImpl->vm.get_gc().extra_mark_ud = mImpl;
    }

    ZenRuntime::~ZenRuntime()
    {
        // classes owns raw ZenScriptClass allocations.  ct::Vector only owns
        // the pointers, so clear the cache before destroying Impl.
        mImpl->clearClasses();
        mImpl->instances.clear();
        mImpl->prefabs.clear();
        mImpl->liveInstances.clear();
        delete mImpl;
    }

    ZenRuntime &ZenRuntime::instance()
    {
        static ZenRuntime runtime;
        return runtime;
    }

    zen::VM &ZenRuntime::vm()
    {
        return mImpl->vm;
    }

    std::size_t ZenRuntime::cachedClassCount() const
    {
        return mImpl->classes.size();
    }

    std::size_t ZenRuntime::cachedInstanceCount() const
    {
        return mImpl->instances.size();
    }

    bool ZenRuntime::hasCachedInstance(const void *pointer) const
    {
        return mImpl->instances.find(const_cast<void *>(pointer)) != nullptr;
    }

    void ZenRuntime::setVmProfiling(bool enabled)
    {
        mImpl->vmProfiling = enabled;
        if (!enabled)
        {
            mImpl->vmUpdateTicks = 0;
            mImpl->vmRenderTicks = 0;
            mImpl->vmOtherTicks = 0;
            mImpl->vmUpdateCalls = 0;
            mImpl->vmRenderCalls = 0;
            mImpl->vmOtherCalls = 0;
        }
    }

    bool ZenRuntime::vmProfiling() const
    {
        return mImpl->vmProfiling;
    }

    void ZenRuntime::submitProfilerSamples()
    {
        Impl &impl = *mImpl;
        if (!impl.vmProfiling)
            return;
        if (impl.vmProfileFrequency == 0)
            impl.vmProfileFrequency = SDL_GetPerformanceFrequency();

        const float toMilliseconds = 1000.0f / static_cast<float>(impl.vmProfileFrequency);
        Profiler &profiler = Profiler::Get();
        profiler.addSample("vm.update", static_cast<float>(impl.vmUpdateTicks) * toMilliseconds,
                           impl.vmUpdateCalls);
        profiler.addSample("vm.render", static_cast<float>(impl.vmRenderTicks) * toMilliseconds,
                           impl.vmRenderCalls);
        profiler.addSample("vm.other", static_cast<float>(impl.vmOtherTicks) * toMilliseconds,
                           impl.vmOtherCalls);

        impl.vmUpdateTicks = 0;
        impl.vmRenderTicks = 0;
        impl.vmOtherTicks = 0;
        impl.vmUpdateCalls = 0;
        impl.vmRenderCalls = 0;
        impl.vmOtherCalls = 0;
    }

    void ZenRuntime::reset()
    {
        mImpl->clearClasses();
        mImpl->instances.clear();
        mImpl->prefabs.clear();
        ++mGeneration;
    }

    void ZenRuntime::clearPrefabCache()
    {
        mImpl->prefabs.clear();
    }

    long long ZenFileTimestamp(const char *path)
    {
        if (!path || !path[0])
            return 0;
        ct::String resolved;
        if (!FileSystem::Instance().Resolve(path, resolved))
            return 0;
        std::error_code error;
        const std::filesystem::file_time_type time = std::filesystem::last_write_time(resolved.c_str(), error);
        if (error)
            return 0;
        return (long long)time.time_since_epoch().count();
    }

    bool ZenRuntime::Impl::recompileClass(ZenScriptClass &entry, const char *source)
    {
        ZenScriptClass rebuilt;
        if (!compileClass(source, entry.path.c_str(), rebuilt))
            return false;

        entry.klass = rebuilt.klass;
        entry.module = rebuilt.module;
        entry.slotInit = rebuilt.slotInit;
        entry.slotStart = rebuilt.slotStart;
        entry.slotUpdate = rebuilt.slotUpdate;
        entry.slotDraw = rebuilt.slotDraw;
        entry.slotDrawUi = rebuilt.slotDrawUi;
        entry.slotDestroy = rebuilt.slotDestroy;
        entry.slotEvent = rebuilt.slotEvent;
        entry.slotCollision = rebuilt.slotCollision;
        entry.slotAnimationEvent = rebuilt.slotAnimationEvent;
        entry.slotAnimationFinished = rebuilt.slotAnimationFinished;
        entry.slotParticleHit = rebuilt.slotParticleHit;
        entry.properties.clear();
        for (size_t i = 0; i < rebuilt.properties.size(); ++i)
            entry.properties.push_back(rebuilt.properties[i]);
        ++entry.version;
        return true;
    }

    bool ZenRuntime::recompile(const char *path)
    {
        ZenScriptClass *entry = path ? mImpl->findClass(path) : nullptr;
        if (!entry)
            return false;

        FileBuffer buffer;
        if (!FileSystem::Instance().LoadFile(path, buffer, true))
            return false;

        if (!mImpl->recompileClass(*entry, buffer.Text()))
            return false;

        ++mCompileCount;
        entry->timestamp = ZenFileTimestamp(path);
        return true;
    }

    std::size_t ZenRuntime::refreshChangedFiles()
    {
        std::size_t rebuilt = 0;
        for (size_t i = 0; i < mImpl->classes.size(); ++i)
        {
            ZenScriptClass *entry = mImpl->classes[i];
            if (!entry || entry->path.empty())
                continue;

            const long long written = ZenFileTimestamp(entry->path.c_str());
            if (written == 0 || written == entry->timestamp)
                continue;

            FileBuffer buffer;
            if (!FileSystem::Instance().LoadFile(entry->path.c_str(), buffer, true))
                continue;

            if (mImpl->recompileClass(*entry, buffer.Text()))
            {
                ++mCompileCount;
                entry->timestamp = written;
                ++rebuilt;
            }
            else
            {
                entry->timestamp = written;
            }
        }
        return rebuilt;
    }

    bool ZenRuntime::compileFileToBytecode(const char *sourcePath, const char *bytecodePath,
                                           bool stripDebug, ct::String *error)
    {
        if (error)
            error->clear();
        if (!sourcePath || !sourcePath[0] || !bytecodePath || !bytecodePath[0])
        {
            if (error)
                *error = "A source and output path are required";
            return false;
        }

        FileBuffer source;
        if (!FileSystem::Instance().LoadFile(sourcePath, source, true))
        {
            if (error)
                *error = "Could not read Zen source file";
            return false;
        }

        return compileSourceToBytecode(source.Text(), sourcePath, bytecodePath, stripDebug, error);
    }

    bool ZenRuntime::compileSourceToBytecode(const char *source, const char *sourceName,
                                             const char *bytecodePath, bool stripDebug, ct::String *error)
    {
        if (error)
            error->clear();
        if (!source || !source[0] || !bytecodePath || !bytecodePath[0])
        {
            if (error)
                *error = "Source and output paths are required";
            return false;
        }

        zen::Compiler compiler;
        zen::ObjFunc *script = compiler.compile(&mImpl->vm.get_gc(), &mImpl->vm, source,
                                                sourceName && sourceName[0] ? sourceName : "script");
        if (!script)
        {
            if (error)
                *error = "Zen compilation failed";
            return false;
        }

        char bytecodeError[256] = {};
        if (!zen::dump_bytecode_file(&mImpl->vm, script, bytecodePath, stripDebug, bytecodeError,
                                     static_cast<int>(sizeof(bytecodeError))))
        {
            if (error)
                *error = bytecodeError[0] ? bytecodeError : "Could not write Zen bytecode";
            return false;
        }
        return true;
    }

    bool ZenRuntime::loadBytecodeBundle(const char *bytecodePath, ct::String *error)
    {
        if (error)
            error->clear();
        if (!bytecodePath || !bytecodePath[0])
        {
            if (error)
                *error = "A bytecode path is required";
            return false;
        }

        FileBuffer bytecode;
        if (!FileSystem::Instance().LoadFile(bytecodePath, bytecode, false))
        {
            if (error)
                *error = "Could not read Zen bytecode bundle";
            return false;
        }

        char bytecodeError[256] = {};
        zen::ObjFunc *script = zen::load_bytecode_buffer(&mImpl->vm, bytecode.Data(), bytecode.Size(),
                                                         bytecodeError, static_cast<int>(sizeof(bytecodeError)));
        if (!script)
        {
            if (error)
                *error = bytecodeError[0] ? bytecodeError : "Could not load Zen bytecode bundle";
            return false;
        }

        mImpl->vm.run(script);
        if (mImpl->vm.had_error())
        {
            if (error)
                *error = "Zen bytecode bundle failed while initializing";
            return false;
        }
        return true;
    }

    bool ZenRuntime::registerBytecodeScript(const char *scriptPath, const char *className, ct::String *error)
    {
        if (error)
            error->clear();
        if (!scriptPath || !scriptPath[0] || !className || !className[0])
        {
            if (error)
                *error = "A script path and class name are required";
            return false;
        }
        if (mImpl->findClass(scriptPath))
            return true;

        const int global = mImpl->vm.find_global(className);
        if (global < 0 || !zen::is_class(mImpl->vm.get_global(global)))
        {
            if (error)
                *error = "Bytecode bundle does not define the requested script class";
            return false;
        }

        if (mImpl->selectorInit < 0)
        {
            mImpl->selectorInit = mImpl->vm.intern_selector("__init__", 8);
            mImpl->selectorStart = mImpl->vm.intern_selector("on_start", 8);
            mImpl->selectorUpdate = mImpl->vm.intern_selector("on_update", 9);
            mImpl->selectorDraw = mImpl->vm.intern_selector("on_draw", 7);
            mImpl->selectorDrawUi = mImpl->vm.intern_selector("on_draw_ui", 10);
            mImpl->selectorDestroy = mImpl->vm.intern_selector("on_destroy", 10);
            mImpl->selectorEvent = mImpl->vm.intern_selector("on_event", 8);
            mImpl->selectorCollision = mImpl->vm.intern_selector("on_collision", 12);
            mImpl->selectorAnimationEvent = mImpl->vm.intern_selector("on_animation_event", 18);
            mImpl->selectorAnimationFinished = mImpl->vm.intern_selector("on_animation_finished", 21);
            mImpl->selectorParticleHit = mImpl->vm.intern_selector("on_particle_hit", 15);
        }

        ZenScriptClass entry;
        entry.path = scriptPath;
        entry.klass = mImpl->vm.get_global(global);
        zen::ObjClass *klass = zen::as_class(entry.klass);
        const auto slotIfPresent = [klass](int slot) -> int
        {
            if (slot < 0 || slot >= klass->vtable_size || zen::is_nil(klass->vtable[slot]))
                return -1;
            return slot;
        };
        entry.slotInit = slotIfPresent(mImpl->selectorInit);
        entry.slotStart = slotIfPresent(mImpl->selectorStart);
        entry.slotUpdate = slotIfPresent(mImpl->selectorUpdate);
        entry.slotDraw = slotIfPresent(mImpl->selectorDraw);
        entry.slotDrawUi = slotIfPresent(mImpl->selectorDrawUi);
        entry.slotDestroy = slotIfPresent(mImpl->selectorDestroy);
        entry.slotEvent = slotIfPresent(mImpl->selectorEvent);
        entry.slotCollision = slotIfPresent(mImpl->selectorCollision);
        entry.slotAnimationEvent = slotIfPresent(mImpl->selectorAnimationEvent);
        entry.slotAnimationFinished = slotIfPresent(mImpl->selectorAnimationFinished);
        entry.slotParticleHit = slotIfPresent(mImpl->selectorParticleHit);
        BuildZenClassProperties(entry, nullptr);
        mImpl->addClass(entry);
        return true;
    }

    bool ZenRuntime::invalidate(const char *path)
    {
        if (!path)
            return false;
        for (size_t i = 0; i < mImpl->classes.size(); ++i)
        {
            if (mImpl->classes[i]->path != path)
                continue;
            delete mImpl->classes[i];
            mImpl->classes.erase(mImpl->classes.begin() + i);
            ++mGeneration;
            return true;
        }
        return false;
    }

    bool ZenRuntime::Impl::compileClass(const char *source, const char *path, ZenScriptClass &out)
    {
        if (!source)
            return false;

        const int savedGlobals = vm.num_globals();

        ct::Vector<zen::Value> snapshot;
        for (int i = 0; i < savedGlobals; ++i)
            snapshot.push_back(vm.get_global(i));

        zen::Compiler compiler;
        zen::ObjFunc *script = compiler.compile(&vm.get_gc(), &vm, source, path ? path : "script");
        if (!script)
        {
            vm.shrink_globals(savedGlobals);
            return false;
        }

        vm.run(script);
        if (vm.had_error())
        {
            vm.shrink_globals(savedGlobals);
            return false;
        }

        zen::Value found = zen::val_nil();
        for (int i = 0; i < vm.num_globals(); ++i)
        {
            const zen::Value candidate = vm.get_global(i);
            if (!zen::is_class(candidate))
                continue;

            const bool isNew = i >= savedGlobals;
            const bool changed = !isNew && (!zen::is_class(snapshot[i]) ||
                                            snapshot[i].as.obj != candidate.as.obj);
            if (isNew || changed)
            {
                found = candidate;
                break;
            }
        }

        if (!zen::is_class(found))
            return false;

        out.klass = found;
        out.module = zen::val_nil();

        if (selectorInit < 0)
        {
            selectorInit = vm.intern_selector("__init__", 8);
            selectorStart = vm.intern_selector("on_start", 8);
            selectorUpdate = vm.intern_selector("on_update", 9);
            selectorDraw = vm.intern_selector("on_draw", 7);
            selectorDrawUi = vm.intern_selector("on_draw_ui", 10);
            selectorDestroy = vm.intern_selector("on_destroy", 10);
            selectorEvent = vm.intern_selector("on_event", 8);
            selectorCollision = vm.intern_selector("on_collision", 12);
            selectorAnimationEvent = vm.intern_selector("on_animation_event", 18);
            selectorAnimationFinished = vm.intern_selector("on_animation_finished", 21);
            selectorParticleHit = vm.intern_selector("on_particle_hit", 15);
        }

        zen::ObjClass *klass = zen::as_class(found);
        const auto slotIfPresent = [klass](int slot) -> int
        {
            if (slot < 0 || slot >= klass->vtable_size || zen::is_nil(klass->vtable[slot]))
                return -1;
            return slot;
        };

        out.slotInit = slotIfPresent(selectorInit);
        out.slotStart = slotIfPresent(selectorStart);
        out.slotUpdate = slotIfPresent(selectorUpdate);
        out.slotDraw = slotIfPresent(selectorDraw);
        out.slotDrawUi = slotIfPresent(selectorDrawUi);
        out.slotDestroy = slotIfPresent(selectorDestroy);
        out.slotEvent = slotIfPresent(selectorEvent);
        out.slotCollision = slotIfPresent(selectorCollision);
        out.slotAnimationEvent = slotIfPresent(selectorAnimationEvent);
        out.slotAnimationFinished = slotIfPresent(selectorAnimationFinished);
        out.slotParticleHit = slotIfPresent(selectorParticleHit);
        BuildZenClassProperties(out, source);
        return true;
    }

    bool ZenRuntime::Impl::loadBytecodeClass(const unsigned char *data, size_t size, const char *path,
                                             ZenScriptClass &out)
    {
        if (!zen::is_bytecode_buffer(data, size))
            return false;

        const int savedGlobals = vm.num_globals();
        ct::Vector<zen::Value> snapshot;
        for (int i = 0; i < savedGlobals; ++i)
            snapshot.push_back(vm.get_global(i));

        char error[256] = {};
        zen::ObjFunc *script = zen::load_bytecode_buffer(&vm, data, size, error,
                                                         static_cast<int>(sizeof(error)));
        if (!script)
            return false;

        vm.run(script);
        if (vm.had_error())
            return false;

        zen::Value found = zen::val_nil();
        for (int i = 0; i < vm.num_globals(); ++i)
        {
            const zen::Value candidate = vm.get_global(i);
            if (!zen::is_class(candidate))
                continue;

            const bool isNew = i >= savedGlobals;
            const bool changed = !isNew && (!zen::is_class(snapshot[i]) ||
                                            snapshot[i].as.obj != candidate.as.obj);
            if (isNew || changed)
            {
                found = candidate;
                break;
            }
        }

        if (!zen::is_class(found))
            return false;

        if (selectorInit < 0)
        {
            selectorInit = vm.intern_selector("__init__", 8);
            selectorStart = vm.intern_selector("on_start", 8);
            selectorUpdate = vm.intern_selector("on_update", 9);
            selectorDraw = vm.intern_selector("on_draw", 7);
            selectorDrawUi = vm.intern_selector("on_draw_ui", 10);
            selectorDestroy = vm.intern_selector("on_destroy", 10);
            selectorEvent = vm.intern_selector("on_event", 8);
            selectorCollision = vm.intern_selector("on_collision", 12);
            selectorAnimationEvent = vm.intern_selector("on_animation_event", 18);
            selectorAnimationFinished = vm.intern_selector("on_animation_finished", 21);
            selectorParticleHit = vm.intern_selector("on_particle_hit", 15);
        }

        zen::ObjClass *klass = zen::as_class(found);
        const auto slotIfPresent = [klass](int slot) -> int
        {
            if (slot < 0 || slot >= klass->vtable_size || zen::is_nil(klass->vtable[slot]))
                return -1;
            return slot;
        };

        out.klass = found;
        out.module = zen::val_nil();
        out.slotInit = slotIfPresent(selectorInit);
        out.slotStart = slotIfPresent(selectorStart);
        out.slotUpdate = slotIfPresent(selectorUpdate);
        out.slotDraw = slotIfPresent(selectorDraw);
        out.slotDrawUi = slotIfPresent(selectorDrawUi);
        out.slotDestroy = slotIfPresent(selectorDestroy);
        out.slotEvent = slotIfPresent(selectorEvent);
        out.slotCollision = slotIfPresent(selectorCollision);
        out.slotAnimationEvent = slotIfPresent(selectorAnimationEvent);
        out.slotAnimationFinished = slotIfPresent(selectorAnimationFinished);
        out.slotParticleHit = slotIfPresent(selectorParticleHit);
        BuildZenClassProperties(out, nullptr);
        return true;
    }

    ZenScriptClass *ZenRuntime::Impl::findClass(const char *path)
    {
        for (size_t i = 0; i < classes.size(); ++i)
            if (classes[i]->path == path)
                return classes[i];
        return nullptr;
    }

    ZenScriptClass *ZenRuntime::Impl::addClass(const ZenScriptClass &entry)
    {
        ZenScriptClass *stored = new ZenScriptClass(entry);
        classes.push_back(stored);
        return stored;
    }

    void ZenRuntime::Impl::clearClasses()
    {
        for (size_t i = 0; i < classes.size(); ++i)
            delete classes[i];
        classes.clear();
    }

    zen::Value ZenRuntime::Impl::instanceFor(zen::ObjClass *klass, void *ptr)
    {
        if (!ptr)
            return zen::val_nil();
        if (CachedInstance *cached = instances.find(ptr))
        {
            if (cached->klass == klass)
                return cached->value;
            // The address is being re-handed out under a different class. The
            // entry below replaces this one, so neutralise it here or it stays
            // reachable from Zen with a payload nothing will ever clear.
            if (zen::is_instance(cached->value))
                zen::as_instance(cached->value)->native_data = nullptr;
        }

        zen::Value value = vm.make_instance(klass);
        zen::as_instance(value)->native_data = ptr;
        CachedInstance cached;
        cached.key = ptr;
        cached.klass = klass;
        cached.value = value;
        instances.put(ptr, cached);
        return value;
    }

    void ZenRuntime::Impl::forgetInstance(void *ptr)
    {
        // Dropping the cache entry alone leaves any handle a script already
        // holds pointing at freed memory: clear the payload too, so the
        // nodeFromSelf/componentFromSelf null checks catch the stale handle
        // instead of dereferencing it.
        CachedInstance cached;
        if (instances.erase(ptr, cached) && zen::is_instance(cached.value))
            zen::as_instance(cached.value)->native_data = nullptr;
    }

}
