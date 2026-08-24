#pragma once

#include "k2d/ZenRuntime.h"
#include "k2d/ZenScriptComponent.h"

#include <zen/value.h>
#include <zen/vm.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstddef>

namespace k2d
{

    std::size_t CollectZenClassProperties(zen::ObjClass *klass, ct::Vector<ZenScriptProperty> &out);


    struct ZenScriptClass
    {
        ct::String path;
        zen::Value klass = zen::val_nil();
        zen::Value module = zen::val_nil();
        int slotInit = -1;
        int slotStart = -1;
        int slotUpdate = -1;
        int slotDestroy = -1;
        int slotEvent = -1;
        int slotCollision = -1;
        long long timestamp = 0;
        unsigned int version = 1;
        ct::Vector<ZenScriptProperty> properties;
    };

    long long ZenFileTimestamp(const char *path);
    void BuildZenClassProperties(ZenScriptClass &entry, const char *source);

    struct ZenRuntime::Impl
    {
        struct CachedInstance
        {
            void *key = nullptr;
            zen::Value value = zen::val_nil();
        };

        zen::VM vm;
        zen::ObjClass *nodeClass = nullptr;
        zen::ObjClass *spriteClass = nullptr;
        zen::ObjClass *animationClass = nullptr;
        zen::ObjClass *particleClass = nullptr;
        zen::ObjClass *bodyClass = nullptr;

        ct::Vector<ZenScriptClass *> classes;
        ct::Vector<CachedInstance> instances;
        ct::Vector<zen::Value *> liveInstances;

        int executing = 0;

        int selectorInit = -1;
        int selectorStart = -1;
        int selectorUpdate = -1;
        int selectorDestroy = -1;
        int selectorEvent = -1;
        int selectorCollision = -1;

        void initialize();
        bool compileClass(const char *source, const char *path, ZenScriptClass &out);
        bool recompileClass(ZenScriptClass &entry, const char *source);
        ZenScriptClass *findClass(const char *path);
        ZenScriptClass *addClass(const ZenScriptClass &entry);
        void clearClasses();
        zen::Value instanceFor(zen::ObjClass *klass, void *ptr);
        void forgetInstance(void *ptr);
    };

}
