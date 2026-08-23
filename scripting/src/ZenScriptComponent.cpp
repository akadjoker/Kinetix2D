#include "k2d/ZenScriptComponent.h"

#include "k2d/Animation2D.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"
#include "k2d/Input.h"
#include "k2d/ParticleComponent.h"
#include "k2d/Scene.h"
#include "k2d/Serializer.h"
#include "k2d/SpriteComponent.h"

#include <zen/vm.h>
#include <zen/compiler.h>
#include <zen/module.h>
#include <zen/object.h>

#include <cstring>

namespace k2d
{

    namespace
    {
        Input *gZenInput = nullptr;

        int scancodeFor(const char *name)
        {
            if (!name || !name[0])
                return -1;
            if (!name[1])
            {
                const char c = name[0];
                if (c >= 'a' && c <= 'z')
                    return 4 + (c - 'a');
                if (c >= 'A' && c <= 'Z')
                    return 4 + (c - 'A');
                if (c >= '1' && c <= '9')
                    return 30 + (c - '1');
                if (c == '0')
                    return 39;
                return -1;
            }
            struct Named
            {
                const char *name;
                int code;
            };
            static const Named named[] = {
                {"space", 44}, {"escape", 41}, {"enter", 40}, {"tab", 43}, {"backspace", 42},
                {"right", 79}, {"left", 80}, {"down", 81}, {"up", 82},
                {"lshift", 225}, {"rshift", 229}, {"lctrl", 224}, {"rctrl", 228}, {"lalt", 226},
            };
            for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i)
                if (std::strcmp(named[i].name, name) == 0)
                    return named[i].code;
            return -1;
        }

        const char *valueToCString(zen::VM *vm, zen::Value v, char *smallBuffer, size_t smallSize)
        {
            if (zen::is_small_string(v))
            {
                const int len = zen::small_string_len(v);
                const char *chars = zen::small_string_chars(v);
                size_t n = (size_t)len < smallSize - 1 ? (size_t)len : smallSize - 1;
                std::memcpy(smallBuffer, chars, n);
                smallBuffer[n] = '\0';
                return smallBuffer;
            }
            if (zen::is_obj(v) && v.as.obj && v.as.obj->type == zen::OBJ_STRING)
                return ((zen::ObjString *)v.as.obj)->chars;
            (void)vm;
            return "";
        }
    }

    struct ZenScriptComponent::State
    {
        struct CachedInstance
        {
            const void *key;
            zen::Value value;
        };

        zen::VM vm;
        zen::ObjClass *nodeClass = nullptr;
        zen::ObjClass *spriteClass = nullptr;
        zen::ObjClass *animationClass = nullptr;
        zen::ObjClass *particleClass = nullptr;
        ct::Vector<CachedInstance> cache;
        zen::Value self = zen::val_nil();
        int readyIdx = -1;
        int updateIdx = -1;
        bool loaded = false;
        bool started = false;

        State();

        zen::Value instanceFor(zen::ObjClass *klass, void *ptr)
        {
            if (!ptr)
                return zen::val_nil();
            for (size_t i = 0; i < cache.size(); ++i)
                if (cache[i].key == ptr)
                    return cache[i].value;
            zen::Value value = vm.make_instance(klass);
            zen::as_instance(value)->native_data = ptr;
            CachedInstance cached;
            cached.key = ptr;
            cached.value = value;
            cache.push_back(cached);
            return value;
        }
    };

    namespace
    {
        ZenScriptComponent::State *stateFromVM(zen::VM *vm)
        {
            const zen::Value v = vm->get_global("__k2d");
            return zen::is_ptr(v) ? (ZenScriptComponent::State *)zen::as_ptr(v) : nullptr;
        }

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

        int natNodeRotate(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 1)
                node->rotate((float)zen::to_number(args[0]));
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

        int natNodeSetActive(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 1)
                node->setActive(zen::is_truthy(args[0]));
            return 0;
        }

        int natNodeIsActive(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_bool(node ? node->active() : false);
            return 1;
        }

        int natNodeSetZIndex(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 1)
                node->setZIndex((int)zen::to_integer(args[0]));
            return 0;
        }

        int natNodeGetZIndex(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_int(node ? node->zIndex() : 0);
            return 1;
        }

        int natNodeQueueDestroy(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            if (node)
                node->dispose();
            return 0;
        }

        int natNodeGetParent(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            args[0] = (node && state) ? state->instanceFor(state->nodeClass, node->parent())
                                      : zen::val_nil();
            return 1;
        }

        int natNodeChildCount(zen::VM *, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            args[0] = zen::val_int(node ? (int64_t)node->childCount() : 0);
            return 1;
        }

        int natNodeGetChild(zen::VM *vm, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            GameObject *child = nullptr;
            if (node && nargs >= 1)
            {
                const int64_t index = zen::to_integer(args[0]);
                if (index >= 0 && (size_t)index < node->childCount())
                    child = node->child((size_t)index);
            }
            args[0] = (child && state) ? state->instanceFor(state->nodeClass, child) : zen::val_nil();
            return 1;
        }

        int natNodeFind(zen::VM *vm, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            GameObject *found = nullptr;
            if (node && node->scene() && nargs >= 1)
            {
                char small[16];
                found = node->scene()->find(valueToCString(vm, args[0], small, sizeof(small)));
            }
            args[0] = (found && state) ? state->instanceFor(state->nodeClass, found) : zen::val_nil();
            return 1;
        }

        int natNodeCreateChild(zen::VM *vm, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            GameObject *child = nullptr;
            if (node && node->scene() && nargs >= 1)
            {
                char small[16];
                child = node->scene()->createObject(valueToCString(vm, args[0], small, sizeof(small)), node);
            }
            args[0] = (child && state) ? state->instanceFor(state->nodeClass, child) : zen::val_nil();
            return 1;
        }

        int natNodeGetSprite(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            SpriteComponent *sprite = node ? node->getComponent<SpriteComponent>() : nullptr;
            args[0] = (sprite && state) ? state->instanceFor(state->spriteClass, sprite) : zen::val_nil();
            return 1;
        }

        int natNodeGetAnimation(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            Animation2D *animation = node ? node->getComponent<Animation2D>() : nullptr;
            args[0] = (animation && state) ? state->instanceFor(state->animationClass, animation)
                                           : zen::val_nil();
            return 1;
        }

        int natNodeGetParticle(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenScriptComponent::State *state = stateFromVM(vm);
            ParticleComponent *particle = node ? node->getComponent<ParticleComponent>() : nullptr;
            args[0] = (particle && state) ? state->instanceFor(state->particleClass, particle)
                                          : zen::val_nil();
            return 1;
        }

        int natSpriteSetColor(zen::VM *, zen::Value *args, int nargs)
        {
            SpriteComponent *sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
            if (sprite && nargs >= 4)
                sprite->setColor((unsigned char)zen::to_integer(args[0]),
                                 (unsigned char)zen::to_integer(args[1]),
                                 (unsigned char)zen::to_integer(args[2]),
                                 (unsigned char)zen::to_integer(args[3]));
            return 0;
        }

        int natSpriteSetFlip(zen::VM *, zen::Value *args, int nargs)
        {
            SpriteComponent *sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
            if (sprite && nargs >= 2)
                sprite->setFlip(zen::is_truthy(args[0]), zen::is_truthy(args[1]));
            return 0;
        }

        int natSpriteSetSize(zen::VM *, zen::Value *args, int nargs)
        {
            SpriteComponent *sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
            if (sprite && nargs >= 2)
                sprite->setSize(Math::Vec2((float)zen::to_number(args[0]),
                                           (float)zen::to_number(args[1])));
            return 0;
        }

        int natAnimationPlay(zen::VM *vm, zen::Value *args, int nargs)
        {
            Animation2D *animation = zen::zen_instance_data<Animation2D>(args[-1]);
            bool ok = false;
            if (animation && nargs >= 1)
            {
                char small[16];
                ok = animation->play(valueToCString(vm, args[0], small, sizeof(small)));
            }
            else if (animation)
            {
                animation->play();
                ok = true;
            }
            args[0] = zen::val_bool(ok);
            return 1;
        }

        int natAnimationStop(zen::VM *, zen::Value *args, int)
        {
            Animation2D *animation = zen::zen_instance_data<Animation2D>(args[-1]);
            if (animation)
                animation->stop();
            return 0;
        }

        int natAnimationIsPlaying(zen::VM *, zen::Value *args, int)
        {
            Animation2D *animation = zen::zen_instance_data<Animation2D>(args[-1]);
            args[0] = zen::val_bool(animation ? animation->playing() : false);
            return 1;
        }

        int natAnimationCurrent(zen::VM *vm, zen::Value *args, int)
        {
            Animation2D *animation = zen::zen_instance_data<Animation2D>(args[-1]);
            args[0] = zen::val_obj((zen::Obj *)vm->make_string(animation ? animation->currentClip() : ""));
            return 1;
        }

        int natParticleStart(zen::VM *, zen::Value *args, int)
        {
            ParticleComponent *particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
            if (particle)
                particle->system().Start();
            return 0;
        }

        int natParticleStop(zen::VM *, zen::Value *args, int)
        {
            ParticleComponent *particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
            if (particle)
                particle->system().Stop();
            return 0;
        }

        int natParticleReset(zen::VM *, zen::Value *args, int)
        {
            ParticleComponent *particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
            if (particle)
                particle->system().Reset();
            return 0;
        }

        int natParticleBurst(zen::VM *, zen::Value *args, int nargs)
        {
            ParticleComponent *particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
            if (particle)
            {
                const int64_t count = nargs >= 1 ? zen::to_integer(args[0]) : 1;
                for (int64_t i = 0; i < count; ++i)
                    particle->system().Emit(particle->system().EmitterPosition(),
                                            particle->system().GetPrefab());
            }
            return 0;
        }

        int natParticleIsPlaying(zen::VM *, zen::Value *args, int)
        {
            ParticleComponent *particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
            args[0] = zen::val_bool(particle ? particle->system().IsPlaying() : false);
            return 1;
        }

        int natKeyDown(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const int code = nargs >= 1 ? scancodeFor(valueToCString(vm, args[0], small, sizeof(small))) : -1;
            args[0] = zen::val_bool(gZenInput && code >= 0 && gZenInput->KeyDown(code));
            return 1;
        }

        int natKeyPressed(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const int code = nargs >= 1 ? scancodeFor(valueToCString(vm, args[0], small, sizeof(small))) : -1;
            args[0] = zen::val_bool(gZenInput && code >= 0 && gZenInput->KeyPressed(code));
            return 1;
        }

        int natKeyReleased(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const int code = nargs >= 1 ? scancodeFor(valueToCString(vm, args[0], small, sizeof(small))) : -1;
            args[0] = zen::val_bool(gZenInput && code >= 0 && gZenInput->KeyReleased(code));
            return 1;
        }

        int natMouseDown(zen::VM *, zen::Value *args, int nargs)
        {
            const int button = nargs >= 1 ? (int)zen::to_integer(args[0]) : 0;
            args[0] = zen::val_bool(gZenInput && gZenInput->MouseDown(button));
            return 1;
        }

        int natMousePressed(zen::VM *, zen::Value *args, int nargs)
        {
            const int button = nargs >= 1 ? (int)zen::to_integer(args[0]) : 0;
            args[0] = zen::val_bool(gZenInput && gZenInput->MousePressed(button));
            return 1;
        }

        int natMouseX(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenInput ? gZenInput->MouseX() : 0.0);
            return 1;
        }

        int natMouseY(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenInput ? gZenInput->MouseY() : 0.0);
            return 1;
        }

        int natWheelY(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenInput ? gZenInput->WheelY() : 0.0);
            return 1;
        }
    }

    ZenScriptComponent::State::State()
    {
        vm.open_lib_globals(&zen::zen_lib_base);
        vm.register_lib(&zen::zen_lib_math);
        vm.register_lib(&zen::zen_lib_time);

        vm.def_global("__k2d", zen::val_ptr(this));

        auto node = vm.def_class("Node");
        node.method("get_name", &natNodeGetName, 0);
        node.method("get_x", &natNodeGetX, 0);
        node.method("get_y", &natNodeGetY, 0);
        node.method("get_position", &natNodeGetPosition, 0);
        node.method("set_position", &natNodeSetPosition, 2);
        node.method("translate", &natNodeTranslate, 2);
        node.method("get_rotation", &natNodeGetRotation, 0);
        node.method("set_rotation", &natNodeSetRotation, 1);
        node.method("rotate", &natNodeRotate, 1);
        node.method("get_scale_x", &natNodeGetScaleX, 0);
        node.method("get_scale_y", &natNodeGetScaleY, 0);
        node.method("set_scale", &natNodeSetScale, 2);
        node.method("set_visible", &natNodeSetVisible, 1);
        node.method("is_visible", &natNodeIsVisible, 0);
        node.method("set_active", &natNodeSetActive, 1);
        node.method("is_active", &natNodeIsActive, 0);
        node.method("set_z_index", &natNodeSetZIndex, 1);
        node.method("get_z_index", &natNodeGetZIndex, 0);
        node.method("queue_destroy", &natNodeQueueDestroy, 0);
        node.method("get_parent", &natNodeGetParent, 0);
        node.method("child_count", &natNodeChildCount, 0);
        node.method("get_child", &natNodeGetChild, 1);
        node.method("find", &natNodeFind, 1);
        node.method("create_child", &natNodeCreateChild, 1);
        node.method("get_sprite", &natNodeGetSprite, 0);
        node.method("get_animation", &natNodeGetAnimation, 0);
        node.method("get_particle", &natNodeGetParticle, 0);
        node.persistent(true).constructable(false);
        nodeClass = node.end();

        auto sprite = vm.def_class("Sprite");
        sprite.method("set_color", &natSpriteSetColor, 4);
        sprite.method("set_flip", &natSpriteSetFlip, 2);
        sprite.method("set_size", &natSpriteSetSize, 2);
        sprite.persistent(true).constructable(false);
        spriteClass = sprite.end();

        auto animation = vm.def_class("Animation");
        animation.method("play", &natAnimationPlay, -1);
        animation.method("stop", &natAnimationStop, 0);
        animation.method("is_playing", &natAnimationIsPlaying, 0);
        animation.method("current", &natAnimationCurrent, 0);
        animation.persistent(true).constructable(false);
        animationClass = animation.end();

        auto particle = vm.def_class("Particle");
        particle.method("start", &natParticleStart, 0);
        particle.method("stop", &natParticleStop, 0);
        particle.method("reset", &natParticleReset, 0);
        particle.method("burst", &natParticleBurst, 1);
        particle.method("is_playing", &natParticleIsPlaying, 0);
        particle.persistent(true).constructable(false);
        particleClass = particle.end();

        vm.def_native("key_down", &natKeyDown, 1);
        vm.def_native("key_pressed", &natKeyPressed, 1);
        vm.def_native("key_released", &natKeyReleased, 1);
        vm.def_native("mouse_down", &natMouseDown, 1);
        vm.def_native("mouse_pressed", &natMousePressed, 1);
        vm.def_native("mouse_x", &natMouseX, 0);
        vm.def_native("mouse_y", &natMouseY, 0);
        vm.def_native("wheel_y", &natWheelY, 0);
    }

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
            mState->self = mState->instanceFor(mState->nodeClass, owner());

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

    void SetZenScriptInput(Input *input)
    {
        gZenInput = input;
    }

    namespace
    {
        Component *createZenScript(GameObject &owner)
        {
            return owner.addComponent<ZenScriptComponent>();
        }

        void writeZenScript(const Component &component, ct::Json &data, Assets *)
        {
            const ZenScriptComponent &script = static_cast<const ZenScriptComponent &>(component);
            data.set("path", ct::Json(script.scriptPath().c_str()));
        }

        void readZenScript(Component &component, const ct::Json &data, Assets *)
        {
            ZenScriptComponent &script = static_cast<ZenScriptComponent &>(component);
            const char *path = data["path"].as_cstr("");
            if (path[0])
                script.loadFile(path);
        }

        bool matchZenScript(const Component &component)
        {
            return dynamic_cast<const ZenScriptComponent *>(&component) != nullptr;
        }
    }

    void RegisterZenScriptSerializer()
    {
        Serializer::RegisterType(ComponentType::Script, "ZenScript", &createZenScript,
                                 &writeZenScript, &readZenScript, &matchZenScript);
    }

}
