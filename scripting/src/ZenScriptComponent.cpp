#include "k2d/ZenScriptComponent.h"

#include "k2d/PhysicsWorld2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/ZenRuntime.h"
#include "ZenRuntimeInternal.h"

#include "k2d/Animation2D.h"
#include "k2d/Assets.h"
#include "k2d/Camera2D.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"
#include "k2d/Input.h"
#include "k2d/ParticleComponent.h"
#include "k2d/RenderQueue.h"
#include "k2d/Scene.h"
#include "k2d/Serializer.h"
#include "k2d/SpriteComponent.h"

#include <zen/vm.h>
#include <zen/compiler.h>
#include <zen/module.h>
#include <zen/object.h>
#include <zen/zen_host_output.h>

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <system_error>

namespace k2d
{

    namespace
    {
        Input *gZenInput = nullptr;
        Assets *gZenAssets = nullptr;
        struct ZenGameViewport
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            bool valid = false;
        };
        ZenGameViewport gZenGameViewport;
        Camera2D gZenGameCamera;
        bool gZenGameCameraValid = false;
        float gZenFps = 0.0f;
        bool gZenProfilerVisible = false;
        bool gZenScriptsEnabled = false;
        void (*gZenOutput)(const char *text, bool isError, void *user) = nullptr;
        void *gZenOutputUser = nullptr;
        GameObject *gZenCallbackNode = nullptr;

        struct ZenCallbackScope
        {
            explicit ZenCallbackScope(GameObject *node) : previous(gZenCallbackNode)
            {
                gZenCallbackNode = node;
            }
            ~ZenCallbackScope() { gZenCallbackNode = previous; }

            GameObject *previous;
        };

        struct ZenDrawContext
        {
            RenderQueue *queue = nullptr;
            RenderItem *item = nullptr;
            int zIndex = 0;
            Color color = Color::White();
            Matrix2D xform;
            bool hasXform = false;

            RenderItem *ensureItem()
            {
                if (!item && queue)
                {
                    item = &queue->AddItem(zIndex);
                    if (hasXform)
                        item->xform = xform;
                }
                return item;
            }
        };

        ZenDrawContext *gZenDrawContext = nullptr;

        void zenHostWriter(const char *text, size_t length, int isError, void *)
        {
            if (!gZenOutput)
            {
                std::fwrite(text, 1, length, isError ? stderr : stdout);
                return;
            }
            char buffer[1024];
            const size_t n = length < sizeof(buffer) - 1 ? length : sizeof(buffer) - 1;
            std::memcpy(buffer, text, n);
            buffer[n] = '\0';
            gZenOutput(buffer, isError != 0, gZenOutputUser);
        }

        long long fileTimestamp(const char *path)
        {
            if (!path || !path[0])
                return 0;
            std::error_code error;
            const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, error);
            if (error)
                return 0;
            return (long long)time.time_since_epoch().count();
        }

        void preloadPrefabTextures(const ct::Json &node)
        {
            if (!gZenAssets)
                return;
            if (node.is_object())
            {
                const ct::Json::Object &members = node.members();
                for (size_t i = 0; i < members.size(); ++i)
                {
                    const ct::String &key = members[i].key;
                    const ct::Json &value = members[i].value;
                    if ((key == "texture" || key == "normalMap") && value.is_string())
                    {
                        const char *path = value.as_cstr("");
                        if (path[0] && !gZenAssets->GetTexture(path))
                            gZenAssets->LoadTexture(path, path, true, false);
                    }
                    else
                    {
                        preloadPrefabTextures(value);
                    }
                }
            }
            else if (node.is_array())
            {
                for (size_t i = 0; i < node.size(); ++i)
                    preloadPrefabTextures(node[i]);
            }
        }

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

        float drawColorComponent(zen::Value value)
        {
            const float component = (float)zen::to_number(value);
            return component < 0.0f ? 0.0f : (component > 1.0f ? 1.0f : component);
        }

        void addLineTriangles(ct::Vector<Math::Vec2> &triangles, float x0, float y0,
                              float x1, float y1, float thickness)
        {
            Math::Vec2 direction(x1 - x0, y1 - y0);
            const float length = direction.Length();
            if (length < 0.0001f)
                return;
            direction /= length;
            const float half = thickness > 0.0f ? thickness * 0.5f : 0.5f;
            const Math::Vec2 normal(-direction.y * half, direction.x * half);
            const Math::Vec2 a(x0, y0);
            const Math::Vec2 b(x1, y1);
            triangles.push_back(a - normal);
            triangles.push_back(a + normal);
            triangles.push_back(b + normal);
            triangles.push_back(a - normal);
            triangles.push_back(b + normal);
            triangles.push_back(b - normal);
        }

        int natSetDrawColor(zen::VM *, zen::Value *args, int nargs)
        {
            if (gZenDrawContext && nargs >= 3)
            {
                gZenDrawContext->color.r = drawColorComponent(args[0]);
                gZenDrawContext->color.g = drawColorComponent(args[1]);
                gZenDrawContext->color.b = drawColorComponent(args[2]);
                gZenDrawContext->color.a = nargs >= 4 ? drawColorComponent(args[3]) : 1.0f;
            }
            return 0;
        }

        int natDrawLine(zen::VM *, zen::Value *args, int nargs)
        {
            RenderItem *item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
            if (!item || nargs < 4)
                return 0;
            RenderCommand command;
            command.type = RenderCommand::kPolygon;
            command.color = gZenDrawContext->color;
            addLineTriangles(command.ownedPolygonPoints, (float)zen::to_number(args[0]),
                             (float)zen::to_number(args[1]), (float)zen::to_number(args[2]),
                             (float)zen::to_number(args[3]),
                             nargs >= 5 ? (float)zen::to_number(args[4]) : 1.0f);
            if (!command.ownedPolygonPoints.empty())
                item->commands.push_back(command);
            return 0;
        }

        int natDrawRect(zen::VM *, zen::Value *args, int nargs)
        {
            RenderItem *item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
            if (!item || nargs < 4)
                return 0;
            const float x = (float)zen::to_number(args[0]);
            const float y = (float)zen::to_number(args[1]);
            const float width = (float)zen::to_number(args[2]);
            const float height = (float)zen::to_number(args[3]);
            const bool fill = nargs < 5 || zen::is_truthy(args[4]);
            RenderCommand command;
            command.color = gZenDrawContext->color;
            if (fill)
            {
                command.type = RenderCommand::kRect;
                command.x = x;
                command.y = y;
                command.width = width;
                command.height = height;
                command.pivotX = 0.0f;
                command.pivotY = 0.0f;
            }
            else
            {
                command.type = RenderCommand::kPolygon;
                const float thickness = nargs >= 6 ? (float)zen::to_number(args[5]) : 1.0f;
                addLineTriangles(command.ownedPolygonPoints, x, y, x + width, y, thickness);
                addLineTriangles(command.ownedPolygonPoints, x + width, y, x + width, y + height, thickness);
                addLineTriangles(command.ownedPolygonPoints, x + width, y + height, x, y + height, thickness);
                addLineTriangles(command.ownedPolygonPoints, x, y + height, x, y, thickness);
            }
            item->commands.push_back(command);
            return 0;
        }

        int natDrawCircle(zen::VM *, zen::Value *args, int nargs)
        {
            RenderItem *item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
            if (!item || nargs < 3)
                return 0;
            const float x = (float)zen::to_number(args[0]);
            const float y = (float)zen::to_number(args[1]);
            const float radius = (float)zen::to_number(args[2]);
            if (radius <= 0.0f)
                return 0;
            const bool fill = nargs < 4 || zen::is_truthy(args[3]);
            int segments = nargs >= 5 ? (int)zen::to_integer(args[4]) : 32;
            segments = segments < 3 ? 3 : (segments > 512 ? 512 : segments);
            const float thickness = nargs >= 6 ? (float)zen::to_number(args[5]) : 1.0f;
            const float pi = 3.14159265358979323846f;

            RenderCommand command;
            command.type = RenderCommand::kPolygon;
            command.color = gZenDrawContext->color;
            for (int i = 0; i < segments; ++i)
            {
                const float a0 = (float)i * 2.0f * pi / (float)segments;
                const float a1 = (float)(i + 1) * 2.0f * pi / (float)segments;
                const float x0 = x + std::cos(a0) * radius;
                const float y0 = y + std::sin(a0) * radius;
                const float x1 = x + std::cos(a1) * radius;
                const float y1 = y + std::sin(a1) * radius;
                if (fill)
                {
                    command.ownedPolygonPoints.push_back(Math::Vec2(x, y));
                    command.ownedPolygonPoints.push_back(Math::Vec2(x0, y0));
                    command.ownedPolygonPoints.push_back(Math::Vec2(x1, y1));
                }
                else
                {
                    addLineTriangles(command.ownedPolygonPoints, x0, y0, x1, y1, thickness);
                }
            }
            item->commands.push_back(command);
            return 0;
        }

        int natDrawText(zen::VM *vm, zen::Value *args, int nargs)
        {
            RenderItem *item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
            if (!item || nargs < 3)
                return 0;
            char small[16];
            RenderCommand command;
            command.type = RenderCommand::kText;
            command.x = (float)zen::to_number(args[0]);
            command.y = (float)zen::to_number(args[1]);
            command.width = nargs >= 4 ? (float)zen::to_number(args[3]) : 16.0f;
            command.color = gZenDrawContext->color;
            command.text = valueToCString(vm, args[2], small, sizeof(small));
            if (command.width > 0.0f && !command.text.empty())
                item->commands.push_back(command);
            return 0;
        }

        int natDrawTextWidth(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const char *text = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
            const float size = nargs >= 2 ? (float)zen::to_number(args[1]) : 16.0f;
            unsigned int longest = 0;
            unsigned int length = 0;
            for (const char *character = text; *character; ++character)
            {
                if (*character == '\n')
                {
                    if (length > longest)
                        longest = length;
                    length = 0;
                }
                else
                {
                    ++length;
                }
            }
            if (length > longest)
                longest = length;
            args[0] = zen::val_float((double)longest * (size > 0.0f ? size : 0.0f));
            return 1;
        }

        int natObjectCount(zen::VM *, zen::Value *args, int)
        {
            const Scene *scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
            args[0] = zen::val_int(scene ? (int64_t)scene->objectCount() : 0);
            return 1;
        }
    }

    struct ZenScriptComponent::State
    {
        ZenScriptClass *scriptClass = nullptr;
        zen::Value self = zen::val_nil();
        zen::Value instance = zen::val_nil();
        unsigned int generation = 0;
        unsigned int classVersion = 0;
        bool loaded = false;
        bool pending = false;
        bool started = false;
    };

    namespace
    {
        enum class ScriptProfilePhase
        {
            Other,
            Update,
            Render
        };

        struct RunningScript
        {
            explicit RunningScript(ScriptProfilePhase phase = ScriptProfilePhase::Other)
                : mImpl(ZenRuntime::instance().impl()), mPhase(phase), mCounter(0), mProfiling(mImpl.vmProfiling)
            {
                ++mImpl.executing;
                if (mProfiling)
                    mCounter = SDL_GetPerformanceCounter();
            }

            ~RunningScript()
            {
                if (mProfiling)
                {
                    const uint64_t elapsed = SDL_GetPerformanceCounter() - mCounter;
                    switch (mPhase)
                    {
                    case ScriptProfilePhase::Update:
                        mImpl.vmUpdateTicks += elapsed;
                        ++mImpl.vmUpdateCalls;
                        break;
                    case ScriptProfilePhase::Render:
                        mImpl.vmRenderTicks += elapsed;
                        ++mImpl.vmRenderCalls;
                        break;
                    default:
                        mImpl.vmOtherTicks += elapsed;
                        ++mImpl.vmOtherCalls;
                        break;
                    }
                }
                --mImpl.executing;
            }

            ZenRuntime::Impl &mImpl;
            ScriptProfilePhase mPhase;
            uint64_t mCounter;
            bool mProfiling;
        };

        bool scriptRunning()
        {
            return ZenRuntime::instance().impl().executing > 0;
        }
    }

    namespace
    {
        ZenRuntime::Impl *stateFromVM(zen::VM *)
        {
            return &ZenRuntime::instance().impl();
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
            ZenRuntime::Impl *state = stateFromVM(vm);
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
            ZenRuntime::Impl *state = stateFromVM(vm);
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
            ZenRuntime::Impl *state = stateFromVM(vm);
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
            ZenRuntime::Impl *state = stateFromVM(vm);
            GameObject *child = nullptr;
            if (node && node->scene() && nargs >= 1)
            {
                char small[16];
                child = node->scene()->createObject(valueToCString(vm, args[0], small, sizeof(small)), node);
            }
            args[0] = (child && state) ? state->instanceFor(state->nodeClass, child) : zen::val_nil();
            return 1;
        }

        int natNodeSpawn(zen::VM *vm, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            ZenRuntime::Impl *state = stateFromVM(vm);
            GameObject *spawned = nullptr;
            if (node && node->scene() && nargs >= 1)
            {
                char small[16];
                const char *path = valueToCString(vm, args[0], small, sizeof(small));
                FileBuffer buffer;
                if (FileSystem::Instance().LoadFile(path, buffer, true))
                {
                    ct::Json::Error err;
                    const ct::Json json = ct::Json::parse(buffer.Text(), &err);
                    if (!err)
                    {
                        preloadPrefabTextures(json);
                        spawned = Serializer::ReadObject(*node->scene(), json, nullptr, gZenAssets);
                        if (spawned && nargs >= 3)
                            spawned->setPosition(Math::Vec2((float)zen::to_number(args[1]),
                                                            (float)zen::to_number(args[2])));
                    }
                }
            }
            args[0] = (spawned && state) ? state->instanceFor(state->nodeClass, spawned) : zen::val_nil();
            return 1;
        }

        int natNodeDistanceTo(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            double distance = 0.0;
            if (node && nargs >= 2)
            {
                const Math::Vec2 p = node->globalPosition();
                const double dx = zen::to_number(args[0]) - p.x;
                const double dy = zen::to_number(args[1]) - p.y;
                distance = std::sqrt(dx * dx + dy * dy);
            }
            args[0] = zen::val_float(distance);
            return 1;
        }

        int natNodeAngleTo(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            double degrees = 0.0;
            if (node && nargs >= 2)
            {
                const Math::Vec2 p = node->globalPosition();
                degrees = std::atan2(zen::to_number(args[1]) - p.y,
                                     zen::to_number(args[0]) - p.x) * 57.29577951308232;
            }
            args[0] = zen::val_float(degrees);
            return 1;
        }

        int natNodeLookAt(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 2)
            {
                const Math::Vec2 p = node->globalPosition();
                node->setRotationDegrees((float)(std::atan2(zen::to_number(args[1]) - p.y,
                                                            zen::to_number(args[0]) - p.x) *
                                                 57.29577951308232));
            }
            return 0;
        }

        int natNodeMoveToward(zen::VM *, zen::Value *args, int nargs)
        {
            GameObject *node = nodeFromSelf(args);
            if (node && nargs >= 3)
            {
                const Math::Vec2 p = node->position();
                const float tx = (float)zen::to_number(args[0]);
                const float ty = (float)zen::to_number(args[1]);
                const float maxDistance = (float)zen::to_number(args[2]);
                const float dx = tx - p.x;
                const float dy = ty - p.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length <= maxDistance || length < 0.0001f)
                    node->setPosition(Math::Vec2(tx, ty));
                else
                    node->setPosition(Math::Vec2(p.x + dx / length * maxDistance,
                                                 p.y + dy / length * maxDistance));
            }
            const Math::Vec2 result = node ? node->position() : Math::Vec2(0.0f, 0.0f);
            args[0] = zen::val_float(result.x);
            args[1] = zen::val_float(result.y);
            return 2;
        }

        int natRaycast(zen::VM *vm, zen::Value *args, int nargs)
        {
            ZenRuntime::Impl *state = stateFromVM(vm);
            GameObject *hit = nullptr;
            Math::Vec2 point(0.0f, 0.0f);
            if (PhysicsWorld2D *world = PhysicsWorld2D::Active())
            {
                if (nargs >= 5)
                {
                    const Math::Vec2 origin((float)zen::to_number(args[0]),
                                            (float)zen::to_number(args[1]));
                    const Math::Vec2 direction((float)zen::to_number(args[2]),
                                               (float)zen::to_number(args[3]));
                    hit = world->raycast(origin, direction, (float)zen::to_number(args[4]), &point);
                }
            }
            args[0] = (hit && state) ? state->instanceFor(state->nodeClass, hit) : zen::val_nil();
            args[1] = zen::val_float(point.x);
            args[2] = zen::val_float(point.y);
            return 3;
        }

        int natBodyAt(zen::VM *vm, zen::Value *args, int nargs)
        {
            ZenRuntime::Impl *state = stateFromVM(vm);
            GameObject *found = nullptr;
            if (PhysicsWorld2D *world = PhysicsWorld2D::Active())
                if (nargs >= 2)
                    found = world->objectAtPoint(Math::Vec2((float)zen::to_number(args[0]),
                                                            (float)zen::to_number(args[1])));
            args[0] = (found && state) ? state->instanceFor(state->nodeClass, found)
                                       : zen::val_nil();
            return 1;
        }

        int natSetGravity(zen::VM *, zen::Value *args, int nargs)
        {
            if (PhysicsWorld2D *world = PhysicsWorld2D::Active())
                if (nargs >= 2)
                    world->setGravity(Math::Vec2((float)zen::to_number(args[0]),
                                                 (float)zen::to_number(args[1])));
            args[0] = zen::val_nil();
            return 1;
        }

        int natGetGravity(zen::VM *, zen::Value *args, int)
        {
            PhysicsWorld2D *world = PhysicsWorld2D::Active();
            const Math::Vec2 gravity = world ? world->gravity() : Math::Vec2(0.0f, 0.0f);
            args[0] = zen::val_float(gravity.x);
            args[1] = zen::val_float(gravity.y);
            return 2;
        }

        RigidBody2D *bodyFromSelf(zen::Value *args)
        {
            return zen::zen_instance_data<RigidBody2D>(args[-1]);
        }

        int natNodeGetBody(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenRuntime::Impl *state = stateFromVM(vm);
            RigidBody2D *body = node ? node->getComponent<RigidBody2D>() : nullptr;
            args[0] = (body && state) ? state->instanceFor(state->bodyClass, body) : zen::val_nil();
            return 1;
        }

        int natBodyGetVelocity(zen::VM *, zen::Value *args, int)
        {
            RigidBody2D *body = bodyFromSelf(args);
            const Math::Vec2 velocity = body ? body->velocity() : Math::Vec2(0.0f, 0.0f);
            args[0] = zen::val_float(velocity.x);
            args[1] = zen::val_float(velocity.y);
            return 2;
        }

        int natBodySetVelocity(zen::VM *, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 2)
                body->setVelocity(Math::Vec2((float)zen::to_number(args[0]),
                                             (float)zen::to_number(args[1])));
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodyGetAngularVelocity(zen::VM *, zen::Value *args, int)
        {
            RigidBody2D *body = bodyFromSelf(args);
            args[0] = zen::val_float(body ? body->angularVelocity() : 0.0);
            return 1;
        }

        int natBodySetAngularVelocity(zen::VM *, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 1)
                body->setAngularVelocity((float)zen::to_number(args[0]));
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodyApplyForce(zen::VM *, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 2)
                body->applyForce(Math::Vec2((float)zen::to_number(args[0]),
                                            (float)zen::to_number(args[1])));
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodyApplyImpulse(zen::VM *, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 2)
                body->applyImpulse(Math::Vec2((float)zen::to_number(args[0]),
                                              (float)zen::to_number(args[1])));
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodyApplyTorque(zen::VM *, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 1)
                body->applyTorque((float)zen::to_number(args[0]));
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodySetGravityScale(zen::VM *, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 1)
                body->setGravityScale((float)zen::to_number(args[0]));
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodyGetGravityScale(zen::VM *, zen::Value *args, int)
        {
            RigidBody2D *body = bodyFromSelf(args);
            args[0] = zen::val_float(body ? body->gravityScale() : 0.0);
            return 1;
        }

        int natBodyWake(zen::VM *, zen::Value *args, int)
        {
            if (RigidBody2D *body = bodyFromSelf(args))
                body->wake();
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodySetType(zen::VM *vm, zen::Value *args, int nargs)
        {
            RigidBody2D *body = bodyFromSelf(args);
            if (body && nargs >= 1)
            {
                char small[16];
                const char *name = valueToCString(vm, args[0], small, sizeof(small));
                if (std::strcmp(name, "static") == 0)
                    body->setBodyType(kx::BodyType::Static);
                else if (std::strcmp(name, "kinematic") == 0)
                    body->setBodyType(kx::BodyType::Kinematic);
                else
                    body->setBodyType(kx::BodyType::Dynamic);
            }
            args[0] = zen::val_nil();
            return 1;
        }

        int natBodyIsAwake(zen::VM *, zen::Value *args, int)
        {
            RigidBody2D *body = bodyFromSelf(args);
            args[0] = zen::val_bool(body && body->body() && body->body()->IsAwake());
            return 1;
        }

        int natNodeGetSprite(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenRuntime::Impl *state = stateFromVM(vm);
            SpriteComponent *sprite = node ? node->getComponent<SpriteComponent>() : nullptr;
            args[0] = (sprite && state) ? state->instanceFor(state->spriteClass, sprite) : zen::val_nil();
            return 1;
        }

        int natNodeGetAnimation(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenRuntime::Impl *state = stateFromVM(vm);
            Animation2D *animation = node ? node->getComponent<Animation2D>() : nullptr;
            args[0] = (animation && state) ? state->instanceFor(state->animationClass, animation)
                                           : zen::val_nil();
            return 1;
        }

        int natNodeGetParticle(zen::VM *vm, zen::Value *args, int)
        {
            GameObject *node = nodeFromSelf(args);
            ZenRuntime::Impl *state = stateFromVM(vm);
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

        int natGetNumber(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const char *key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
            const double fallback = nargs >= 2 ? zen::to_number(args[1]) : 0.0;
            args[0] = zen::val_float(ZenBlackboard::getNumber(key, fallback));
            return 1;
        }

        int natSetNumber(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            if (nargs >= 2)
                ZenBlackboard::setNumber(valueToCString(vm, args[0], small, sizeof(small)),
                                         zen::to_number(args[1]));
            return 0;
        }

        int natGetString(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            char smallFallback[16];
            const char *key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
            const char *fallback =
                nargs >= 2 ? valueToCString(vm, args[1], smallFallback, sizeof(smallFallback)) : "";
            const ct::String value = ZenBlackboard::getString(key, fallback);
            args[0] = zen::val_obj((zen::Obj *)vm->make_string(value.c_str()));
            return 1;
        }

        int natSetString(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            char smallValue[16];
            if (nargs >= 2)
                ZenBlackboard::setString(valueToCString(vm, args[0], small, sizeof(small)),
                                         valueToCString(vm, args[1], smallValue, sizeof(smallValue)));
            return 0;
        }

        int natGetFlag(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const char *key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
            const bool fallback = nargs >= 2 && zen::is_truthy(args[1]);
            args[0] = zen::val_bool(ZenBlackboard::getBool(key, fallback));
            return 1;
        }

        int natSetFlag(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            if (nargs >= 2)
                ZenBlackboard::setBool(valueToCString(vm, args[0], small, sizeof(small)),
                                       zen::is_truthy(args[1]));
            return 0;
        }

        int natHasKey(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            const char *key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
            args[0] = zen::val_bool(ZenBlackboard::has(key));
            return 1;
        }

        int natEmit(zen::VM *vm, zen::Value *args, int nargs)
        {
            char small[16];
            if (nargs >= 1)
                ZenBlackboard::emit(valueToCString(vm, args[0], small, sizeof(small)),
                                    nargs >= 2 ? zen::to_number(args[1]) : 0.0);
            return 0;
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
            const bool inside = !gZenGameViewport.valid ||
                                (gZenInput && gZenInput->MouseX() >= gZenGameViewport.x &&
                                 gZenInput->MouseX() < gZenGameViewport.x + gZenGameViewport.width &&
                                 gZenInput->MouseY() >= gZenGameViewport.y &&
                                 gZenInput->MouseY() < gZenGameViewport.y + gZenGameViewport.height);
            args[0] = zen::val_bool(gZenInput && inside && gZenInput->MouseDown(button));
            return 1;
        }

        int natMousePressed(zen::VM *, zen::Value *args, int nargs)
        {
            const int button = nargs >= 1 ? (int)zen::to_integer(args[0]) : 0;
            const bool inside = !gZenGameViewport.valid ||
                                (gZenInput && gZenInput->MouseX() >= gZenGameViewport.x &&
                                 gZenInput->MouseX() < gZenGameViewport.x + gZenGameViewport.width &&
                                 gZenInput->MouseY() >= gZenGameViewport.y &&
                                 gZenInput->MouseY() < gZenGameViewport.y + gZenGameViewport.height);
            args[0] = zen::val_bool(gZenInput && inside && gZenInput->MousePressed(button));
            return 1;
        }

        int natMouseX(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenInput ? gZenInput->MouseX() -
                                                  (gZenGameViewport.valid ? gZenGameViewport.x : 0.0f)
                                                : 0.0);
            return 1;
        }

        int natMouseY(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenInput ? gZenInput->MouseY() -
                                                  (gZenGameViewport.valid ? gZenGameViewport.y : 0.0f)
                                                : 0.0);
            return 1;
        }

        int natViewportWidth(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenGameViewport.valid ? gZenGameViewport.width : 1280.0f);
            return 1;
        }

        int natViewportHeight(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenGameViewport.valid ? gZenGameViewport.height : 720.0f);
            return 1;
        }

        int natGetFps(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenFps);
            return 1;
        }

        int natProfilerVisible(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_bool(gZenProfilerVisible);
            return 1;
        }

        Math::Vec2 screenToWorld(float x, float y)
        {
            if (!gZenGameCameraValid || !gZenGameViewport.valid)
                return Math::Vec2(x, y);
            return gZenGameCamera.ScreenToWorld(x, y, gZenGameViewport.width, gZenGameViewport.height);
        }

        int natScreenToWorld(zen::VM *, zen::Value *args, int nargs)
        {
            const float x = nargs >= 1 ? (float)zen::to_number(args[0]) : 0.0f;
            const float y = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
            const Math::Vec2 point = screenToWorld(x, y);
            args[0] = zen::val_float(point.x);
            args[1] = zen::val_float(point.y);
            return 2;
        }

        int natMouseWorldPosition(zen::VM *, zen::Value *args, int)
        {
            const float x = gZenInput ? gZenInput->MouseX() -
                                           (gZenGameViewport.valid ? gZenGameViewport.x : 0.0f)
                                     : 0.0f;
            const float y = gZenInput ? gZenInput->MouseY() -
                                           (gZenGameViewport.valid ? gZenGameViewport.y : 0.0f)
                                     : 0.0f;
            const Math::Vec2 point = screenToWorld(x, y);
            args[0] = zen::val_float(point.x);
            args[1] = zen::val_float(point.y);
            return 2;
        }

        int natWorldViewRect(zen::VM *, zen::Value *args, int)
        {
            const float width = gZenGameViewport.valid ? gZenGameViewport.width : 1280.0f;
            const float height = gZenGameViewport.valid ? gZenGameViewport.height : 720.0f;
            const Math::Vec2 corners[4] = {
                screenToWorld(0.0f, 0.0f), screenToWorld(width, 0.0f),
                screenToWorld(width, height), screenToWorld(0.0f, height)};
            float minX = corners[0].x;
            float minY = corners[0].y;
            float maxX = corners[0].x;
            float maxY = corners[0].y;
            for (int i = 1; i < 4; ++i)
            {
                if (corners[i].x < minX) minX = corners[i].x;
                if (corners[i].y < minY) minY = corners[i].y;
                if (corners[i].x > maxX) maxX = corners[i].x;
                if (corners[i].y > maxY) maxY = corners[i].y;
            }
            args[0] = zen::val_float(minX);
            args[1] = zen::val_float(minY);
            args[2] = zen::val_float(maxX);
            args[3] = zen::val_float(maxY);
            return 4;
        }

        int natWheelY(zen::VM *, zen::Value *args, int)
        {
            args[0] = zen::val_float(gZenInput ? gZenInput->WheelY() : 0.0);
            return 1;
        }
    }

    void ZenRuntime::Impl::initialize()
    {
        zen_host_set_writer(&zenHostWriter, nullptr);

        vm.open_lib_globals(&zen::zen_lib_base);
        vm.register_lib(&zen::zen_lib_math);
        vm.register_lib(&zen::zen_lib_time);
        vm.register_lib(&zen::zen_lib_json);
        vm.register_lib(&zen::zen_lib_net);
        vm.register_lib(&zen::zen_lib_http);

        vm.def_global("__k2d", zen::val_ptr(this));

        auto scriptComponent = vm.def_class("ScriptComponent");
        // Set to the host GameObject by ZenScriptComponent before user __init__ runs.
        scriptComponent.field("node");
        scriptComponent.constructable(false);
        scriptComponentClass = scriptComponent.end();

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
        node.method("spawn", &natNodeSpawn, -1);
        node.method("distance_to", &natNodeDistanceTo, 2);
        node.method("angle_to", &natNodeAngleTo, 2);
        node.method("look_at", &natNodeLookAt, 2);
        node.method("move_toward", &natNodeMoveToward, 3);
        node.method("get_body", &natNodeGetBody, 0);
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

        auto body = vm.def_class("Body");
        body.method("get_velocity", &natBodyGetVelocity, 0);
        body.method("set_velocity", &natBodySetVelocity, 2);
        body.method("get_angular_velocity", &natBodyGetAngularVelocity, 0);
        body.method("set_angular_velocity", &natBodySetAngularVelocity, 1);
        body.method("apply_force", &natBodyApplyForce, 2);
        body.method("apply_impulse", &natBodyApplyImpulse, 2);
        body.method("apply_torque", &natBodyApplyTorque, 1);
        body.method("get_gravity_scale", &natBodyGetGravityScale, 0);
        body.method("set_gravity_scale", &natBodySetGravityScale, 1);
        body.method("set_type", &natBodySetType, 1);
        body.method("is_awake", &natBodyIsAwake, 0);
        body.method("wake", &natBodyWake, 0);
        body.persistent(true).constructable(false);
        bodyClass = body.end();

        auto particle = vm.def_class("Particle");
        particle.method("start", &natParticleStart, 0);
        particle.method("stop", &natParticleStop, 0);
        particle.method("reset", &natParticleReset, 0);
        particle.method("burst", &natParticleBurst, 1);
        particle.method("is_playing", &natParticleIsPlaying, 0);
        particle.persistent(true).constructable(false);
        particleClass = particle.end();

        vm.def_native("get_number", &natGetNumber, -1);
        vm.def_native("set_number", &natSetNumber, 2);
        vm.def_native("get_string", &natGetString, -1);
        vm.def_native("set_string", &natSetString, 2);
        vm.def_native("get_flag", &natGetFlag, -1);
        vm.def_native("set_flag", &natSetFlag, 2);
        vm.def_native("has_key", &natHasKey, 1);
        vm.def_native("emit", &natEmit, -1);

        vm.def_native("key_down", &natKeyDown, 1);
        vm.def_native("key_pressed", &natKeyPressed, 1);
        vm.def_native("key_released", &natKeyReleased, 1);
        vm.def_native("mouse_down", &natMouseDown, 1);
        vm.def_native("mouse_pressed", &natMousePressed, 1);
        vm.def_native("mouse_x", &natMouseX, 0);
        vm.def_native("mouse_y", &natMouseY, 0);
        vm.def_native("wheel_y", &natWheelY, 0);
        vm.def_native("viewport_width", &natViewportWidth, 0);
        vm.def_native("viewport_height", &natViewportHeight, 0);
        vm.def_native("get_fps", &natGetFps, 0);
        vm.def_native("profiler_visible", &natProfilerVisible, 0);
        vm.def_native("screen_to_world", &natScreenToWorld, 2);
        vm.def_native("mouse_world_position", &natMouseWorldPosition, 0);
        vm.def_native("world_view_rect", &natWorldViewRect, 0);

        vm.def_native("set_draw_color", &natSetDrawColor, -1);
        vm.def_native("draw_line", &natDrawLine, -1);
        vm.def_native("draw_rect", &natDrawRect, -1);
        vm.def_native("draw_circle", &natDrawCircle, -1);
        vm.def_native("draw_text", &natDrawText, -1);
        vm.def_native("draw_text_width", &natDrawTextWidth, -1);
        vm.def_native("object_count", &natObjectCount, 0);

        vm.def_native("raycast", &natRaycast, -1);
        vm.def_native("body_at", &natBodyAt, 2);
        vm.def_native("set_gravity", &natSetGravity, 2);
        vm.def_native("get_gravity", &natGetGravity, 0);
    }

    ZenScriptComponent::ZenScriptComponent()
        : ScriptComponent(ComponentEventUpdate | ComponentEventRender), mState(new State())
    {
        ZenRuntime::instance().impl().liveInstances.push_back(&mState->instance);
    }

    ZenScriptComponent::~ZenScriptComponent()
    {
        destroyInstance();
        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        for (size_t i = 0; i < impl.liveInstances.size(); ++i)
        {
            if (impl.liveInstances[i] == &mState->instance)
            {
                impl.liveInstances[i] = impl.liveInstances.back();
                impl.liveInstances.pop_back();
                break;
            }
        }
        delete mState;
    }

    void ZenScriptComponent::destroyInstance()
    {
        if (!mState->scriptClass || zen::is_nil(mState->instance))
        {
            mState->instance = zen::val_nil();
            mState->started = false;
            return;
        }

        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        if (mState->started && mState->scriptClass->slotDestroy >= 0)
        {
            zen::ObjInstance *inst = zen::as_instance(mState->instance);
            if (inst && inst->klass && mState->scriptClass->slotDestroy < inst->klass->vtable_size &&
                !zen::is_nil(inst->klass->vtable[mState->scriptClass->slotDestroy]))
            {
                RunningScript running;
                impl.vm.invoke(mState->instance, mState->scriptClass->slotDestroy, nullptr, 0);
            }
        }
        mState->instance = zen::val_nil();
        mState->started = false;
    }

    bool ZenScriptComponent::loadSource(const char *source, const char *scriptName)
    {
        if (!source)
            return false;

        return loadFromSource(source, scriptName ? scriptName : "script");
    }

    bool ZenScriptComponent::loadFile(const char *path)
    {
        if (!path || !path[0])
            return false;

        ZenRuntime &runtime = ZenRuntime::instance();
        if (ZenScriptClass *cached = runtime.impl().findClass(path))
        {
            mScriptPath = path;
            mSourceTimestamp = cached->timestamp;
            destroyInstance();
            mState->scriptClass = cached;
            mState->classVersion = cached->version;
            mState->generation = runtime.generation();
            mState->loaded = true;
            mState->pending = false;
            return true;
        }

        if (scriptRunning())
        {
            destroyInstance();
            mScriptPath = path;
            mSourceTimestamp = fileTimestamp(path);
            mState->scriptClass = nullptr;
            mState->loaded = false;
            mState->pending = true;
            return true;
        }

        FileBuffer buffer;
        if (!FileSystem::Instance().LoadFile(path, buffer, true))
            return false;

        mScriptPath = path;
        mSourceTimestamp = fileTimestamp(path);
        return loadFromSource(buffer.Text(), path);
    }

    bool ZenScriptComponent::loadFromSource(const char *source, const char *path)
    {
        ZenRuntime &runtime = ZenRuntime::instance();
        ZenRuntime::Impl &impl = runtime.impl();

        destroyInstance();
        mState->scriptClass = nullptr;
        mState->loaded = false;
        mState->pending = false;

        ZenScriptClass compiled;
        if (!impl.compileClass(source, path, compiled))
            return false;

        ++runtime.mCompileCount;
        compiled.path = path;
        compiled.timestamp = mSourceTimestamp;
        mState->scriptClass = impl.addClass(compiled);
        mState->classVersion = mState->scriptClass->version;
        mState->generation = runtime.generation();
        mState->loaded = true;
        return true;
    }

    bool ZenScriptComponent::ensureInstance()
    {
        ZenRuntime &runtime = ZenRuntime::instance();

        if (mState->pending)
        {
            if (scriptRunning())
                return false;
            mState->pending = false;
            if (!loadFile(mScriptPath.c_str()))
                return false;
        }

        if (mState->generation != runtime.generation())
        {
            mState->scriptClass = nullptr;
            mState->instance = zen::val_nil();
            mState->self = zen::val_nil();
            mState->started = false;
            mState->loaded = false;
            if (!mScriptPath.empty() && !loadFile(mScriptPath.c_str()))
                return false;
            if (!mState->scriptClass)
                return false;
        }

        if (mState->scriptClass && mState->scriptClass->version != mState->classVersion)
        {
            destroyInstance();
            mState->classVersion = mState->scriptClass->version;
            mSourceTimestamp = mState->scriptClass->timestamp;
        }

        if (!mState->scriptClass || !owner())
            return false;

        ZenRuntime::Impl &impl = runtime.impl();
        if (zen::is_nil(mState->self))
            mState->self = impl.instanceFor(impl.nodeClass, owner());

        if (zen::is_nil(mState->instance))
        {
            zen::ObjClass *scriptClass = zen::as_class(mState->scriptClass->klass);
            const bool derivesFromScriptComponent = scriptClass &&
                                                    scriptClass->parent == impl.scriptComponentClass;
            mState->instance = impl.vm.make_instance(scriptClass);
            if (!zen::is_instance(mState->instance))
            {
                mState->instance = zen::val_nil();
                return false;
            }
            if (derivesFromScriptComponent)
            {
                zen::ObjInstance *instance = zen::as_instance(mState->instance);
                if (instance && instance->fields && instance->num_fields > 0)
                    instance->fields[0] = mState->self;
            }
            if (mState->scriptClass->slotInit >= 0)
            {
                RunningScript running;
                if (derivesFromScriptComponent)
                    impl.vm.invoke(mState->instance, mState->scriptClass->slotInit, nullptr, 0);
                else
                {
                    zen::Value args[1] = {mState->self};
                    impl.vm.invoke(mState->instance, mState->scriptClass->slotInit, args, 1);
                }
            }
            applyOverrides();
            mState->started = false;
        }
        return true;
    }

    bool ZenScriptComponent::callEvent(const char *event, double value)
    {
        ZenCallbackScope callbackScope(owner());
        if ((!mState->loaded && !mState->pending) || !ensureInstance() || !event)
            return false;

        ZenScriptClass *scriptClass = mState->scriptClass;
        if (scriptClass->slotEvent < 0)
            return false;

        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        zen::ObjInstance *inst = zen::as_instance(mState->instance);
        if (!inst || !inst->klass || scriptClass->slotEvent >= inst->klass->vtable_size ||
            zen::is_nil(inst->klass->vtable[scriptClass->slotEvent]))
            return false;

        zen::Value args[2] = {zen::val_obj((zen::Obj *)impl.vm.make_string(event)),
                              zen::val_float(value)};
        RunningScript running;
        impl.vm.invoke(mState->instance, scriptClass->slotEvent, args, 2);
        return !impl.vm.had_error();
    }

    bool ZenScriptComponent::callCollision(GameObject *other, bool began)
    {
        ZenCallbackScope callbackScope(owner());
        if ((!mState->loaded && !mState->pending) || !ensureInstance())
            return false;

        ZenScriptClass *scriptClass = mState->scriptClass;
        if (!scriptClass || scriptClass->slotCollision < 0)
            return false;

        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        zen::ObjInstance *inst = zen::as_instance(mState->instance);
        if (!inst || !inst->klass || scriptClass->slotCollision >= inst->klass->vtable_size ||
            zen::is_nil(inst->klass->vtable[scriptClass->slotCollision]))
            return false;

        zen::Value args[2] = {other ? impl.instanceFor(impl.nodeClass, other) : zen::val_nil(),
                              zen::val_bool(began)};
        RunningScript running;
        impl.vm.invoke(mState->instance, scriptClass->slotCollision, args, 2);
        return !impl.vm.had_error();
    }

    namespace
    {
        void routeCollision(const CollisionInfo &info, void *)
        {
            if (!gZenScriptsEnabled || !info.self)
                return;
            const size_t count = info.self->componentCount<ZenScriptComponent>();
            for (size_t i = 0; i < count; ++i)
                if (ZenScriptComponent *script = info.self->getComponentAt<ZenScriptComponent>(i))
                    script->callCollision(info.other, info.began);
        }
    }

    void RouteZenScriptCollisions(PhysicsWorld2D &world)
    {
        world.setCollisionCallback(&routeCollision, nullptr);
    }

    bool ZenScriptComponent::callFunction(const char *name, double value)
    {
        ZenCallbackScope callbackScope(owner());
        if ((!mState->loaded && !mState->pending) || !ensureInstance() || !name)
            return false;

        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        zen::Value args[1] = {zen::val_float(value)};
        RunningScript running;
        impl.vm.invoke(mState->instance, name, args, 1);
        return !impl.vm.had_error();
    }

    bool ZenScriptComponent::hasFunction(const char *name) const
    {
        if (!mState->loaded || !mState->scriptClass || !name)
            return false;

        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        const int slot = impl.vm.find_selector(name, (int)std::strlen(name));
        if (slot < 0)
            return false;

        zen::ObjClass *klass = zen::is_class(mState->scriptClass->klass)
                                   ? zen::as_class(mState->scriptClass->klass)
                                   : nullptr;
        return klass && slot < klass->vtable_size && !zen::is_nil(klass->vtable[slot]);
    }

    bool ZenScriptComponent::reloadIfChanged()
    {
        if (mScriptPath.empty())
            return false;
        const long long stamp = fileTimestamp(mScriptPath.c_str());
        if (stamp == 0 || stamp == mSourceTimestamp)
            return false;
        return ZenRuntime::instance().recompile(mScriptPath.c_str());
    }

    bool ZenScriptComponent::loaded() const
    {
        return mState->loaded;
    }

    bool ZenScriptComponent::pendingLoad() const
    {
        return mState->pending;
    }

    std::size_t ZenScriptComponent::declaredPropertyCount() const
    {
        return mState->scriptClass ? mState->scriptClass->properties.size() : 0;
    }

    const ZenScriptProperty *ZenScriptComponent::declaredPropertyAt(std::size_t index) const
    {
        if (!mState->scriptClass || index >= mState->scriptClass->properties.size())
            return nullptr;
        return &mState->scriptClass->properties[index];
    }

    const ZenScriptProperty *ZenScriptComponent::declaredProperty(const char *name) const
    {
        if (!mState->scriptClass || !name)
            return nullptr;
        const ct::Vector<ZenScriptProperty> &declared = mState->scriptClass->properties;
        for (size_t i = 0; i < declared.size(); ++i)
            if (declared[i].name == name)
                return &declared[i];
        return nullptr;
    }

    std::size_t ZenScriptComponent::overrideCount() const
    {
        return mOverrides.size();
    }

    const ZenScriptProperty *ZenScriptComponent::overrideAt(std::size_t index) const
    {
        return index < mOverrides.size() ? &mOverrides[index] : nullptr;
    }

    const ZenScriptProperty *ZenScriptComponent::findOverride(const char *name) const
    {
        if (!name)
            return nullptr;
        for (size_t i = 0; i < mOverrides.size(); ++i)
            if (mOverrides[i].name == name)
                return &mOverrides[i];
        return nullptr;
    }

    ZenScriptProperty &ZenScriptComponent::overrideSlot(const char *name)
    {
        for (size_t i = 0; i < mOverrides.size(); ++i)
            if (mOverrides[i].name == name)
                return mOverrides[i];

        ZenScriptProperty added;
        added.name = name;
        mOverrides.push_back(added);
        return mOverrides[mOverrides.size() - 1];
    }

    void ZenScriptComponent::setNumberOverride(const char *name, double value, bool integer)
    {
        if (!name || !name[0])
            return;
        ZenScriptProperty &prop = overrideSlot(name);
        prop.kind = ZenScriptProperty::Kind::Number;
        prop.number = value;
        prop.integer = integer;
        applyOverrides();
    }

    void ZenScriptComponent::setStringOverride(const char *name, const char *value)
    {
        if (!name || !name[0])
            return;
        ZenScriptProperty &prop = overrideSlot(name);
        prop.kind = ZenScriptProperty::Kind::String;
        prop.text = value ? value : "";
        applyOverrides();
    }

    void ZenScriptComponent::setBoolOverride(const char *name, bool value)
    {
        if (!name || !name[0])
            return;
        ZenScriptProperty &prop = overrideSlot(name);
        prop.kind = ZenScriptProperty::Kind::Bool;
        prop.flag = value;
        applyOverrides();
    }

    void ZenScriptComponent::clearOverride(const char *name)
    {
        if (!name)
            return;

        const ct::String key(name);
        bool removed = false;
        for (size_t i = 0; i < mOverrides.size(); ++i)
        {
            if (mOverrides[i].name == key)
            {
                mOverrides.erase(mOverrides.begin() + i);
                removed = true;
                break;
            }
        }
        if (!removed)
            return;

        if (const ZenScriptProperty *declared = declaredProperty(key.c_str()))
            writeProperty(*declared);
        else
            destroyInstance();
    }

    void ZenScriptComponent::clearOverrides()
    {
        if (mOverrides.empty())
            return;

        ct::Vector<ZenScriptProperty> cleared;
        for (size_t i = 0; i < mOverrides.size(); ++i)
            cleared.push_back(mOverrides[i]);
        mOverrides.clear();

        for (size_t i = 0; i < cleared.size(); ++i)
        {
            const ZenScriptProperty *declared = declaredProperty(cleared[i].name.c_str());
            if (declared)
                writeProperty(*declared);
            else
                destroyInstance();
        }
    }

    bool ZenScriptComponent::writeProperty(const ZenScriptProperty &prop)
    {
        if (zen::is_nil(mState->instance))
            return false;

        zen::ObjInstance *inst = zen::as_instance(mState->instance);
        if (!inst || !inst->klass || !inst->klass->field_names)
            return false;

        zen::ObjClass *klass = inst->klass;
        int field = -1;
        for (int f = 0; f < klass->num_fields; ++f)
        {
            if (klass->field_names[f] &&
                std::strcmp(klass->field_names[f]->chars, prop.name.c_str()) == 0)
            {
                field = f;
                break;
            }
        }
        if (field < 0 || field >= inst->num_fields)
            return false;

        switch (prop.kind)
        {
        case ZenScriptProperty::Kind::Number:
            inst->fields[field] =
                prop.integer ? zen::val_int((int64_t)prop.number) : zen::val_float(prop.number);
            break;
        case ZenScriptProperty::Kind::String:
            inst->fields[field] = zen::val_obj(
                (zen::Obj *)ZenRuntime::instance().impl().vm.make_string(prop.text.c_str()));
            break;
        case ZenScriptProperty::Kind::Bool:
            inst->fields[field] = zen::val_bool(prop.flag);
            break;
        }
        return true;
    }

    std::size_t ZenScriptComponent::applyOverrides()
    {
        std::size_t applied = 0;
        for (size_t i = 0; i < mOverrides.size(); ++i)
            if (writeProperty(mOverrides[i]))
                ++applied;
        return applied;
    }

    std::size_t ReloadChangedZenScripts()
    {
        return ZenRuntime::instance().refreshChangedFiles();
    }

    void ZenScriptComponent::onUpdate(float deltaTime)
    {
        ZenCallbackScope callbackScope(owner());
        if (!gZenScriptsEnabled || (!mState->loaded && !mState->pending) || !owner())
            return;
        if (!ensureInstance())
            return;

        ZenScriptClass *scriptClass = mState->scriptClass;
        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        zen::ObjInstance *inst = zen::as_instance(mState->instance);
        if (!inst || !inst->klass)
            return;

        if (!mState->started)
        {
            mState->started = true;
            if (scriptClass->slotStart >= 0 && scriptClass->slotStart < inst->klass->vtable_size &&
                !zen::is_nil(inst->klass->vtable[scriptClass->slotStart]))
            {
                RunningScript running(ScriptProfilePhase::Update);
                impl.vm.invoke(mState->instance, scriptClass->slotStart, nullptr, 0);
            }
        }

        if (scriptClass->slotUpdate >= 0 && scriptClass->slotUpdate < inst->klass->vtable_size &&
            !zen::is_nil(inst->klass->vtable[scriptClass->slotUpdate]))
        {
            zen::Value dt = zen::val_float(deltaTime);
            RunningScript running(ScriptProfilePhase::Update);
            impl.vm.invoke(mState->instance, scriptClass->slotUpdate, &dt, 1);
        }
    }

    void ZenScriptComponent::onRender(RenderQueue &queue)
    {
        ZenCallbackScope callbackScope(owner());
        if (!gZenScriptsEnabled || (!mState->loaded && !mState->pending) || !owner())
            return;
        if (!ensureInstance())
            return;

        ZenScriptClass *scriptClass = mState->scriptClass;
        ZenRuntime::Impl &impl = ZenRuntime::instance().impl();
        zen::ObjInstance *inst = zen::as_instance(mState->instance);
        if (!inst || !inst->klass)
            return;

        const auto invokeDraw = [&](int slot, int zIndex, bool screenSpace)
        {
            if (slot < 0 || slot >= inst->klass->vtable_size || zen::is_nil(inst->klass->vtable[slot]))
                return;
            ZenDrawContext context;
            context.queue = &queue;
            context.zIndex = zIndex;
            if (screenSpace)
            {
                const float width = gZenGameViewport.valid ? gZenGameViewport.width : 1280.0f;
                const float height = gZenGameViewport.valid ? gZenGameViewport.height : 720.0f;
                const Camera2D camera = gZenGameCameraValid ? gZenGameCamera : Camera2D();
                // The renderer keeps the world's camera projection. Cancel it for
                // the UI item, yielding stable top-left screen coordinates.
                context.xform = camera.CameraXform(width, height);
                context.hasXform = true;
            }
            ZenDrawContext *previousContext = gZenDrawContext;
            gZenDrawContext = &context;
            {
                RunningScript running(ScriptProfilePhase::Render);
                impl.vm.invoke(mState->instance, slot, nullptr, 0);
            }
            gZenDrawContext = previousContext;
        };

        invokeDraw(scriptClass->slotDraw, owner()->zIndex(), false);
        // UI commands form a second pass after every world-space item, so
        // counters/HUDs are never covered by sprites spawned later in a scene.
        invokeDraw(scriptClass->slotDrawUi, (std::numeric_limits<int>::max)(), true);
    }

    namespace
    {
        struct BlackboardEntry
        {
            ct::String key;
            double number = 0.0;
            ct::String text;
            bool flag = false;
            int kind = 0;
        };

        struct PendingEvent
        {
            ct::String name;
            double value = 0.0;
        };

        ct::Vector<BlackboardEntry> &blackboardEntries()
        {
            static ct::Vector<BlackboardEntry> entries;
            return entries;
        }

        ct::Vector<PendingEvent> &pendingEvents()
        {
            static ct::Vector<PendingEvent> events;
            return events;
        }

        ZenBlackboard::Handler gHostHandler = nullptr;
        void *gHostHandlerUser = nullptr;

        BlackboardEntry *findEntry(const char *key)
        {
            if (!key)
                return nullptr;
            ct::Vector<BlackboardEntry> &entries = blackboardEntries();
            for (size_t i = 0; i < entries.size(); ++i)
                if (entries[i].key == key)
                    return &entries[i];
            return nullptr;
        }

        BlackboardEntry &entryFor(const char *key)
        {
            if (BlackboardEntry *existing = findEntry(key))
                return *existing;
            BlackboardEntry entry;
            entry.key = key ? key : "";
            blackboardEntries().push_back(entry);
            return blackboardEntries().back();
        }
    }

    void ZenBlackboard::setNumber(const char *key, double value)
    {
        BlackboardEntry &entry = entryFor(key);
        entry.number = value;
        entry.kind = 0;
    }

    void ZenBlackboard::setString(const char *key, const char *value)
    {
        BlackboardEntry &entry = entryFor(key);
        entry.text = value ? value : "";
        entry.kind = 1;
    }

    void ZenBlackboard::setBool(const char *key, bool value)
    {
        BlackboardEntry &entry = entryFor(key);
        entry.flag = value;
        entry.kind = 2;
    }

    double ZenBlackboard::getNumber(const char *key, double fallback)
    {
        const BlackboardEntry *entry = findEntry(key);
        if (!entry)
            return fallback;
        if (entry->kind == 2)
            return entry->flag ? 1.0 : 0.0;
        return entry->kind == 0 ? entry->number : fallback;
    }

    ct::String ZenBlackboard::getString(const char *key, const char *fallback)
    {
        const BlackboardEntry *entry = findEntry(key);
        if (!entry || entry->kind != 1)
            return ct::String(fallback ? fallback : "");
        return entry->text;
    }

    bool ZenBlackboard::getBool(const char *key, bool fallback)
    {
        const BlackboardEntry *entry = findEntry(key);
        if (!entry)
            return fallback;
        if (entry->kind == 2)
            return entry->flag;
        return entry->kind == 0 ? entry->number != 0.0 : fallback;
    }

    bool ZenBlackboard::has(const char *key)
    {
        return findEntry(key) != nullptr;
    }

    void ZenBlackboard::remove(const char *key)
    {
        ct::Vector<BlackboardEntry> &entries = blackboardEntries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].key == key)
            {
                entries.erase(entries.begin() + i);
                return;
            }
        }
    }

    void ZenBlackboard::clear()
    {
        blackboardEntries().clear();
        pendingEvents().clear();
    }

    std::size_t ZenBlackboard::keyCount()
    {
        return blackboardEntries().size();
    }

    ct::String ZenBlackboard::keyAt(std::size_t index)
    {
        const ct::Vector<BlackboardEntry> &entries = blackboardEntries();
        return index < entries.size() ? entries[index].key : ct::String();
    }

    ZenBlackboard::Kind ZenBlackboard::kindOf(const char *key)
    {
        const BlackboardEntry *entry = findEntry(key);
        if (!entry)
            return Kind::Number;
        if (entry->kind == 1)
            return Kind::String;
        if (entry->kind == 2)
            return Kind::Bool;
        return Kind::Number;
    }

    void ZenBlackboard::emit(const char *event, double value)
    {
        if (!event || !event[0])
            return;
        if (gHostHandler)
            gHostHandler(event, value, gHostHandlerUser);
        PendingEvent pending;
        pending.name = event;
        pending.value = value;
        pendingEvents().push_back(pending);
    }

    std::size_t ZenBlackboard::pendingEventCount()
    {
        return pendingEvents().size();
    }

    void ZenBlackboard::clearEvents()
    {
        pendingEvents().clear();
    }

    void ZenBlackboard::setHostHandler(Handler handler, void *user)
    {
        gHostHandler = handler;
        gHostHandlerUser = user;
    }

    void BroadcastZenScriptEvent(GameObject &root, const char *event, double value)
    {
        const size_t count = root.componentCount<ZenScriptComponent>();
        for (size_t i = 0; i < count; ++i)
            if (ZenScriptComponent *script = root.getComponentAt<ZenScriptComponent>(i))
                if (script->active())
                    script->callEvent(event, value);

        for (size_t i = 0; i < root.childCount(); ++i)
            BroadcastZenScriptEvent(*root.child(i), event, value);
    }

    void DispatchZenScriptEvents(GameObject &root)
    {
        ct::Vector<PendingEvent> &events = pendingEvents();
        if (events.empty())
            return;

        ct::Vector<PendingEvent> batch;
        for (size_t i = 0; i < events.size(); ++i)
            batch.push_back(events[i]);
        events.clear();

        for (size_t i = 0; i < batch.size(); ++i)
            BroadcastZenScriptEvent(root, batch[i].name.c_str(), batch[i].value);
    }

    void SetZenScriptsEnabled(bool enabled)
    {
        gZenScriptsEnabled = enabled;
    }

    bool ZenScriptsEnabled()
    {
        return gZenScriptsEnabled;
    }

    void SetZenScriptInput(Input *input)
    {
        gZenInput = input;
    }

    void SetZenScriptGameViewport(float x, float y, float width, float height)
    {
        gZenGameViewport.x = x;
        gZenGameViewport.y = y;
        gZenGameViewport.width = width > 0.0f ? width : 0.0f;
        gZenGameViewport.height = height > 0.0f ? height : 0.0f;
        gZenGameViewport.valid = gZenGameViewport.width > 0.0f && gZenGameViewport.height > 0.0f;
    }

    void SetZenScriptGameCamera(const Camera2D *camera)
    {
        gZenGameCameraValid = camera != nullptr;
        if (camera)
            gZenGameCamera = *camera;
    }

    void SetZenScriptFrameStats(float, float fps)
    {
        gZenFps = fps > 0.0f ? fps : 0.0f;
    }

    void SetZenScriptProfilerVisible(bool visible)
    {
        gZenProfilerVisible = visible;
    }

    void SetZenScriptAssets(Assets *assets)
    {
        gZenAssets = assets;
    }

    void SetZenScriptOutput(void (*fn)(const char *text, bool isError, void *user), void *user)
    {
        gZenOutput = fn;
        gZenOutputUser = user;
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

            if (script.overrideCount() == 0)
                return;

            ct::Json properties = ct::Json::array();
            for (size_t i = 0; i < script.overrideCount(); ++i)
            {
                const ZenScriptProperty *prop = script.overrideAt(i);
                ct::Json entry = ct::Json::object();
                entry.set("name", ct::Json(prop->name.c_str()));
                switch (prop->kind)
                {
                case ZenScriptProperty::Kind::Number:
                    if (prop->integer)
                        entry.set("value", ct::Json((int64_t)prop->number));
                    else
                        entry.set("value", ct::Json(prop->number));
                    break;
                case ZenScriptProperty::Kind::String:
                    entry.set("value", ct::Json(prop->text.c_str()));
                    break;
                case ZenScriptProperty::Kind::Bool:
                    entry.set("value", ct::Json(prop->flag));
                    break;
                }
                properties.push_back(entry);
            }
            data.set("properties", properties);
        }

        void readZenScript(Component &component, const ct::Json &data, Assets *)
        {
            ZenScriptComponent &script = static_cast<ZenScriptComponent &>(component);
            const char *path = data["path"].as_cstr("");
            if (path[0])
                script.loadFile(path);

            const ct::Json &properties = data["properties"];
            if (!properties.is_array())
                return;

            for (size_t i = 0; i < properties.size(); ++i)
            {
                const ct::Json &entry = properties[i];
                const char *name = entry["name"].as_cstr("");
                if (!name[0])
                    continue;
                const ct::Json &value = entry["value"];
                if (value.is_bool())
                    script.setBoolOverride(name, value.as_bool());
                else if (value.is_string())
                    script.setStringOverride(name, value.as_cstr(""));
                else if (value.is_number())
                    script.setNumberOverride(name, value.as_double(), !value.is_real());
            }
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
