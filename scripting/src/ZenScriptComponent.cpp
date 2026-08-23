#include "k2d/ZenScriptComponent.h"

#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"

#include <zen/vm.h>
#include <zen/compiler.h>
#include <zen/module.h>
#include <zen/object.h>

namespace k2d
{

    namespace
    {
        GameObject *nodeFromSelf(zen::Value *args)
        {
            return zen::zen_instance_data<GameObject>(args[-1]);
        }

        int natNodeGetName(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_obj((zen::Obj *)vm->make_string(node ? node->name().c_str() : ""));
            return 1;
        }

        int natNodeGetX(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_float(node ? node->position().x : 0.0);
            return 1;
        }

        int natNodeGetY(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_float(node ? node->position().y : 0.0);
            return 1;
        }

        int natNodeGetPosition(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_float(node ? node->position().x : 0.0);
            args[1] = zen::val_float(node ? node->position().y : 0.0);
            return 2;
        }

        int natNodeSetPosition(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 2)
                node->setPosition(Math::Vec2((float)zen::to_number(args[0]),
                                             (float)zen::to_number(args[1])));
            return 0;
        }

        int natNodeTranslate(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 2)
                node->translate(Math::Vec2((float)zen::to_number(args[0]),
                                           (float)zen::to_number(args[1])));
            return 0;
        }

        int natNodeGetRotation(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_float(node ? node->rotationDegrees() : 0.0);
            return 1;
        }

        int natNodeSetRotation(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 1)
                node->setRotationDegrees((float)zen::to_number(args[0]));
            return 0;
        }

        int natNodeGetScaleX(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_float(node ? node->scale().x : 1.0);
            return 1;
        }

        int natNodeGetScaleY(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_float(node ? node->scale().y : 1.0);
            return 1;
        }

        int natNodeSetScale(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 2)
                node->setScale(Math::Vec2((float)zen::to_number(args[0]),
                                          (float)zen::to_number(args[1])));
            return 0;
        }

        int natNodeSetVisible(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 1)
                node->setVisible(zen::is_truthy(args[0]));
            return 0;
        }

        int natNodeIsVisible(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_bool(node ? node->visible() : false);
            return 1;
        }
    }

    struct ZenScriptComponent::State
    {
        zen::VM vm;
        zen::ObjClass *nodeClass = nullptr;
        zen::Value self = zen::val_nil();
        int readyIdx = -1;
        int updateIdx = -1;
        bool loaded = false;
        bool started = false;

        State()
        {
            vm.open_lib_globals(&zen::zen_lib_base);
            vm.register_lib(&zen::zen_lib_math);
            vm.register_lib(&zen::zen_lib_time);

            auto builder = vm.def_class("Node");
            builder.method("get_name", &natNodeGetName, 0);
            builder.method("get_x", &natNodeGetX, 0);
            builder.method("get_y", &natNodeGetY, 0);
            builder.method("get_position", &natNodeGetPosition, 0);
            builder.method("set_position", &natNodeSetPosition, 2);
            builder.method("translate", &natNodeTranslate, 2);
            builder.method("get_rotation", &natNodeGetRotation, 0);
            builder.method("set_rotation", &natNodeSetRotation, 1);
            builder.method("get_scale_x", &natNodeGetScaleX, 0);
            builder.method("get_scale_y", &natNodeGetScaleY, 0);
            builder.method("set_scale", &natNodeSetScale, 2);
            builder.method("set_visible", &natNodeSetVisible, 1);
            builder.method("is_visible", &natNodeIsVisible, 0);
            builder.persistent(true).constructable(false);
            nodeClass = builder.end();
        }
    };

    ZenScriptComponent::ZenScriptComponent()
        : ScriptComponent(ComponentEventUpdate), mState(new State())
    {
    }

    ZenScriptComponent::~ZenScriptComponent()
    {
        delete mState;
    }

    bool ZenScriptComponent::loadSource(const char *source, const char *scriptName)
    {
        if (!source)
            return false;

        zen::Compiler compiler;
        zen::ObjFunc *script = compiler.compile(&mState->vm.get_gc(), &mState->vm, source,
                                                scriptName ? scriptName : "script");
        if (!script)
            return false;

        mState->vm.run(script);
        if (mState->vm.had_error())
            return false;

        mState->readyIdx = mState->vm.find_global("ready");
        mState->updateIdx = mState->vm.find_global("update");
        mState->loaded = true;
        mState->started = false;
        return true;
    }

    bool ZenScriptComponent::loadFile(const char *path)
    {
        if (!path || !path[0])
            return false;

        FileBuffer buffer;
        if (!FileSystem::Instance().LoadFile(path, buffer, true))
            return false;

        mScriptPath = path;
        return loadSource(buffer.Text(), path);
    }

    bool ZenScriptComponent::loaded() const
    {
        return mState->loaded;
    }

    void ZenScriptComponent::onUpdate(float deltaTime)
    {
        if (!mState->loaded || !owner())
            return;

        if (zen::is_nil(mState->self))
        {
            mState->self = mState->vm.make_instance(mState->nodeClass);
            zen::as_instance(mState->self)->native_data = owner();
        }

        if (!mState->started)
        {
            mState->started = true;
            if (mState->readyIdx >= 0)
            {
                zen::Value args[1] = {mState->self};
                mState->vm.call_global(mState->readyIdx, args, 1);
            }
        }

        if (mState->updateIdx >= 0)
        {
            zen::Value args[2] = {mState->self, zen::val_float(deltaTime)};
            mState->vm.call_global(mState->updateIdx, args, 2);
        }
    }

}
