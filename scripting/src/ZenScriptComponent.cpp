#include "k2d/ZenScriptComponent.h"

#include "k2d/BoxCollider2D.h"
#include "k2d/ChainCollider2D.h"
#include "k2d/CharacterBody2D.h"
#include "k2d/CircleCollider2D.h"
#include "k2d/EdgeCollider2D.h"
#include "k2d/PolygonCollider2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/ZenRuntime.h"
#include "ZenRuntimeInternal.h"

#include <zen/bytecode.h>

#include "k2d/Animation2D.h"
#include "k2d/AStar2D.h"
#include "k2d/AStarGrid2D.h"
#include "k2d/AudioEngine.h"
#include "k2d/AudioPlayer.h"
#include "k2d/Assets.h"
#include "k2d/Bone2D.h"
#include "k2d/Camera2D.h"
#include "k2d/CameraComponent.h"
#include "k2d/CapsuleShape.h"
#include "k2d/CircleShape.h"
#include "k2d/DirectionalLight2D.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/GameObject.h"
#include "k2d/Input.h"
#include "k2d/InputActionMap.h"
#include "k2d/Light2D.h"
#include "k2d/LightOccluder2D.h"
#include "k2d/Line2D.h"
#include "k2d/MotionStreak2D.h"
#include "k2d/MotionTween2D.h"
#include "k2d/Navigation2D.h"
#include "k2d/Arrive2D.h"
#include "k2d/Flee2D.h"
#include "k2d/NavigationAgent2D.h"
#include "k2d/ObstacleAvoidance2D.h"
#include "k2d/Seek2D.h"
#include "k2d/Separation2D.h"
#include "k2d/Steering2D.h"
#include "k2d/Wander2D.h"
#include "k2d/NavigationRegion2D.h"
#include "k2d/NinePatchComponent.h"
#include "k2d/ParticleComponent.h"
#include "k2d/Polygon2D.h"
#include "k2d/RectShape.h"
#include "k2d/RenderQueue.h"
#include "k2d/Scene.h"
#include "k2d/ScreenFade.h"
#include "k2d/SceneManager.h"
#include "k2d/Skeleton2D.h"
#include "k2d/Serializer.h"
#include "k2d/SpriteComponent.h"
#include "k2d/SpriteBatch.h"
#include "k2d/TileMapComponent.h"
#include "k2d/UiControls.h"
#include "k2d/UserData.h"
#include "k2d/VirtualPad.h"

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
Input* gZenInput = nullptr;
VirtualPad* gZenVirtualPad = nullptr;
Assets* gZenAssets = nullptr;
UserData* gZenUserData = nullptr;
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
void (*gZenOutput)(const char* text, bool isError, void* user) = nullptr;
void* gZenOutputUser = nullptr;
GameObject* gZenCallbackNode = nullptr;

struct ZenCallbackScope
{
    explicit ZenCallbackScope(GameObject* node) : previous(gZenCallbackNode)
    {
        gZenCallbackNode = node;
    }
    ~ZenCallbackScope()
    {
        gZenCallbackNode = previous;
    }

    GameObject* previous;
};

struct ZenDrawContext
{
    RenderQueue* queue = nullptr;
    RenderItem* item = nullptr;
    int zIndex = 0;
    Color color = Color::White();
    Matrix2D xform;
    bool hasXform = false;

    RenderItem* ensureItem()
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

ZenDrawContext* gZenDrawContext = nullptr;

void zenHostWriter(const char* text, size_t length, int isError, void*)
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

long long fileTimestamp(const char* path)
{
    if (!path || !path[0])
        return 0;
    std::error_code error;
    const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, error);
    if (error)
        return 0;
    return (long long)time.time_since_epoch().count();
}

void preloadPrefabTextures(const ct::Json& node)
{
    if (!gZenAssets)
        return;
    if (node.is_object())
    {
        const ct::Json::Object& members = node.members();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const ct::String& key = members[i].key;
            const ct::Json& value = members[i].value;
            if ((key == "texture" || key == "normalMap") && value.is_string())
            {
                const char* path = value.as_cstr("");
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

int keyCode(zen::Value value)
{
    if (!zen::is_int(value) && !zen::is_float(value))
        return -1;
    const int code = static_cast<int>(zen::to_integer(value));
    return code >= 0 && code < Input::MAX_KEYS ? code : -1;
}

const char* valueToCString(zen::VM* vm, zen::Value v, char* smallBuffer, size_t smallSize)
{
    if (zen::is_small_string(v))
    {
        const int len = zen::small_string_len(v);
        const char* chars = zen::small_string_chars(v);
        size_t n = (size_t)len < smallSize - 1 ? (size_t)len : smallSize - 1;
        std::memcpy(smallBuffer, chars, n);
        smallBuffer[n] = '\0';
        return smallBuffer;
    }
    if (zen::is_obj(v) && v.as.obj && v.as.obj->type == zen::OBJ_STRING)
        return ((zen::ObjString*)v.as.obj)->chars;
    (void)vm;
    return "";
}

// A point list argument is an array of [x, y] pairs, mirroring the engine's
// own JSON convention for polygons (Serializer.cpp's ReadVec2/WriteVec2) —
// not a flat list of numbers.
bool unpackPointArray(zen::Value value, ct::Vector<Math::Vec2>& out)
{
    if (!zen::is_array(value))
        return false;
    zen::ObjArray* outer = zen::as_array(value);
    const int32_t count = arr_count(outer);
    out.clear();
    for (int32_t i = 0; i < count; ++i)
    {
        const zen::Value entry = zen::array_get(outer, i);
        if (!zen::is_array(entry))
            return false;
        zen::ObjArray* pair = zen::as_array(entry);
        if (arr_count(pair) < 2)
            return false;
        out.push_back(Math::Vec2((float)zen::to_number(zen::array_get(pair, 0)),
                                 (float)zen::to_number(zen::array_get(pair, 1))));
    }
    return true;
}

// Mirror of unpackPointArray's convention: an array of [x, y] pairs, not a
// flat list of numbers.
zen::Value packPointArray(zen::VM* vm, const ct::Vector<Math::Vec2>& points)
{
    zen::GC& gc = vm->get_gc();
    zen::ObjArray* outer = zen::new_array(&gc);
    for (size_t i = 0; i < points.size(); ++i)
    {
        zen::ObjArray* pair = zen::new_array(&gc);
        zen::array_push(&gc, pair, zen::val_float(points[i].x));
        zen::array_push(&gc, pair, zen::val_float(points[i].y));
        zen::array_push(&gc, outer, zen::val_obj((zen::Obj*)pair));
    }
    return zen::val_obj((zen::Obj*)outer);
}

float drawColorComponent(zen::Value value)
{
    const float component = (float)zen::to_number(value);
    return component < 0.0f ? 0.0f : (component > 1.0f ? 1.0f : component);
}

void addLineTriangles(ct::Vector<Math::Vec2>& triangles, float x0, float y0, float x1, float y1, float thickness)
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

int natSetDrawColor(zen::VM*, zen::Value* args, int nargs)
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

int natDrawLine(zen::VM*, zen::Value* args, int nargs)
{
    RenderItem* item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
    if (!item || nargs < 4)
        return 0;
    RenderCommand command;
    command.type = RenderCommand::kPolygon;
    command.color = gZenDrawContext->color;
    addLineTriangles(command.ownedPolygonPoints, (float)zen::to_number(args[0]), (float)zen::to_number(args[1]),
                     (float)zen::to_number(args[2]), (float)zen::to_number(args[3]),
                     nargs >= 5 ? (float)zen::to_number(args[4]) : 1.0f);
    if (!command.ownedPolygonPoints.empty())
        item->commands.push_back(command);
    return 0;
}

int natDrawRect(zen::VM*, zen::Value* args, int nargs)
{
    RenderItem* item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
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

int natDrawCircle(zen::VM*, zen::Value* args, int nargs)
{
    RenderItem* item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
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

int natDrawText(zen::VM* vm, zen::Value* args, int nargs)
{
    RenderItem* item = gZenDrawContext ? gZenDrawContext->ensureItem() : nullptr;
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

int natDrawTextWidth(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    const char* text = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const float size = nargs >= 2 ? (float)zen::to_number(args[1]) : 16.0f;
    unsigned int longest = 0;
    unsigned int length = 0;
    for (const char* character = text; *character; ++character)
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

int natObjectCount(zen::VM*, zen::Value* args, int)
{
    const Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    args[0] = zen::val_int(scene ? (int64_t)scene->objectCount() : 0);
    return 1;
}
} // namespace

struct ZenScriptComponent::State
{
    ZenScriptClass* scriptClass = nullptr;
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

    ZenRuntime::Impl& mImpl;
    ScriptProfilePhase mPhase;
    uint64_t mCounter;
    bool mProfiling;
};

bool scriptRunning()
{
    return ZenRuntime::instance().impl().executing > 0;
}
} // namespace

namespace
{
ZenRuntime::Impl* stateFromVM(zen::VM*)
{
    return &ZenRuntime::instance().impl();
}

GameObject* nodeFromSelf(zen::Value* args)
{
    return zen::zen_instance_data<GameObject>(args[-1]);
}

int natNodeGetName(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(node ? node->name().c_str() : ""));
    return 1;
}

int natNodeGetX(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_float(node ? node->position().x : 0.0);
    return 1;
}

int natNodeGetY(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_float(node ? node->position().y : 0.0);
    return 1;
}

int natNodeGetPosition(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_float(node ? node->position().x : 0.0);
    args[1] = zen::val_float(node ? node->position().y : 0.0);
    return 2;
}

int natNodeSetPosition(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 2)
        node->setPosition(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natNodeTranslate(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 2)
        node->translate(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natNodeGetRotation(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_float(node ? node->rotationDegrees() : 0.0);
    return 1;
}

int natNodeSetRotation(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
        node->setRotationDegrees((float)zen::to_number(args[0]));
    return 0;
}

int natNodeRotate(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
        node->rotate((float)zen::to_number(args[0]));
    return 0;
}

int natNodeGetScaleX(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_float(node ? node->scale().x : 1.0);
    return 1;
}

int natNodeGetScaleY(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_float(node ? node->scale().y : 1.0);
    return 1;
}

int natNodeSetScale(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 2)
        node->setScale(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natNodeSetVisible(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
        node->setVisible(zen::is_truthy(args[0]));
    return 0;
}

int natNodeIsVisible(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_bool(node ? node->visible() : false);
    return 1;
}

int natNodeSetActive(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
        node->setActive(zen::is_truthy(args[0]));
    return 0;
}

int natNodeIsActive(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_bool(node ? node->active() : false);
    return 1;
}

int natNodeSetZIndex(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
        node->setZIndex((int)zen::to_integer(args[0]));
    return 0;
}

int natNodeGetZIndex(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_int(node ? node->zIndex() : 0);
    return 1;
}

int natNodeQueueDestroy(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    if (node)
        node->dispose();
    return 0;
}

int natNodeGetParent(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    args[0] = (node && state) ? state->instanceFor(state->nodeClass, node->parent()) : zen::val_nil();
    return 1;
}

int natNodeChildCount(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_int(node ? (int64_t)node->childCount() : 0);
    return 1;
}

int natNodeGetChild(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    GameObject* child = nullptr;
    if (node && nargs >= 1)
    {
        const int64_t index = zen::to_integer(args[0]);
        if (index >= 0 && (size_t)index < node->childCount())
            child = node->child((size_t)index);
    }
    args[0] = (child && state) ? state->instanceFor(state->nodeClass, child) : zen::val_nil();
    return 1;
}

int natNodeFind(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    GameObject* found = nullptr;
    if (node && node->scene() && nargs >= 1)
    {
        char small[16];
        found = node->scene()->find(valueToCString(vm, args[0], small, sizeof(small)));
    }
    args[0] = (found && state) ? state->instanceFor(state->nodeClass, found) : zen::val_nil();
    return 1;
}

int natNodeCreateChild(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    GameObject* child = nullptr;
    if (node && node->scene() && nargs >= 1)
    {
        char small[16];
        child = node->scene()->createObject(valueToCString(vm, args[0], small, sizeof(small)), node);
    }
    args[0] = (child && state) ? state->instanceFor(state->nodeClass, child) : zen::val_nil();
    return 1;
}

int natNodeSpawn(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    GameObject* spawned = nullptr;
    if (node && node->scene() && nargs >= 1)
    {
        char small[16];
        const char* path = valueToCString(vm, args[0], small, sizeof(small));
        ct::String key(path);
        ZenRuntime::Impl::CachedPrefab* prefab = state ? state->prefabs.find(key) : nullptr;
        if (!prefab)
        {
            FileBuffer buffer;
            if (FileSystem::Instance().LoadFile(path, buffer, true))
            {
                ct::Json::Error err;
                ct::Json json = ct::Json::parse(buffer.Text(), &err);
                if (!err)
                {
                    ZenRuntime::Impl::CachedPrefab cached;
                    cached.data = ct::detail::move(json);
                    state->prefabs.put(key, ct::detail::move(cached));
                    prefab = state->prefabs.find(key);
                }
            }
        }
        if (prefab)
        {
            if (gZenAssets && !prefab->texturesPreloaded)
            {
                preloadPrefabTextures(prefab->data);
                prefab->texturesPreloaded = true;
            }
            spawned = Serializer::ReadObject(*node->scene(), prefab->data, nullptr, gZenAssets);
            if (spawned && nargs >= 3)
                spawned->setPosition(Math::Vec2((float)zen::to_number(args[1]), (float)zen::to_number(args[2])));
        }
    }
    args[0] = (spawned && state) ? state->instanceFor(state->nodeClass, spawned) : zen::val_nil();
    return 1;
}

int natNodeDistanceTo(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
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

int natNodeAngleTo(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    double degrees = 0.0;
    if (node && nargs >= 2)
    {
        const Math::Vec2 p = node->globalPosition();
        degrees = std::atan2(zen::to_number(args[1]) - p.y, zen::to_number(args[0]) - p.x) * 57.29577951308232;
    }
    args[0] = zen::val_float(degrees);
    return 1;
}

int natNodeLookAt(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 2)
    {
        const Math::Vec2 p = node->globalPosition();
        node->setRotationDegrees(
            (float)(std::atan2(zen::to_number(args[1]) - p.y, zen::to_number(args[0]) - p.x) * 57.29577951308232));
    }
    return 0;
}

int natNodeMoveToward(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
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
            node->setPosition(Math::Vec2(p.x + dx / length * maxDistance, p.y + dy / length * maxDistance));
    }
    const Math::Vec2 result = node ? node->position() : Math::Vec2(0.0f, 0.0f);
    args[0] = zen::val_float(result.x);
    args[1] = zen::val_float(result.y);
    return 2;
}

int natRaycast(zen::VM* vm, zen::Value* args, int nargs)
{
    ZenRuntime::Impl* state = stateFromVM(vm);
    GameObject* hit = nullptr;
    Math::Vec2 point(0.0f, 0.0f);
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    if (scene && nargs >= 5)
    {
        const Math::Vec2 origin((float)zen::to_number(args[0]), (float)zen::to_number(args[1]));
        const Math::Vec2 direction((float)zen::to_number(args[2]), (float)zen::to_number(args[3]));
        hit = scene->raycast(origin, direction, (float)zen::to_number(args[4]), &point);
    }
    args[0] = (hit && state) ? state->instanceFor(state->nodeClass, hit) : zen::val_nil();
    args[1] = zen::val_float(point.x);
    args[2] = zen::val_float(point.y);
    return 3;
}

int natBodyAt(zen::VM* vm, zen::Value* args, int nargs)
{
    ZenRuntime::Impl* state = stateFromVM(vm);
    GameObject* found = nullptr;
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    if (scene && nargs >= 2)
        found = scene->objectAtPoint(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = (found && state) ? state->instanceFor(state->nodeClass, found) : zen::val_nil();
    return 1;
}

int natSetGravity(zen::VM*, zen::Value* args, int nargs)
{
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    if (scene && nargs >= 2)
        scene->setGravity(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = zen::val_nil();
    return 1;
}

int natGetActiveCamera(zen::VM* vm, zen::Value* args, int)
{
    ZenRuntime::Impl* state = stateFromVM(vm);
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    CameraComponent* camera = scene ? scene->activeCamera() : nullptr;
    args[0] = (camera && state) ? state->instanceFor(state->cameraClass, camera) : zen::val_nil();
    return 1;
}

int natGetGravity(zen::VM*, zen::Value* args, int)
{
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    const Math::Vec2 gravity = scene ? scene->gravity() : Math::Vec2(0.0f, 0.0f);
    args[0] = zen::val_float(gravity.x);
    args[1] = zen::val_float(gravity.y);
    return 2;
}

RigidBody2D* bodyFromSelf(zen::Value* args)
{
    return zen::zen_instance_data<RigidBody2D>(args[-1]);
}

int natNodeGetBody(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    RigidBody2D* body = node ? node->getComponent<RigidBody2D>() : nullptr;
    args[0] = (body && state) ? state->instanceFor(state->rigidBodyClass, body) : zen::val_nil();
    return 1;
}

CharacterBody2D* characterFromSelf(zen::Value* args)
{
    GameObject* node = nodeFromSelf(args);
    return node ? node->getComponent<CharacterBody2D>() : nullptr;
}

int natNodePlaceFree(zen::VM*, zen::Value* args, int nargs)
{
    CharacterBody2D* character = characterFromSelf(args);
    args[0] = zen::val_bool(character && nargs >= 2 &&
                            character->placeFree((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 1;
}

int natNodePlaceMeeting(zen::VM* vm, zen::Value* args, int nargs)
{
    CharacterBody2D* character = characterFromSelf(args);
    GameObject* other = character && nargs >= 2
                            ? character->placeMeeting((float)zen::to_number(args[0]), (float)zen::to_number(args[1]))
                            : nullptr;
    ZenRuntime::Impl* state = stateFromVM(vm);
    args[0] = other && state ? state->instanceFor(state->nodeClass, other) : zen::val_nil();
    return 1;
}

int natNodeMoveAndCollide(zen::VM* vm, zen::Value* args, int nargs)
{
    CharacterBody2D* character = characterFromSelf(args);
    CollisionInfo collision;
    if (character && nargs >= 2)
        collision =
            character->moveAndCollide(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    ZenRuntime::Impl* state = stateFromVM(vm);
    args[0] = collision.other && state ? state->instanceFor(state->nodeClass, collision.other) : zen::val_nil();
    args[1] = zen::val_float(collision.point.x);
    args[2] = zen::val_float(collision.point.y);
    args[3] = zen::val_float(collision.normal.x);
    args[4] = zen::val_float(collision.normal.y);
    return 5;
}

int natNodeSetCharacterVelocity(zen::VM*, zen::Value* args, int nargs)
{
    if (CharacterBody2D* character = characterFromSelf(args))
        if (nargs >= 2)
            character->setVelocity(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natNodeGetCharacterVelocity(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = characterFromSelf(args);
    const Math::Vec2 velocity = character ? character->velocity() : Math::Vec2(0.0f, 0.0f);
    args[0] = zen::val_float(velocity.x);
    args[1] = zen::val_float(velocity.y);
    return 2;
}

int natNodeMoveAndSlide(zen::VM*, zen::Value* args, int nargs)
{
    CharacterBody2D* character = characterFromSelf(args);
    if (character && nargs >= 2)
        character->setVelocity(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    const bool hit = character && character->moveAndSlide();
    const Math::Vec2 velocity = character ? character->velocity() : Math::Vec2(0.0f, 0.0f);
    args[0] = zen::val_bool(hit);
    args[1] = zen::val_float(velocity.x);
    args[2] = zen::val_float(velocity.y);
    args[3] = zen::val_bool(character && character->isOnFloor());
    args[4] = zen::val_bool(character && character->isOnWall());
    args[5] = zen::val_bool(character && character->isOnCeiling());
    return 6;
}

int natNodeSlideCollision(zen::VM* vm, zen::Value* args, int nargs)
{
    CharacterBody2D* character = characterFromSelf(args);
    const CollisionInfo* collision =
        character && nargs >= 1 ? character->slideCollision((size_t)zen::to_integer(args[0])) : nullptr;
    ZenRuntime::Impl* state = stateFromVM(vm);
    args[0] = collision && collision->other && state ? state->instanceFor(state->nodeClass, collision->other)
                                                     : zen::val_nil();
    args[1] = zen::val_float(collision ? collision->point.x : 0.0f);
    args[2] = zen::val_float(collision ? collision->point.y : 0.0f);
    args[3] = zen::val_float(collision ? collision->normal.x : 0.0f);
    args[4] = zen::val_float(collision ? collision->normal.y : 0.0f);
    return 5;
}

int natNodeSlideCollisionCount(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = characterFromSelf(args);
    args[0] = zen::val_int(character ? (int64_t)character->slideCollisionCount() : 0);
    return 1;
}

int natBodyGetVelocity(zen::VM*, zen::Value* args, int)
{
    RigidBody2D* body = bodyFromSelf(args);
    const Math::Vec2 velocity = body ? body->velocity() : Math::Vec2(0.0f, 0.0f);
    args[0] = zen::val_float(velocity.x);
    args[1] = zen::val_float(velocity.y);
    return 2;
}

int natBodySetVelocity(zen::VM*, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 2)
        body->setVelocity(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = zen::val_nil();
    return 1;
}

int natBodyGetAngularVelocity(zen::VM*, zen::Value* args, int)
{
    RigidBody2D* body = bodyFromSelf(args);
    args[0] = zen::val_float(body ? body->angularVelocity() : 0.0);
    return 1;
}

int natBodySetAngularVelocity(zen::VM*, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 1)
        body->setAngularVelocity((float)zen::to_number(args[0]));
    args[0] = zen::val_nil();
    return 1;
}

int natBodyApplyForce(zen::VM*, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 2)
        body->applyForce(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = zen::val_nil();
    return 1;
}

int natBodyApplyImpulse(zen::VM*, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 2)
        body->applyImpulse(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = zen::val_nil();
    return 1;
}

int natBodyApplyTorque(zen::VM*, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 1)
        body->applyTorque((float)zen::to_number(args[0]));
    args[0] = zen::val_nil();
    return 1;
}

int natBodySetGravityScale(zen::VM*, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 1)
        body->setGravityScale((float)zen::to_number(args[0]));
    args[0] = zen::val_nil();
    return 1;
}

int natBodyGetGravityScale(zen::VM*, zen::Value* args, int)
{
    RigidBody2D* body = bodyFromSelf(args);
    args[0] = zen::val_float(body ? body->gravityScale() : 0.0);
    return 1;
}

int natBodyWake(zen::VM*, zen::Value* args, int)
{
    if (RigidBody2D* body = bodyFromSelf(args))
        body->wake();
    args[0] = zen::val_nil();
    return 1;
}

int natBodySetType(zen::VM* vm, zen::Value* args, int nargs)
{
    RigidBody2D* body = bodyFromSelf(args);
    if (body && nargs >= 1)
    {
        char small[16];
        const char* name = valueToCString(vm, args[0], small, sizeof(small));
        if (std::strcmp(name, "static") == 0)
            body->setBodyType(k2d::BodyType::Static);
        else if (std::strcmp(name, "kinematic") == 0)
            body->setBodyType(k2d::BodyType::Kinematic);
        else
            body->setBodyType(k2d::BodyType::Dynamic);
    }
    args[0] = zen::val_nil();
    return 1;
}

int natBodyIsAwake(zen::VM*, zen::Value* args, int)
{
    RigidBody2D* body = bodyFromSelf(args);
    args[0] = zen::val_bool(body && body->inWorld() && body->IsAwake());
    return 1;
}

int natNodeGetSprite(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    SpriteComponent* sprite = node ? node->getComponent<SpriteComponent>() : nullptr;
    args[0] = (sprite && state) ? state->instanceFor(state->spriteClass, sprite) : zen::val_nil();
    return 1;
}

int natNodeGetAnimation(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    Animation2D* animation = node ? node->getComponent<Animation2D>() : nullptr;
    args[0] = (animation && state) ? state->instanceFor(state->animationClass, animation) : zen::val_nil();
    return 1;
}

int natNodeGetCamera(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    CameraComponent* camera = node ? node->getComponent<CameraComponent>() : nullptr;
    args[0] = (camera && state) ? state->instanceFor(state->cameraClass, camera) : zen::val_nil();
    return 1;
}

CameraComponent* cameraFromSelf(zen::Value* args)
{
    return zen::zen_instance_data<CameraComponent>(args[-1]);
}

int natCameraStartShake(zen::VM*, zen::Value* args, int nargs)
{
    if (CameraComponent* component = cameraFromSelf(args))
        if (nargs >= 4)
            component->camera().startShake((float)zen::to_number(args[0]), (float)zen::to_number(args[1]),
                                           (float)zen::to_number(args[2]), (float)zen::to_number(args[3]));
    return 0;
}

int natCameraStopShake(zen::VM*, zen::Value* args, int)
{
    if (CameraComponent* component = cameraFromSelf(args))
        component->camera().stopShake();
    return 0;
}

int natCameraAddTrauma(zen::VM*, zen::Value* args, int nargs)
{
    if (CameraComponent* component = cameraFromSelf(args))
        if (nargs >= 1)
            component->camera().addTrauma((float)zen::to_number(args[0]));
    return 0;
}

int natCameraSetTraumaProfile(zen::VM*, zen::Value* args, int nargs)
{
    if (CameraComponent* component = cameraFromSelf(args))
        if (nargs >= 4)
            component->camera().setTraumaProfile((float)zen::to_number(args[0]), (float)zen::to_number(args[1]),
                                                 (float)zen::to_number(args[2]), (float)zen::to_number(args[3]));
    return 0;
}

int natCameraClearTrauma(zen::VM*, zen::Value* args, int)
{
    if (CameraComponent* component = cameraFromSelf(args))
        component->camera().clearTrauma();
    return 0;
}

int natCameraStartZoomPunch(zen::VM*, zen::Value* args, int nargs)
{
    if (CameraComponent* component = cameraFromSelf(args))
        if (nargs >= 2)
            component->camera().startZoomPunch((float)zen::to_number(args[0]), (float)zen::to_number(args[1]));
    return 0;
}

int natCameraStopZoomPunch(zen::VM*, zen::Value* args, int)
{
    if (CameraComponent* component = cameraFromSelf(args))
        component->camera().stopZoomPunch();
    return 0;
}

int natCameraIsShaking(zen::VM*, zen::Value* args, int)
{
    CameraComponent* component = cameraFromSelf(args);
    args[0] = zen::val_bool(component && component->camera().isShaking());
    return 1;
}

int natNodeGetParticle(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    ParticleComponent* particle = node ? node->getComponent<ParticleComponent>() : nullptr;
    args[0] = (particle && state) ? state->instanceFor(state->particleClass, particle) : zen::val_nil();
    return 1;
}

int natNodeGetButton(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    UiButton* button = node ? node->getComponent<UiButton>() : nullptr;
    args[0] = (button && state) ? state->instanceFor(state->buttonClass, button) : zen::val_nil();
    return 1;
}

int natNodeGetCheckBox(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    UiCheckBox* check = node ? node->getComponent<UiCheckBox>() : nullptr;
    args[0] = (check && state) ? state->instanceFor(state->checkBoxClass, check) : zen::val_nil();
    return 1;
}

int natNodeGetSlider(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    UiSlider* slider = node ? node->getComponent<UiSlider>() : nullptr;
    args[0] = (slider && state) ? state->instanceFor(state->sliderClass, slider) : zen::val_nil();
    return 1;
}

Component* componentFromSelf(zen::Value* args)
{
    return zen::zen_instance_data<Component>(args[-1]);
}

int natComponentIsActive(zen::VM*, zen::Value* args, int)
{
    Component* component = componentFromSelf(args);
    args[0] = zen::val_bool(component && component->active());
    return 1;
}

int natComponentSetActive(zen::VM*, zen::Value* args, int nargs)
{
    if (Component* component = componentFromSelf(args))
        if (nargs >= 1)
            component->setActive(zen::is_truthy(args[0]));
    return 0;
}

int natCharacterGetVelocity(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = componentFromSelf(args) ?
        static_cast<CharacterBody2D*>(componentFromSelf(args)) : nullptr;
    const Math::Vec2 velocity = character ? character->velocity() : Math::Vec2(0.0f);
    args[0] = zen::val_float(velocity.x);
    args[1] = zen::val_float(velocity.y);
    return 2;
}

int natCharacterSetVelocity(zen::VM*, zen::Value* args, int nargs)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    if (character && nargs >= 2)
        character->setVelocity(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natCharacterGetSafeMargin(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    args[0] = zen::val_float(character ? character->safeMargin() : 0.0f);
    return 1;
}

int natCharacterSetSafeMargin(zen::VM*, zen::Value* args, int nargs)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    if (character && nargs >= 1)
        character->setSafeMargin((float)zen::to_number(args[0]));
    return 0;
}

int natCharacterGetMaxSlides(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    args[0] = zen::val_int(character ? character->maxSlides() : 0);
    return 1;
}

int natCharacterSetMaxSlides(zen::VM*, zen::Value* args, int nargs)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    if (character && nargs >= 1)
        character->setMaxSlides((int)zen::to_integer(args[0]));
    return 0;
}

int natCharacterIsOnFloor(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    args[0] = zen::val_bool(character && character->isOnFloor());
    return 1;
}

int natCharacterIsOnWall(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    args[0] = zen::val_bool(character && character->isOnWall());
    return 1;
}

int natCharacterIsOnCeiling(zen::VM*, zen::Value* args, int)
{
    CharacterBody2D* character = static_cast<CharacterBody2D*>(componentFromSelf(args));
    args[0] = zen::val_bool(character && character->isOnCeiling());
    return 1;
}

int natColliderGetOffset(zen::VM*, zen::Value* args, int)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    const Math::Vec2 offset = collider ? collider->offset() : Math::Vec2(0.0f);
    args[0] = zen::val_float(offset.x);
    args[1] = zen::val_float(offset.y);
    return 2;
}

int natColliderSetOffset(zen::VM*, zen::Value* args, int nargs)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    if (collider && nargs >= 2)
        collider->setOffset(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natColliderIsSensor(zen::VM*, zen::Value* args, int)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    args[0] = zen::val_bool(collider && collider->isSensor());
    return 1;
}

int natColliderSetSensor(zen::VM*, zen::Value* args, int nargs)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    if (collider && nargs >= 1)
        collider->setSensor(zen::is_truthy(args[0]));
    return 0;
}

int natColliderGetFilter(zen::VM*, zen::Value* args, int)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    args[0] = zen::val_int(collider ? collider->category() : 0);
    args[1] = zen::val_int(collider ? collider->mask() : 0);
    return 2;
}

int natColliderSetFilter(zen::VM*, zen::Value* args, int nargs)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    if (collider && nargs >= 2)
        collider->setFilter((uint16_t)zen::to_integer(args[0]), (uint16_t)zen::to_integer(args[1]));
    return 0;
}

int natColliderShapeCount(zen::VM*, zen::Value* args, int)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    args[0] = zen::val_int(collider ? collider->shapeCount() : 0);
    return 1;
}

int natColliderAttached(zen::VM*, zen::Value* args, int)
{
    Collider2D* collider = static_cast<Collider2D*>(componentFromSelf(args));
    args[0] = zen::val_bool(collider && collider->attached());
    return 1;
}

int natUiPanelSetColor(zen::VM*, zen::Value* args, int nargs)
{
    UiPanel* panel = static_cast<UiPanel*>(componentFromSelf(args));
    if (panel && nargs >= 3)
    {
        const unsigned char r = (unsigned char)zen::to_integer(args[0]);
        const unsigned char g = (unsigned char)zen::to_integer(args[1]);
        const unsigned char b = (unsigned char)zen::to_integer(args[2]);
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        panel->setColor(Color(r, g, b, a));
    }
    return 0;
}

int natUiLabelSetText(zen::VM* vm, zen::Value* args, int nargs)
{
    UiLabel* label = static_cast<UiLabel*>(componentFromSelf(args));
    if (label && nargs >= 1)
    {
        char small[256];
        label->setText(valueToCString(vm, args[0], small, sizeof(small)));
    }
    return 0;
}

int natUiLabelGetText(zen::VM* vm, zen::Value* args, int)
{
    UiLabel* label = static_cast<UiLabel*>(componentFromSelf(args));
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(label ? label->text().c_str() : ""));
    return 1;
}

int natUiLabelSetFontSize(zen::VM*, zen::Value* args, int nargs)
{
    UiLabel* label = static_cast<UiLabel*>(componentFromSelf(args));
    if (label && nargs >= 1)
        label->setFontSize((float)zen::to_number(args[0]));
    return 0;
}

int natUiLabelGetFontSize(zen::VM*, zen::Value* args, int)
{
    UiLabel* label = static_cast<UiLabel*>(componentFromSelf(args));
    args[0] = zen::val_float(label ? label->fontSize() : 0.0f);
    return 1;
}

int natSkeletonPlay(zen::VM* vm, zen::Value* args, int nargs)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    bool ok = false;
    if (skeleton && nargs >= 1)
    {
        char small[16];
        const char* name = valueToCString(vm, args[0], small, sizeof(small));
        const bool loop = nargs >= 2 ? zen::is_truthy(args[1]) : true;
        const float speed = nargs >= 3 ? (float)zen::to_number(args[2]) : 1.0f;
        ok = skeleton->play(name, loop, speed);
    }
    args[0] = zen::val_bool(ok);
    return 1;
}

int natSkeletonStop(zen::VM*, zen::Value* args, int)
{
    if (Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]))
        skeleton->stop();
    return 0;
}

int natSkeletonPause(zen::VM*, zen::Value* args, int)
{
    if (Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]))
        skeleton->pause();
    return 0;
}

int natSkeletonResume(zen::VM*, zen::Value* args, int)
{
    if (Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]))
        skeleton->resume();
    return 0;
}

int natSkeletonSeek(zen::VM*, zen::Value* args, int nargs)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    if (skeleton && nargs >= 1)
        skeleton->seek((float)zen::to_number(args[0]));
    return 0;
}

int natSkeletonIsPlaying(zen::VM*, zen::Value* args, int)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    args[0] = zen::val_bool(skeleton && skeleton->playing());
    return 1;
}

int natSkeletonCurrent(zen::VM* vm, zen::Value* args, int)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    const char* name = skeleton ? skeleton->currentAnimation() : "";
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(name ? name : ""));
    return 1;
}

int natSkeletonGetTime(zen::VM*, zen::Value* args, int)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    args[0] = zen::val_float(skeleton ? skeleton->currentTime() : 0.0f);
    return 1;
}

int natSkeletonGetSpeed(zen::VM*, zen::Value* args, int)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    args[0] = zen::val_float(skeleton ? skeleton->speed() : 0.0f);
    return 1;
}

int natSkeletonSetSpeed(zen::VM*, zen::Value* args, int nargs)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    if (skeleton && nargs >= 1)
        skeleton->setSpeed((float)zen::to_number(args[0]));
    return 0;
}

int natSkeletonClipCount(zen::VM*, zen::Value* args, int)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    args[0] = zen::val_int(skeleton ? (int64_t)skeleton->clipCount() : 0);
    return 1;
}

int natSkeletonFindBone(zen::VM* vm, zen::Value* args, int nargs)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    ZenRuntime::Impl* state = stateFromVM(vm);
    Bone2D* bone = nullptr;
    if (skeleton && state && nargs >= 1)
    {
        char small[16];
        bone = skeleton->findBone(valueToCString(vm, args[0], small, sizeof(small)));
    }
    args[0] = bone ? state->instanceFor(state->boneClass, bone) : zen::val_nil();
    return 1;
}

int natSkeletonResetToRest(zen::VM*, zen::Value* args, int)
{
    if (Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]))
        skeleton->resetToRest();
    return 0;
}

int natSkeletonSolveIK(zen::VM* vm, zen::Value* args, int nargs)
{
    Skeleton2D* skeleton = zen::zen_instance_data<Skeleton2D>(args[-1]);
    bool ok = false;
    if (skeleton && nargs >= 3)
    {
        char small[16];
        const char* effector = valueToCString(vm, args[0], small, sizeof(small));
        const Math::Vec2 target((float)zen::to_number(args[1]), (float)zen::to_number(args[2]));
        const int chain = nargs >= 4 ? (int)zen::to_number(args[3]) : 0;
        const int iterations = nargs >= 5 ? (int)zen::to_number(args[4]) : 8;
        const float tolerance = nargs >= 6 ? (float)zen::to_number(args[5]) : 0.5f;
        ok = skeleton->solveIK(effector, target, chain, iterations, tolerance);
    }
    args[0] = zen::val_bool(ok);
    return 1;
}

int natBoneGetLength(zen::VM*, zen::Value* args, int)
{
    Bone2D* bone = zen::zen_instance_data<Bone2D>(args[-1]);
    args[0] = zen::val_float(bone ? bone->length() : 0.0f);
    return 1;
}

int natBoneSetLength(zen::VM*, zen::Value* args, int nargs)
{
    Bone2D* bone = zen::zen_instance_data<Bone2D>(args[-1]);
    if (bone && nargs >= 1)
        bone->setLength((float)zen::to_number(args[0]));
    return 0;
}

int natBoneGetRestPosition(zen::VM*, zen::Value* args, int)
{
    Bone2D* bone = zen::zen_instance_data<Bone2D>(args[-1]);
    args[0] = zen::val_float(bone ? bone->restPosition().x : 0.0f);
    args[1] = zen::val_float(bone ? bone->restPosition().y : 0.0f);
    return 2;
}

int natBoneGetRestRotation(zen::VM*, zen::Value* args, int)
{
    Bone2D* bone = zen::zen_instance_data<Bone2D>(args[-1]);
    args[0] = zen::val_float(bone ? bone->restRotationDegrees() : 0.0f);
    return 1;
}

int natBoneSaveRestPose(zen::VM*, zen::Value* args, int)
{
    if (Bone2D* bone = zen::zen_instance_data<Bone2D>(args[-1]))
        bone->saveRestPose();
    return 0;
}

int natBoneResetToRest(zen::VM*, zen::Value* args, int)
{
    if (Bone2D* bone = zen::zen_instance_data<Bone2D>(args[-1]))
        bone->resetToRest();
    return 0;
}

int natAudioPlayerPlay(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_int(player ? (int64_t)player->play() : 0);
    return 1;
}

int natAudioPlayerStop(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_bool(player && player->stop());
    return 1;
}

int natAudioPlayerPause(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_bool(player && player->pause());
    return 1;
}

int natAudioPlayerResume(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_bool(player && player->resume());
    return 1;
}

int natAudioPlayerIsPlaying(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_bool(player && player->playing());
    return 1;
}

int natAudioPlayerGetVolume(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_float(player ? player->volume() : 0.0f);
    return 1;
}

int natAudioPlayerSetVolume(zen::VM*, zen::Value* args, int nargs)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    if (player && nargs >= 1)
        player->setVolume((float)zen::to_number(args[0]));
    return 0;
}

int natAudioPlayerGetLoop(zen::VM*, zen::Value* args, int)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    args[0] = zen::val_bool(player && player->loop());
    return 1;
}

int natAudioPlayerSetLoop(zen::VM*, zen::Value* args, int nargs)
{
    AudioPlayer* player = zen::zen_instance_data<AudioPlayer>(args[-1]);
    if (player && nargs >= 1)
        player->setLoop(zen::is_truthy(args[0]));
    return 0;
}

int natLight2DGetColor(zen::VM*, zen::Value* args, int)
{
    Light2D* light = zen::zen_instance_data<Light2D>(args[-1]);
    const Color color = light ? light->color() : Color();
    args[0] = zen::val_float(color.r);
    args[1] = zen::val_float(color.g);
    args[2] = zen::val_float(color.b);
    args[3] = zen::val_float(color.a);
    return 4;
}

int natLight2DSetColor(zen::VM*, zen::Value* args, int nargs)
{
    Light2D* light = zen::zen_instance_data<Light2D>(args[-1]);
    if (light && nargs >= 3)
    {
        const float a = nargs >= 4 ? (float)zen::to_number(args[3]) : 1.0f;
        light->setColor((float)zen::to_number(args[0]), (float)zen::to_number(args[1]),
                        (float)zen::to_number(args[2]), a);
    }
    return 0;
}

int natLight2DGetEnergy(zen::VM*, zen::Value* args, int)
{
    Light2D* light = zen::zen_instance_data<Light2D>(args[-1]);
    args[0] = zen::val_float(light ? light->energy() : 0.0f);
    return 1;
}

int natLight2DSetEnergy(zen::VM*, zen::Value* args, int nargs)
{
    Light2D* light = zen::zen_instance_data<Light2D>(args[-1]);
    if (light && nargs >= 1)
        light->setEnergy((float)zen::to_number(args[0]));
    return 0;
}

int natLight2DGetRadius(zen::VM*, zen::Value* args, int)
{
    Light2D* light = zen::zen_instance_data<Light2D>(args[-1]);
    args[0] = zen::val_float(light ? light->radius() : 0.0f);
    return 1;
}

int natLight2DSetRadius(zen::VM*, zen::Value* args, int nargs)
{
    Light2D* light = zen::zen_instance_data<Light2D>(args[-1]);
    if (light && nargs >= 1)
        light->setRadius((float)zen::to_number(args[0]));
    return 0;
}

int natTileMapGetTile(zen::VM*, zen::Value* args, int nargs)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    int tile = 0;
    if (tileMap && nargs >= 2)
        tile = tileMap->getTile((int)zen::to_number(args[0]), (int)zen::to_number(args[1]));
    args[0] = zen::val_int(tile);
    return 1;
}

int natTileMapSetTile(zen::VM*, zen::Value* args, int nargs)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    if (tileMap && nargs >= 3)
        tileMap->setTile((int)zen::to_number(args[0]), (int)zen::to_number(args[1]), (int)zen::to_number(args[2]));
    return 0;
}

int natTileMapHasCollision(zen::VM*, zen::Value* args, int nargs)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    bool solid = false;
    if (tileMap && nargs >= 2)
        solid = tileMap->hasCollision((int)zen::to_number(args[0]), (int)zen::to_number(args[1]));
    args[0] = zen::val_bool(solid);
    return 1;
}

int natTileMapSetCollision(zen::VM*, zen::Value* args, int nargs)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    if (tileMap && nargs >= 3)
        tileMap->setCollision((int)zen::to_number(args[0]), (int)zen::to_number(args[1]), zen::is_truthy(args[2]));
    return 0;
}

int natTileMapGetColumns(zen::VM*, zen::Value* args, int)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    args[0] = zen::val_int(tileMap ? tileMap->columns() : 0);
    return 1;
}

int natTileMapGetRows(zen::VM*, zen::Value* args, int)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    args[0] = zen::val_int(tileMap ? tileMap->rows() : 0);
    return 1;
}

int natTileMapGetCellSize(zen::VM*, zen::Value* args, int)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    args[0] = zen::val_float(tileMap ? tileMap->cellWidth() : 0.0f);
    args[1] = zen::val_float(tileMap ? tileMap->cellHeight() : 0.0f);
    return 2;
}

// Cell coordinates are in the tilemap's own local grid; the node's full
// transform (position, rotation, scale) carries them into world space, the
// same math TileMapComponent::onRender uses to place each tile.
int natTileMapWorldToCell(zen::VM*, zen::Value* args, int nargs)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    int cellX = 0;
    int cellY = 0;
    if (tileMap && tileMap->owner() && nargs >= 2 && tileMap->cellWidth() != 0.0f && tileMap->cellHeight() != 0.0f)
    {
        const Math::Vec2 local = tileMap->owner()->globalTransform().AffineInverse().Transform(
            (float)zen::to_number(args[0]), (float)zen::to_number(args[1]));
        cellX = (int)std::floor(local.x / tileMap->cellWidth());
        cellY = (int)std::floor(local.y / tileMap->cellHeight());
    }
    args[0] = zen::val_int(cellX);
    args[1] = zen::val_int(cellY);
    return 2;
}

int natTileMapCellToWorld(zen::VM*, zen::Value* args, int nargs)
{
    TileMapComponent* tileMap = zen::zen_instance_data<TileMapComponent>(args[-1]);
    Math::Vec2 world(0.0f, 0.0f);
    if (tileMap && tileMap->owner() && nargs >= 2)
    {
        const Math::Vec2 local((float)zen::to_number(args[0]) * tileMap->cellWidth(),
                               (float)zen::to_number(args[1]) * tileMap->cellHeight());
        world = tileMap->owner()->globalTransform().Transform(local);
    }
    args[0] = zen::val_float(world.x);
    args[1] = zen::val_float(world.y);
    return 2;
}

int natNavAgentSetTarget(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    bool ok = false;
    if (agent && nargs >= 2)
        ok = agent->setTargetPosition(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = zen::val_bool(ok);
    return 1;
}

int natNavAgentFollowPosition(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    Math::Vec2 position(0.0f, 0.0f);
    const bool resolved = agent && agent->followPosition(position);
    args[0] = zen::val_bool(resolved);
    args[1] = zen::val_float(position.x);
    args[2] = zen::val_float(position.y);
    return 3;
}

int natNavAgentGetTarget(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    const Math::Vec2 target = agent ? agent->targetPosition() : Math::Vec2(0.0f);
    args[0] = zen::val_float(target.x);
    args[1] = zen::val_float(target.y);
    return 2;
}

int natNavAgentRepath(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_bool(agent && agent->repath());
    return 1;
}

int natNavAgentSetFollowTarget(zen::VM* vm, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    if (agent && nargs >= 1)
    {
        char small[64];
        agent->setFollowTargetName(valueToCString(vm, args[0], small, sizeof(small)));
    }
    return 0;
}

int natNavAgentClearFollowTarget(zen::VM*, zen::Value* args, int)
{
    if (NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]))
        agent->setFollowTargetName(nullptr);
    return 0;
}

int natNavAgentClearPath(zen::VM*, zen::Value* args, int)
{
    if (NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]))
        agent->clearPath();
    return 0;
}

int natNavAgentHasPath(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_bool(agent && agent->hasPath());
    return 1;
}

int natNavAgentIsFinished(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_bool(!agent || agent->isNavigationFinished());
    return 1;
}

int natNavAgentNextPosition(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    const Math::Vec2 next = agent ? agent->nextPathPosition() : Math::Vec2(0.0f);
    args[0] = zen::val_float(next.x);
    args[1] = zen::val_float(next.y);
    return 2;
}

int natNavAgentAdvance(zen::VM*, zen::Value* args, int)
{
    if (NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]))
        agent->advance();
    return 0;
}

int natNavAgentPathCount(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_int(agent ? (int64_t)agent->path().size() : 0);
    return 1;
}

int natNavAgentPathPoint(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (agent && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& path = agent->path();
        if (index >= 0 && (std::size_t)index < path.size())
            point = path[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natNavAgentGetMaxSpeed(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_float(agent ? agent->maxSpeed() : 0.0f);
    return 1;
}

int natNavAgentSetMaxSpeed(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    if (agent && nargs >= 1)
        agent->setMaxSpeed((float)zen::to_number(args[0]));
    return 0;
}

int natNavAgentGetAutoMove(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_bool(agent && agent->autoMove());
    return 1;
}

int natNavAgentSetAutoMove(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    if (agent && nargs >= 1)
        agent->setAutoMove(zen::is_truthy(args[0]));
    return 0;
}

int natNavAgentGetOrientToPath(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_bool(agent && agent->orientToPath());
    return 1;
}

int natNavAgentSetOrientToPath(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    if (agent && nargs >= 1)
        agent->setOrientToPath(zen::is_truthy(args[0]));
    return 0;
}

int natNavAgentGetRotationLerpSpeed(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_float(agent ? agent->rotationLerpSpeed() : 0.0f);
    return 1;
}

int natNavAgentSetRotationLerpSpeed(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    if (agent && nargs >= 1)
        agent->setRotationLerpSpeed((float)zen::to_number(args[0]));
    return 0;
}

int natNavAgentGetRotationOffset(zen::VM*, zen::Value* args, int)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    args[0] = zen::val_float(agent ? agent->rotationOffsetDegrees() : 0.0f);
    return 1;
}

int natNavAgentSetRotationOffset(zen::VM*, zen::Value* args, int nargs)
{
    NavigationAgent2D* agent = zen::zen_instance_data<NavigationAgent2D>(args[-1]);
    if (agent && nargs >= 1)
        agent->setRotationOffsetDegrees((float)zen::to_number(args[0]));
    return 0;
}


int natSteeringGetWeight(zen::VM*, zen::Value* args, int)
{
    Steering2D* steering = zen::zen_instance_data<Steering2D>(args[-1]);
    args[0] = zen::val_float(steering ? steering->weight() : 0.0f);
    return 1;
}

int natSteeringSetWeight(zen::VM*, zen::Value* args, int nargs)
{
    Steering2D* steering = zen::zen_instance_data<Steering2D>(args[-1]);
    if (steering && nargs >= 1)
        steering->setWeight((float)zen::to_number(args[0]));
    return 0;
}

int natSteeringSetTarget(zen::VM*, zen::Value* args, int nargs)
{
    Steering2D* steering = zen::zen_instance_data<Steering2D>(args[-1]);
    if (steering && nargs >= 2)
        steering->setTargetPosition(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natSteeringGetTarget(zen::VM*, zen::Value* args, int)
{
    Steering2D* steering = zen::zen_instance_data<Steering2D>(args[-1]);
    Math::Vec2 point(0.0f, 0.0f);
    const bool resolved = steering && steering->target(point);
    args[0] = zen::val_bool(resolved);
    args[1] = zen::val_float(point.x);
    args[2] = zen::val_float(point.y);
    return 3;
}

int natSteeringSetTargetObject(zen::VM* vm, zen::Value* args, int nargs)
{
    Steering2D* steering = zen::zen_instance_data<Steering2D>(args[-1]);
    if (steering && nargs >= 1)
    {
        char small[64];
        steering->setTargetName(valueToCString(vm, args[0], small, sizeof(small)));
    }
    return 0;
}

int natSteeringClearTargetObject(zen::VM*, zen::Value* args, int)
{
    if (Steering2D* steering = zen::zen_instance_data<Steering2D>(args[-1]))
        steering->setTargetName(nullptr);
    return 0;
}

int natFleeGetRadius(zen::VM*, zen::Value* args, int)
{
    Flee2D* flee = zen::zen_instance_data<Flee2D>(args[-1]);
    args[0] = zen::val_float(flee ? flee->radius() : 0.0f);
    return 1;
}

int natFleeSetRadius(zen::VM*, zen::Value* args, int nargs)
{
    Flee2D* flee = zen::zen_instance_data<Flee2D>(args[-1]);
    if (flee && nargs >= 1)
        flee->setRadius((float)zen::to_number(args[0]));
    return 0;
}

int natArriveGetSlowRadius(zen::VM*, zen::Value* args, int)
{
    Arrive2D* arrive = zen::zen_instance_data<Arrive2D>(args[-1]);
    args[0] = zen::val_float(arrive ? arrive->slowRadius() : 0.0f);
    return 1;
}

int natArriveSetSlowRadius(zen::VM*, zen::Value* args, int nargs)
{
    Arrive2D* arrive = zen::zen_instance_data<Arrive2D>(args[-1]);
    if (arrive && nargs >= 1)
        arrive->setSlowRadius((float)zen::to_number(args[0]));
    return 0;
}

int natArriveGetStopRadius(zen::VM*, zen::Value* args, int)
{
    Arrive2D* arrive = zen::zen_instance_data<Arrive2D>(args[-1]);
    args[0] = zen::val_float(arrive ? arrive->stopRadius() : 0.0f);
    return 1;
}

int natArriveSetStopRadius(zen::VM*, zen::Value* args, int nargs)
{
    Arrive2D* arrive = zen::zen_instance_data<Arrive2D>(args[-1]);
    if (arrive && nargs >= 1)
        arrive->setStopRadius((float)zen::to_number(args[0]));
    return 0;
}

int natWanderGetJitter(zen::VM*, zen::Value* args, int)
{
    Wander2D* wander = zen::zen_instance_data<Wander2D>(args[-1]);
    args[0] = zen::val_float(wander ? wander->jitter() : 0.0f);
    return 1;
}

int natWanderSetJitter(zen::VM*, zen::Value* args, int nargs)
{
    Wander2D* wander = zen::zen_instance_data<Wander2D>(args[-1]);
    if (wander && nargs >= 1)
        wander->setJitter((float)zen::to_number(args[0]));
    return 0;
}

int natWanderGetRadius(zen::VM*, zen::Value* args, int)
{
    Wander2D* wander = zen::zen_instance_data<Wander2D>(args[-1]);
    args[0] = zen::val_float(wander ? wander->radius() : 0.0f);
    return 1;
}

int natWanderSetRadius(zen::VM*, zen::Value* args, int nargs)
{
    Wander2D* wander = zen::zen_instance_data<Wander2D>(args[-1]);
    if (wander && nargs >= 1)
        wander->setRadius((float)zen::to_number(args[0]));
    return 0;
}

int natWanderGetDistance(zen::VM*, zen::Value* args, int)
{
    Wander2D* wander = zen::zen_instance_data<Wander2D>(args[-1]);
    args[0] = zen::val_float(wander ? wander->distance() : 0.0f);
    return 1;
}

int natWanderSetDistance(zen::VM*, zen::Value* args, int nargs)
{
    Wander2D* wander = zen::zen_instance_data<Wander2D>(args[-1]);
    if (wander && nargs >= 1)
        wander->setDistance((float)zen::to_number(args[0]));
    return 0;
}

int natSeparationGetRadius(zen::VM*, zen::Value* args, int)
{
    Separation2D* separation = zen::zen_instance_data<Separation2D>(args[-1]);
    args[0] = zen::val_float(separation ? separation->radius() : 0.0f);
    return 1;
}

int natSeparationSetRadius(zen::VM*, zen::Value* args, int nargs)
{
    Separation2D* separation = zen::zen_instance_data<Separation2D>(args[-1]);
    if (separation && nargs >= 1)
        separation->setRadius((float)zen::to_number(args[0]));
    return 0;
}

int natSeparationGetMask(zen::VM*, zen::Value* args, int)
{
    Separation2D* separation = zen::zen_instance_data<Separation2D>(args[-1]);
    args[0] = zen::val_int(separation ? (int64_t)separation->mask() : 0);
    return 1;
}

int natSeparationSetMask(zen::VM*, zen::Value* args, int nargs)
{
    Separation2D* separation = zen::zen_instance_data<Separation2D>(args[-1]);
    if (separation && nargs >= 1)
        separation->setMask((uint16_t)zen::to_number(args[0]));
    return 0;
}

int natAvoidanceGetLookAhead(zen::VM*, zen::Value* args, int)
{
    ObstacleAvoidance2D* avoidance = zen::zen_instance_data<ObstacleAvoidance2D>(args[-1]);
    args[0] = zen::val_float(avoidance ? avoidance->lookAhead() : 0.0f);
    return 1;
}

int natAvoidanceSetLookAhead(zen::VM*, zen::Value* args, int nargs)
{
    ObstacleAvoidance2D* avoidance = zen::zen_instance_data<ObstacleAvoidance2D>(args[-1]);
    if (avoidance && nargs >= 1)
        avoidance->setLookAhead((float)zen::to_number(args[0]));
    return 0;
}

int natAvoidanceGetMask(zen::VM*, zen::Value* args, int)
{
    ObstacleAvoidance2D* avoidance = zen::zen_instance_data<ObstacleAvoidance2D>(args[-1]);
    args[0] = zen::val_int(avoidance ? (int64_t)avoidance->mask() : 0);
    return 1;
}

int natAvoidanceSetMask(zen::VM*, zen::Value* args, int nargs)
{
    ObstacleAvoidance2D* avoidance = zen::zen_instance_data<ObstacleAvoidance2D>(args[-1]);
    if (avoidance && nargs >= 1)
        avoidance->setMask((uint16_t)zen::to_number(args[0]));
    return 0;
}

int natDirectionalLightGetColor(zen::VM*, zen::Value* args, int)
{
    DirectionalLight2D* light = zen::zen_instance_data<DirectionalLight2D>(args[-1]);
    const Color color = light ? light->color() : Color();
    args[0] = zen::val_float(color.r);
    args[1] = zen::val_float(color.g);
    args[2] = zen::val_float(color.b);
    args[3] = zen::val_float(color.a);
    return 4;
}

int natDirectionalLightSetColor(zen::VM*, zen::Value* args, int nargs)
{
    DirectionalLight2D* light = zen::zen_instance_data<DirectionalLight2D>(args[-1]);
    if (light && nargs >= 3)
    {
        const float a = nargs >= 4 ? (float)zen::to_number(args[3]) : 1.0f;
        light->setColor((float)zen::to_number(args[0]), (float)zen::to_number(args[1]),
                        (float)zen::to_number(args[2]), a);
    }
    return 0;
}

int natDirectionalLightGetEnergy(zen::VM*, zen::Value* args, int)
{
    DirectionalLight2D* light = zen::zen_instance_data<DirectionalLight2D>(args[-1]);
    args[0] = zen::val_float(light ? light->energy() : 0.0f);
    return 1;
}

int natDirectionalLightSetEnergy(zen::VM*, zen::Value* args, int nargs)
{
    DirectionalLight2D* light = zen::zen_instance_data<DirectionalLight2D>(args[-1]);
    if (light && nargs >= 1)
        light->setEnergy((float)zen::to_number(args[0]));
    return 0;
}

// Points are local to the occluder's own transform; RenderQueue applies
// owner()->globalTransform() at render time, same as LightOccluder2D::onRender.
int natOccluderSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    LightOccluder2D* occluder = zen::zen_instance_data<LightOccluder2D>(args[-1]);
    if (occluder && nargs >= 1)
    {
        ct::Vector<Math::Vec2> points;
        if (unpackPointArray(args[0], points) && !points.empty())
            occluder->setPolygon(points.data(), (int)points.size());
    }
    return 0;
}

int natOccluderPointCount(zen::VM*, zen::Value* args, int)
{
    LightOccluder2D* occluder = zen::zen_instance_data<LightOccluder2D>(args[-1]);
    args[0] = zen::val_int(occluder ? (int64_t)occluder->points().size() : 0);
    return 1;
}

int natOccluderGetPoint(zen::VM*, zen::Value* args, int nargs)
{
    LightOccluder2D* occluder = zen::zen_instance_data<LightOccluder2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (occluder && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& points = occluder->points();
        if (index >= 0 && (std::size_t)index < points.size())
            point = points[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

MotionEase parseMotionEase(const char* name)
{
    struct EaseEntry { const char* name; MotionEase ease; };
    static const EaseEntry table[] = {
        {"linear", MotionEase::Linear}, {"in_quad", MotionEase::InQuad}, {"out_quad", MotionEase::OutQuad},
        {"in_out_quad", MotionEase::InOutQuad}, {"in_cubic", MotionEase::InCubic}, {"out_cubic", MotionEase::OutCubic},
        {"in_out_cubic", MotionEase::InOutCubic}, {"in_sine", MotionEase::InSine}, {"out_sine", MotionEase::OutSine},
        {"in_out_sine", MotionEase::InOutSine}, {"in_back", MotionEase::InBack}, {"out_back", MotionEase::OutBack},
        {"in_out_back", MotionEase::InOutBack}, {"in_bounce", MotionEase::InBounce}, {"out_bounce", MotionEase::OutBounce},
        {"in_out_bounce", MotionEase::InOutBounce}, {"in_elastic", MotionEase::InElastic},
        {"out_elastic", MotionEase::OutElastic}, {"in_out_elastic", MotionEase::InOutElastic},
    };
    for (const EaseEntry& entry : table)
        if (std::strcmp(name, entry.name) == 0)
            return entry.ease;
    return MotionEase::Linear;
}

MotionTweenProperty parseMotionTweenProperty(const char* name)
{
    if (std::strcmp(name, "rotation") == 0)
        return MotionTweenProperty::Rotation;
    if (std::strcmp(name, "scale") == 0)
        return MotionTweenProperty::Scale;
    return MotionTweenProperty::Position;
}

MotionTweenLoop parseMotionTweenLoop(const char* name)
{
    if (std::strcmp(name, "repeat") == 0)
        return MotionTweenLoop::Repeat;
    if (std::strcmp(name, "ping_pong") == 0)
        return MotionTweenLoop::PingPong;
    return MotionTweenLoop::None;
}

const char* motionTweenLoopName(MotionTweenLoop loop)
{
    switch (loop)
    {
    case MotionTweenLoop::Repeat:
        return "repeat";
    case MotionTweenLoop::PingPong:
        return "ping_pong";
    default:
        return "none";
    }
}

int natMotionTweenPlay(zen::VM*, zen::Value* args, int nargs)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    if (tween)
        tween->play(nargs >= 1 ? zen::is_truthy(args[0]) : true);
    return 0;
}

int natMotionTweenStop(zen::VM*, zen::Value* args, int)
{
    if (MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]))
        tween->stop();
    return 0;
}

int natMotionTweenPause(zen::VM*, zen::Value* args, int nargs)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    if (tween)
        tween->pause(nargs >= 1 ? zen::is_truthy(args[0]) : true);
    return 0;
}

int natMotionTweenIsPlaying(zen::VM*, zen::Value* args, int)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    args[0] = zen::val_bool(tween && tween->playing());
    return 1;
}

int natMotionTweenIsPaused(zen::VM*, zen::Value* args, int)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    args[0] = zen::val_bool(tween && tween->paused());
    return 1;
}

int natMotionTweenGetTime(zen::VM*, zen::Value* args, int)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    args[0] = zen::val_float(tween ? tween->time() : 0.0f);
    return 1;
}

int natMotionTweenGetLoop(zen::VM* vm, zen::Value* args, int)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(motionTweenLoopName(tween ? tween->loop() : MotionTweenLoop::None)));
    return 1;
}

int natMotionTweenSetLoop(zen::VM* vm, zen::Value* args, int nargs)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    if (tween && nargs >= 1)
    {
        char small[16];
        tween->setLoop(parseMotionTweenLoop(valueToCString(vm, args[0], small, sizeof(small))));
    }
    return 0;
}

int natMotionTweenGetOneShot(zen::VM*, zen::Value* args, int)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    args[0] = zen::val_bool(tween && tween->oneShot());
    return 1;
}

int natMotionTweenSetOneShot(zen::VM*, zen::Value* args, int nargs)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    if (tween && nargs >= 1)
        tween->setOneShot(zen::is_truthy(args[0]));
    return 0;
}

int natMotionTweenClearTracks(zen::VM*, zen::Value* args, int)
{
    if (MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]))
        tween->clearTracks();
    return 0;
}

int natMotionTweenTrackCount(zen::VM*, zen::Value* args, int)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    args[0] = zen::val_int(tween ? (int64_t)tween->trackCount() : 0);
    return 1;
}

int natMotionTweenAddTrack(zen::VM* vm, zen::Value* args, int nargs)
{
    MotionTween2D* tween = zen::zen_instance_data<MotionTween2D>(args[-1]);
    if (tween && nargs >= 6)
    {
        char propertyBuf[16];
        MotionTweenTrack track;
        track.property = parseMotionTweenProperty(valueToCString(vm, args[0], propertyBuf, sizeof(propertyBuf)));
        track.from = Math::Vec2((float)zen::to_number(args[1]), (float)zen::to_number(args[2]));
        track.to = Math::Vec2((float)zen::to_number(args[3]), (float)zen::to_number(args[4]));
        track.duration = (float)zen::to_number(args[5]);
        track.delay = nargs >= 7 ? (float)zen::to_number(args[6]) : 0.0f;
        char easeBuf[16];
        track.ease =
            nargs >= 8 ? parseMotionEase(valueToCString(vm, args[7], easeBuf, sizeof(easeBuf))) : MotionEase::OutQuad;
        track.enabled = nargs >= 9 ? zen::is_truthy(args[8]) : true;
        tween->addTrack(track);
    }
    return 0;
}

int natMotionStreakReset(zen::VM*, zen::Value* args, int)
{
    if (MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]))
        streak->reset();
    return 0;
}

int natMotionStreakGetLifetime(zen::VM*, zen::Value* args, int)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    args[0] = zen::val_float(streak ? streak->lifetime() : 0.0f);
    return 1;
}

int natMotionStreakSetLifetime(zen::VM*, zen::Value* args, int nargs)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    if (streak && nargs >= 1)
        streak->setLifetime((float)zen::to_number(args[0]));
    return 0;
}

int natMotionStreakGetWidth(zen::VM*, zen::Value* args, int)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    args[0] = zen::val_float(streak ? streak->width() : 0.0f);
    return 1;
}

int natMotionStreakSetWidth(zen::VM*, zen::Value* args, int nargs)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    if (streak && nargs >= 1)
        streak->setWidth((float)zen::to_number(args[0]));
    return 0;
}

int natMotionStreakGetMinDistance(zen::VM*, zen::Value* args, int)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    args[0] = zen::val_float(streak ? streak->minDistance() : 0.0f);
    return 1;
}

int natMotionStreakSetMinDistance(zen::VM*, zen::Value* args, int nargs)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    if (streak && nargs >= 1)
        streak->setMinDistance((float)zen::to_number(args[0]));
    return 0;
}

int natMotionStreakGetColor(zen::VM*, zen::Value* args, int)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    const Color color = streak ? streak->color() : Color();
    args[0] = zen::val_float(color.r);
    args[1] = zen::val_float(color.g);
    args[2] = zen::val_float(color.b);
    args[3] = zen::val_float(color.a);
    return 4;
}

int natMotionStreakSetColor(zen::VM*, zen::Value* args, int nargs)
{
    MotionStreak2D* streak = zen::zen_instance_data<MotionStreak2D>(args[-1]);
    if (streak && nargs >= 3)
    {
        const float a = nargs >= 4 ? (float)zen::to_number(args[3]) : 1.0f;
        streak->setColor(Color((float)zen::to_number(args[0]), (float)zen::to_number(args[1]),
                               (float)zen::to_number(args[2]), a));
    }
    return 0;
}

int natSpriteBatchAdd(zen::VM* vm, zen::Value* args, int nargs)
{
    SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]);
    int index = -1;
    if (batch && nargs >= 5)
    {
        char small[128];
        const char* name = valueToCString(vm, args[0], small, sizeof(small));
        Texture* texture = gZenAssets ? gZenAssets->GetTexture(name) : nullptr;
        if (!texture && gZenAssets && name[0])
            texture = gZenAssets->LoadTexture(name, name, true, false);
        const Math::Vec2 position((float)zen::to_number(args[1]), (float)zen::to_number(args[2]));
        const Math::Vec2 size((float)zen::to_number(args[3]), (float)zen::to_number(args[4]));
        Color color = Color::White();
        if (nargs >= 8)
            color = Color((float)zen::to_number(args[5]) / 255.0f, (float)zen::to_number(args[6]) / 255.0f,
                          (float)zen::to_number(args[7]) / 255.0f,
                          nargs >= 9 ? (float)zen::to_number(args[8]) / 255.0f : 1.0f);
        index = batch->add(texture, position, size, color);
    }
    args[0] = zen::val_int(index);
    return 1;
}

int natSpriteBatchRemove(zen::VM*, zen::Value* args, int nargs)
{
    SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]);
    if (batch && nargs >= 1)
        batch->remove((int)zen::to_number(args[0]));
    return 0;
}

int natSpriteBatchClear(zen::VM*, zen::Value* args, int)
{
    if (SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]))
        batch->clear();
    return 0;
}

int natSpriteBatchCount(zen::VM*, zen::Value* args, int)
{
    SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]);
    args[0] = zen::val_int(batch ? batch->count() : 0);
    return 1;
}

int natSpriteBatchSetSource(zen::VM*, zen::Value* args, int nargs)
{
    SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]);
    if (batch && nargs >= 5)
        batch->setSource((int)zen::to_number(args[0]),
                         Math::Vec4((float)zen::to_number(args[1]), (float)zen::to_number(args[2]),
                                    (float)zen::to_number(args[3]), (float)zen::to_number(args[4])));
    return 0;
}

int natSpriteBatchSetFlip(zen::VM*, zen::Value* args, int nargs)
{
    SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]);
    if (batch && nargs >= 3)
        batch->setFlip((int)zen::to_number(args[0]), zen::is_truthy(args[1]), zen::is_truthy(args[2]));
    return 0;
}

int natSpriteBatchSetColor(zen::VM*, zen::Value* args, int nargs)
{
    SpriteBatch* batch = zen::zen_instance_data<SpriteBatch>(args[-1]);
    if (batch && nargs >= 5)
    {
        if (SpriteBatch::Entry* entry = batch->entryAt((int)zen::to_number(args[0])))
            entry->color = Color((float)zen::to_number(args[1]) / 255.0f, (float)zen::to_number(args[2]) / 255.0f,
                                 (float)zen::to_number(args[3]) / 255.0f, (float)zen::to_number(args[4]) / 255.0f);
    }
    return 0;
}

// Points are local to the line's own transform; RenderQueue applies
// owner()->globalTransform() at render time, same as Line2D::onRender.
int natLineSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    if (line && nargs >= 1)
    {
        ct::Vector<Math::Vec2> points;
        if (unpackPointArray(args[0], points))
            line->setPoints(points.data(), (int)points.size());
    }
    return 0;
}

int natLinePointCount(zen::VM*, zen::Value* args, int)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    args[0] = zen::val_int(line ? (int64_t)line->points().size() : 0);
    return 1;
}

int natLineGetPoint(zen::VM*, zen::Value* args, int nargs)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (line && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& points = line->points();
        if (index >= 0 && (std::size_t)index < points.size())
            point = points[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natLineGetWidth(zen::VM*, zen::Value* args, int)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    args[0] = zen::val_float(line ? line->width() : 0.0f);
    return 1;
}

int natLineSetWidth(zen::VM*, zen::Value* args, int nargs)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    if (line && nargs >= 1)
        line->setWidth((float)zen::to_number(args[0]));
    return 0;
}

int natLineGetColor(zen::VM*, zen::Value* args, int)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    const Color color = line ? line->color() : Color();
    args[0] = zen::val_int((int64_t)(color.r * 255.0f + 0.5f));
    args[1] = zen::val_int((int64_t)(color.g * 255.0f + 0.5f));
    args[2] = zen::val_int((int64_t)(color.b * 255.0f + 0.5f));
    args[3] = zen::val_int((int64_t)(color.a * 255.0f + 0.5f));
    return 4;
}

int natLineSetColor(zen::VM*, zen::Value* args, int nargs)
{
    Line2D* line = zen::zen_instance_data<Line2D>(args[-1]);
    if (line && nargs >= 3)
    {
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        line->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                       (unsigned char)zen::to_integer(args[2]), a);
    }
    return 0;
}

// Points are local to the polygon's own transform; RenderQueue applies
// owner()->globalTransform() at render time, same as Polygon2D::onRender.
int natPolygonSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    Polygon2D* polygon = zen::zen_instance_data<Polygon2D>(args[-1]);
    if (polygon && nargs >= 1)
    {
        ct::Vector<Math::Vec2> points;
        if (unpackPointArray(args[0], points) && points.size() >= 3)
            polygon->setPolygon(points.data(), (int)points.size());
    }
    return 0;
}

int natPolygonPointCount(zen::VM*, zen::Value* args, int)
{
    Polygon2D* polygon = zen::zen_instance_data<Polygon2D>(args[-1]);
    args[0] = zen::val_int(polygon ? (int64_t)polygon->polygon().size() : 0);
    return 1;
}

int natPolygonGetPoint(zen::VM*, zen::Value* args, int nargs)
{
    Polygon2D* polygon = zen::zen_instance_data<Polygon2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (polygon && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& points = polygon->polygon();
        if (index >= 0 && (std::size_t)index < points.size())
            point = points[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natPolygonIsValid(zen::VM*, zen::Value* args, int)
{
    Polygon2D* polygon = zen::zen_instance_data<Polygon2D>(args[-1]);
    args[0] = zen::val_bool(polygon && polygon->valid());
    return 1;
}

int natPolygonGetColor(zen::VM*, zen::Value* args, int)
{
    Polygon2D* polygon = zen::zen_instance_data<Polygon2D>(args[-1]);
    const Color color = polygon ? polygon->color() : Color();
    args[0] = zen::val_int((int64_t)(color.r * 255.0f + 0.5f));
    args[1] = zen::val_int((int64_t)(color.g * 255.0f + 0.5f));
    args[2] = zen::val_int((int64_t)(color.b * 255.0f + 0.5f));
    args[3] = zen::val_int((int64_t)(color.a * 255.0f + 0.5f));
    return 4;
}

int natPolygonSetColor(zen::VM*, zen::Value* args, int nargs)
{
    Polygon2D* polygon = zen::zen_instance_data<Polygon2D>(args[-1]);
    if (polygon && nargs >= 3)
    {
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        polygon->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                          (unsigned char)zen::to_integer(args[2]), a);
    }
    return 0;
}

int natNinePatchGetSize(zen::VM*, zen::Value* args, int)
{
    NinePatchComponent* patch = zen::zen_instance_data<NinePatchComponent>(args[-1]);
    const Math::Vec2 size = patch ? patch->size() : Math::Vec2(0.0f);
    args[0] = zen::val_float(size.x);
    args[1] = zen::val_float(size.y);
    return 2;
}

int natNinePatchSetSize(zen::VM*, zen::Value* args, int nargs)
{
    NinePatchComponent* patch = zen::zen_instance_data<NinePatchComponent>(args[-1]);
    if (patch && nargs >= 2)
        patch->setSize(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natNinePatchGetColor(zen::VM*, zen::Value* args, int)
{
    NinePatchComponent* patch = zen::zen_instance_data<NinePatchComponent>(args[-1]);
    const Color color = patch ? patch->color() : Color();
    args[0] = zen::val_int((int64_t)(color.r * 255.0f + 0.5f));
    args[1] = zen::val_int((int64_t)(color.g * 255.0f + 0.5f));
    args[2] = zen::val_int((int64_t)(color.b * 255.0f + 0.5f));
    args[3] = zen::val_int((int64_t)(color.a * 255.0f + 0.5f));
    return 4;
}

int natNinePatchSetColor(zen::VM*, zen::Value* args, int nargs)
{
    NinePatchComponent* patch = zen::zen_instance_data<NinePatchComponent>(args[-1]);
    if (patch && nargs >= 3)
    {
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        patch->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                        (unsigned char)zen::to_integer(args[2]), a);
    }
    return 0;
}



ShapeRenderMode parseShapeRenderMode(const char* name)
{
    return std::strcmp(name, "line") == 0 ? ShapeRenderMode::Line : ShapeRenderMode::Fill;
}

const char* shapeRenderModeName(ShapeRenderMode mode)
{
    return mode == ShapeRenderMode::Line ? "line" : "fill";
}

int natCircleShapeGetRadius(zen::VM*, zen::Value* args, int)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    args[0] = zen::val_float(shape ? shape->radius() : 0.0f);
    return 1;
}

int natCircleShapeSetRadius(zen::VM*, zen::Value* args, int nargs)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    if (shape && nargs >= 1)
        shape->setRadius((float)zen::to_number(args[0]));
    return 0;
}

int natCircleShapeGetMode(zen::VM* vm, zen::Value* args, int)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    args[0] =
        zen::val_obj((zen::Obj*)vm->make_string(shapeRenderModeName(shape ? shape->mode() : ShapeRenderMode::Fill)));
    return 1;
}

int natCircleShapeSetMode(zen::VM* vm, zen::Value* args, int nargs)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    if (shape && nargs >= 1)
    {
        char small[8];
        shape->setMode(parseShapeRenderMode(valueToCString(vm, args[0], small, sizeof(small))));
    }
    return 0;
}

int natCircleShapeGetLineWidth(zen::VM*, zen::Value* args, int)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    args[0] = zen::val_float(shape ? shape->lineWidth() : 0.0f);
    return 1;
}

int natCircleShapeSetLineWidth(zen::VM*, zen::Value* args, int nargs)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    if (shape && nargs >= 1)
        shape->setLineWidth((float)zen::to_number(args[0]));
    return 0;
}

int natCircleShapeGetColor(zen::VM*, zen::Value* args, int)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    const Color color = shape ? shape->color() : Color();
    args[0] = zen::val_int((int64_t)(color.r * 255.0f + 0.5f));
    args[1] = zen::val_int((int64_t)(color.g * 255.0f + 0.5f));
    args[2] = zen::val_int((int64_t)(color.b * 255.0f + 0.5f));
    args[3] = zen::val_int((int64_t)(color.a * 255.0f + 0.5f));
    return 4;
}

int natCircleShapeSetColor(zen::VM*, zen::Value* args, int nargs)
{
    CircleShape* shape = zen::zen_instance_data<CircleShape>(args[-1]);
    if (shape && nargs >= 3)
    {
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        shape->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                        (unsigned char)zen::to_integer(args[2]), a);
    }
    return 0;
}

int natRectShapeGetSize(zen::VM*, zen::Value* args, int)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    const Math::Vec2 size = shape ? shape->size() : Math::Vec2(0.0f);
    args[0] = zen::val_float(size.x);
    args[1] = zen::val_float(size.y);
    return 2;
}

int natRectShapeSetSize(zen::VM*, zen::Value* args, int nargs)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    if (shape && nargs >= 2)
        shape->setSize(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natRectShapeGetMode(zen::VM* vm, zen::Value* args, int)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    args[0] =
        zen::val_obj((zen::Obj*)vm->make_string(shapeRenderModeName(shape ? shape->mode() : ShapeRenderMode::Fill)));
    return 1;
}

int natRectShapeSetMode(zen::VM* vm, zen::Value* args, int nargs)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    if (shape && nargs >= 1)
    {
        char small[8];
        shape->setMode(parseShapeRenderMode(valueToCString(vm, args[0], small, sizeof(small))));
    }
    return 0;
}

int natRectShapeGetLineWidth(zen::VM*, zen::Value* args, int)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    args[0] = zen::val_float(shape ? shape->lineWidth() : 0.0f);
    return 1;
}

int natRectShapeSetLineWidth(zen::VM*, zen::Value* args, int nargs)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    if (shape && nargs >= 1)
        shape->setLineWidth((float)zen::to_number(args[0]));
    return 0;
}

int natRectShapeGetColor(zen::VM*, zen::Value* args, int)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    const Color color = shape ? shape->color() : Color();
    args[0] = zen::val_int((int64_t)(color.r * 255.0f + 0.5f));
    args[1] = zen::val_int((int64_t)(color.g * 255.0f + 0.5f));
    args[2] = zen::val_int((int64_t)(color.b * 255.0f + 0.5f));
    args[3] = zen::val_int((int64_t)(color.a * 255.0f + 0.5f));
    return 4;
}

int natRectShapeSetColor(zen::VM*, zen::Value* args, int nargs)
{
    RectShape* shape = zen::zen_instance_data<RectShape>(args[-1]);
    if (shape && nargs >= 3)
    {
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        shape->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                        (unsigned char)zen::to_integer(args[2]), a);
    }
    return 0;
}

int natCapsuleShapeGetSize(zen::VM*, zen::Value* args, int)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    const Math::Vec2 size = shape ? shape->size() : Math::Vec2(0.0f);
    args[0] = zen::val_float(size.x);
    args[1] = zen::val_float(size.y);
    return 2;
}

int natCapsuleShapeSetSize(zen::VM*, zen::Value* args, int nargs)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    if (shape && nargs >= 2)
        shape->setSize(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natCapsuleShapeGetMode(zen::VM* vm, zen::Value* args, int)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    args[0] =
        zen::val_obj((zen::Obj*)vm->make_string(shapeRenderModeName(shape ? shape->mode() : ShapeRenderMode::Fill)));
    return 1;
}

int natCapsuleShapeSetMode(zen::VM* vm, zen::Value* args, int nargs)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    if (shape && nargs >= 1)
    {
        char small[8];
        shape->setMode(parseShapeRenderMode(valueToCString(vm, args[0], small, sizeof(small))));
    }
    return 0;
}

int natCapsuleShapeGetLineWidth(zen::VM*, zen::Value* args, int)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    args[0] = zen::val_float(shape ? shape->lineWidth() : 0.0f);
    return 1;
}

int natCapsuleShapeSetLineWidth(zen::VM*, zen::Value* args, int nargs)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    if (shape && nargs >= 1)
        shape->setLineWidth((float)zen::to_number(args[0]));
    return 0;
}

int natCapsuleShapeGetColor(zen::VM*, zen::Value* args, int)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    const Color color = shape ? shape->color() : Color();
    args[0] = zen::val_int((int64_t)(color.r * 255.0f + 0.5f));
    args[1] = zen::val_int((int64_t)(color.g * 255.0f + 0.5f));
    args[2] = zen::val_int((int64_t)(color.b * 255.0f + 0.5f));
    args[3] = zen::val_int((int64_t)(color.a * 255.0f + 0.5f));
    return 4;
}

int natCapsuleShapeSetColor(zen::VM*, zen::Value* args, int nargs)
{
    CapsuleShape* shape = zen::zen_instance_data<CapsuleShape>(args[-1]);
    if (shape && nargs >= 3)
    {
        const unsigned char a = nargs >= 4 ? (unsigned char)zen::to_integer(args[3]) : 255;
        shape->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                        (unsigned char)zen::to_integer(args[2]), a);
    }
    return 0;
}

int natBoxColliderGetSize(zen::VM*, zen::Value* args, int)
{
    BoxCollider2D* box = zen::zen_instance_data<BoxCollider2D>(args[-1]);
    const Math::Vec2 size = box ? box->size() : Math::Vec2(0.0f);
    args[0] = zen::val_float(size.x);
    args[1] = zen::val_float(size.y);
    return 2;
}

int natBoxColliderSetSize(zen::VM*, zen::Value* args, int nargs)
{
    BoxCollider2D* box = zen::zen_instance_data<BoxCollider2D>(args[-1]);
    if (box && nargs >= 2)
        box->setSize(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natCircleColliderGetRadius(zen::VM*, zen::Value* args, int)
{
    CircleCollider2D* circle = zen::zen_instance_data<CircleCollider2D>(args[-1]);
    args[0] = zen::val_float(circle ? circle->radius() : 0.0f);
    return 1;
}

int natCircleColliderSetRadius(zen::VM*, zen::Value* args, int nargs)
{
    CircleCollider2D* circle = zen::zen_instance_data<CircleCollider2D>(args[-1]);
    if (circle && nargs >= 1)
        circle->setRadius((float)zen::to_number(args[0]));
    return 0;
}

int natEdgeColliderGetPoints(zen::VM*, zen::Value* args, int)
{
    EdgeCollider2D* edge = zen::zen_instance_data<EdgeCollider2D>(args[-1]);
    const Math::Vec2 start = edge ? edge->start() : Math::Vec2(0.0f);
    const Math::Vec2 end = edge ? edge->end() : Math::Vec2(0.0f);
    args[0] = zen::val_float(start.x);
    args[1] = zen::val_float(start.y);
    args[2] = zen::val_float(end.x);
    args[3] = zen::val_float(end.y);
    return 4;
}

int natEdgeColliderSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    EdgeCollider2D* edge = zen::zen_instance_data<EdgeCollider2D>(args[-1]);
    if (edge && nargs >= 4)
        edge->setPoints(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])),
                        Math::Vec2((float)zen::to_number(args[2]), (float)zen::to_number(args[3])));
    return 0;
}

// Points are local to the collider's own transform (the same convention as
// Polygon2D/LightOccluder2D). setPoints()/setRegular() mark the body dirty,
// same rebuild-on-next-step() path as the other collider setters above.
int natPolygonColliderSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    PolygonCollider2D* polygon = zen::zen_instance_data<PolygonCollider2D>(args[-1]);
    if (polygon && nargs >= 1)
    {
        ct::Vector<Math::Vec2> points;
        if (unpackPointArray(args[0], points) && points.size() >= 3)
            polygon->setPoints(points.data(), (int)points.size());
    }
    return 0;
}

int natPolygonColliderPointCount(zen::VM*, zen::Value* args, int)
{
    PolygonCollider2D* polygon = zen::zen_instance_data<PolygonCollider2D>(args[-1]);
    args[0] = zen::val_int(polygon ? (int64_t)polygon->points().size() : 0);
    return 1;
}

int natPolygonColliderGetPoint(zen::VM*, zen::Value* args, int nargs)
{
    PolygonCollider2D* polygon = zen::zen_instance_data<PolygonCollider2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (polygon && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& points = polygon->points();
        if (index >= 0 && (std::size_t)index < points.size())
            point = points[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natPolygonColliderSetRegular(zen::VM*, zen::Value* args, int nargs)
{
    PolygonCollider2D* polygon = zen::zen_instance_data<PolygonCollider2D>(args[-1]);
    if (polygon && nargs >= 2)
        polygon->setRegular((int)zen::to_number(args[0]), (float)zen::to_number(args[1]));
    return 0;
}

// Points are local to the collider's own transform, same convention as
// PolygonCollider2D above; setPoints()/setLoop() mark the body dirty too.
int natChainColliderSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    ChainCollider2D* chain = zen::zen_instance_data<ChainCollider2D>(args[-1]);
    if (chain && nargs >= 1)
    {
        ct::Vector<Math::Vec2> points;
        if (unpackPointArray(args[0], points) && points.size() >= 2)
            chain->setPoints(points.data(), (int)points.size());
    }
    return 0;
}

int natChainColliderPointCount(zen::VM*, zen::Value* args, int)
{
    ChainCollider2D* chain = zen::zen_instance_data<ChainCollider2D>(args[-1]);
    args[0] = zen::val_int(chain ? (int64_t)chain->points().size() : 0);
    return 1;
}

int natChainColliderGetPoint(zen::VM*, zen::Value* args, int nargs)
{
    ChainCollider2D* chain = zen::zen_instance_data<ChainCollider2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (chain && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& points = chain->points();
        if (index >= 0 && (std::size_t)index < points.size())
            point = points[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natChainColliderGetLoop(zen::VM*, zen::Value* args, int)
{
    ChainCollider2D* chain = zen::zen_instance_data<ChainCollider2D>(args[-1]);
    args[0] = zen::val_bool(chain && chain->loop());
    return 1;
}

int natChainColliderSetLoop(zen::VM*, zen::Value* args, int nargs)
{
    ChainCollider2D* chain = zen::zen_instance_data<ChainCollider2D>(args[-1]);
    if (chain && nargs >= 1)
        chain->setLoop(zen::is_truthy(args[0]));
    return 0;
}

// Points are local to the region's own transform; NavigationRegion2D::getPath
// applies owner()->globalTransform() when it walks the baked triangle mesh.
int natNavRegionSetPoints(zen::VM*, zen::Value* args, int nargs)
{
    NavigationRegion2D* region = zen::zen_instance_data<NavigationRegion2D>(args[-1]);
    if (region && nargs >= 1)
    {
        ct::Vector<Math::Vec2> points;
        if (unpackPointArray(args[0], points) && points.size() >= 3)
            region->setPolygon(points.data(), (int)points.size());
    }
    return 0;
}

int natNavRegionPointCount(zen::VM*, zen::Value* args, int)
{
    NavigationRegion2D* region = zen::zen_instance_data<NavigationRegion2D>(args[-1]);
    args[0] = zen::val_int(region ? (int64_t)region->polygon().size() : 0);
    return 1;
}

int natNavRegionGetPoint(zen::VM*, zen::Value* args, int nargs)
{
    NavigationRegion2D* region = zen::zen_instance_data<NavigationRegion2D>(args[-1]);
    Math::Vec2 point(0.0f);
    if (region && nargs >= 1)
    {
        const int index = (int)zen::to_number(args[0]);
        const ct::Vector<Math::Vec2>& points = region->polygon();
        if (index >= 0 && (std::size_t)index < points.size())
            point = points[index];
    }
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natNavRegionIsValid(zen::VM*, zen::Value* args, int)
{
    NavigationRegion2D* region = zen::zen_instance_data<NavigationRegion2D>(args[-1]);
    args[0] = zen::val_bool(region && region->valid());
    return 1;
}


/* One table behind get_component, add_component, has_component and
** remove_component: the aliases a script may write live here only. An entry
** with no `add` is an abstract base — Collider and Light match any of their
** concrete kinds, so there is nothing single to create. */
struct ComponentBinding
{
    const char* names[3];
    Component* (*get)(GameObject&);
    Component* (*add)(GameObject&);
    bool (*has)(const GameObject&);
};

template <class T> Component* componentGet(GameObject& node) { return node.getComponent<T>(); }
template <class T> Component* componentAdd(GameObject& node) { return node.addComponent<T>(); }
template <class T> bool componentHas(const GameObject& node) { return node.contains<T>(); }

template <ComponentType kType> Component* componentGetRaw(GameObject& node)
{
    return node.rawComponent(kType);
}

template <ComponentType kType> bool componentHasRaw(const GameObject& node)
{
    return node.rawComponentCount(kType) > 0;
}

const ComponentBinding kComponentBindings[] = {
    {{"RigidBody", "Body", "RigidBody2D"}, &componentGet<RigidBody2D>, &componentAdd<RigidBody2D>, &componentHas<RigidBody2D>},
    {{"Sprite", "SpriteComponent", nullptr}, &componentGet<SpriteComponent>, &componentAdd<SpriteComponent>, &componentHas<SpriteComponent>},
    {{"Animation", "Animation2D", nullptr}, &componentGet<Animation2D>, &componentAdd<Animation2D>, &componentHas<Animation2D>},
    {{"Camera", "CameraComponent", nullptr}, &componentGet<CameraComponent>, &componentAdd<CameraComponent>, &componentHas<CameraComponent>},
    {{"Particle", "ParticleComponent", nullptr}, &componentGet<ParticleComponent>, &componentAdd<ParticleComponent>, &componentHas<ParticleComponent>},
    {{"ScriptComponent", nullptr, nullptr}, &componentGet<ZenScriptComponent>, &componentAdd<ZenScriptComponent>, &componentHas<ZenScriptComponent>},
    {{"CharacterBody", "CharacterBody2D", nullptr}, &componentGet<CharacterBody2D>, &componentAdd<CharacterBody2D>, &componentHas<CharacterBody2D>},
    {{"Collider", "Collider2D", nullptr}, &componentGetRaw<ComponentType::Collider>, nullptr, &componentHasRaw<ComponentType::Collider>},
    {{"BoxCollider", "BoxCollider2D", nullptr}, &componentGet<BoxCollider2D>, &componentAdd<BoxCollider2D>, &componentHas<BoxCollider2D>},
    {{"CircleCollider", "CircleCollider2D", nullptr}, &componentGet<CircleCollider2D>, &componentAdd<CircleCollider2D>, &componentHas<CircleCollider2D>},
    {{"EdgeCollider", "EdgeCollider2D", nullptr}, &componentGet<EdgeCollider2D>, &componentAdd<EdgeCollider2D>, &componentHas<EdgeCollider2D>},
    {{"PolygonCollider", "PolygonCollider2D", nullptr}, &componentGet<PolygonCollider2D>, &componentAdd<PolygonCollider2D>, &componentHas<PolygonCollider2D>},
    {{"ChainCollider", "ChainCollider2D", nullptr}, &componentGet<ChainCollider2D>, &componentAdd<ChainCollider2D>, &componentHas<ChainCollider2D>},
    {{"TileMap", "TileMapComponent", nullptr}, &componentGet<TileMapComponent>, &componentAdd<TileMapComponent>, &componentHas<TileMapComponent>},
    {{"SpriteBatch", nullptr, nullptr}, &componentGet<SpriteBatch>, &componentAdd<SpriteBatch>, &componentHas<SpriteBatch>},
    {{"Polygon2D", nullptr, nullptr}, &componentGet<Polygon2D>, &componentAdd<Polygon2D>, &componentHas<Polygon2D>},
    {{"Line2D", nullptr, nullptr}, &componentGet<Line2D>, &componentAdd<Line2D>, &componentHas<Line2D>},
    {{"NinePatch", "NinePatchComponent", nullptr}, &componentGet<NinePatchComponent>, &componentAdd<NinePatchComponent>, &componentHas<NinePatchComponent>},
    {{"Light", nullptr, nullptr}, &componentGetRaw<ComponentType::Light>, nullptr, &componentHasRaw<ComponentType::Light>},
    {{"Light2D", nullptr, nullptr}, &componentGet<Light2D>, &componentAdd<Light2D>, &componentHas<Light2D>},
    {{"DirectionalLight2D", nullptr, nullptr}, &componentGet<DirectionalLight2D>, &componentAdd<DirectionalLight2D>, &componentHas<DirectionalLight2D>},
    {{"LightOccluder", "LightOccluder2D", nullptr}, &componentGet<LightOccluder2D>, &componentAdd<LightOccluder2D>, &componentHas<LightOccluder2D>},
    {{"AudioPlayer", nullptr, nullptr}, &componentGet<AudioPlayer>, &componentAdd<AudioPlayer>, &componentHas<AudioPlayer>},
    {{"CircleShape", nullptr, nullptr}, &componentGet<CircleShape>, &componentAdd<CircleShape>, &componentHas<CircleShape>},
    {{"RectShape", nullptr, nullptr}, &componentGet<RectShape>, &componentAdd<RectShape>, &componentHas<RectShape>},
    {{"CapsuleShape", nullptr, nullptr}, &componentGet<CapsuleShape>, &componentAdd<CapsuleShape>, &componentHas<CapsuleShape>},
    {{"UiCanvas", nullptr, nullptr}, &componentGet<UiCanvas>, &componentAdd<UiCanvas>, &componentHas<UiCanvas>},
    {{"Panel", "UiPanel", nullptr}, &componentGet<UiPanel>, &componentAdd<UiPanel>, &componentHas<UiPanel>},
    {{"Label", "UiLabel", nullptr}, &componentGet<UiLabel>, &componentAdd<UiLabel>, &componentHas<UiLabel>},
    {{"Button", "UiButton", nullptr}, &componentGet<UiButton>, &componentAdd<UiButton>, &componentHas<UiButton>},
    {{"CheckBox", "UiCheckBox", nullptr}, &componentGet<UiCheckBox>, &componentAdd<UiCheckBox>, &componentHas<UiCheckBox>},
    {{"Slider", "UiSlider", nullptr}, &componentGet<UiSlider>, &componentAdd<UiSlider>, &componentHas<UiSlider>},
    {{"NavigationRegion", "NavigationRegion2D", nullptr}, &componentGet<NavigationRegion2D>, &componentAdd<NavigationRegion2D>, &componentHas<NavigationRegion2D>},
    {{"NavigationAgent", "NavigationAgent2D", nullptr}, &componentGet<NavigationAgent2D>, &componentAdd<NavigationAgent2D>, &componentHas<NavigationAgent2D>},
    {{"Steering", "Steering2D", nullptr}, &componentGetRaw<ComponentType::Steering>, nullptr, &componentHasRaw<ComponentType::Steering>},
    {{"Seek", "Seek2D", nullptr}, &componentGet<Seek2D>, &componentAdd<Seek2D>, &componentHas<Seek2D>},
    {{"Flee", "Flee2D", nullptr}, &componentGet<Flee2D>, &componentAdd<Flee2D>, &componentHas<Flee2D>},
    {{"Arrive", "Arrive2D", nullptr}, &componentGet<Arrive2D>, &componentAdd<Arrive2D>, &componentHas<Arrive2D>},
    {{"Wander", "Wander2D", nullptr}, &componentGet<Wander2D>, &componentAdd<Wander2D>, &componentHas<Wander2D>},
    {{"Separation", "Separation2D", nullptr}, &componentGet<Separation2D>, &componentAdd<Separation2D>, &componentHas<Separation2D>},
    {{"ObstacleAvoidance", "ObstacleAvoidance2D", nullptr}, &componentGet<ObstacleAvoidance2D>, &componentAdd<ObstacleAvoidance2D>, &componentHas<ObstacleAvoidance2D>},
    {{"MotionTween", "MotionTween2D", nullptr}, &componentGet<MotionTween2D>, &componentAdd<MotionTween2D>, &componentHas<MotionTween2D>},
    {{"MotionStreak", "MotionStreak2D", nullptr}, &componentGet<MotionStreak2D>, &componentAdd<MotionStreak2D>, &componentHas<MotionStreak2D>},
    {{"Skeleton", "Skeleton2D", nullptr}, &componentGet<Skeleton2D>, &componentAdd<Skeleton2D>, &componentHas<Skeleton2D>},
    {{"Bone", "Bone2D", nullptr}, &componentGet<Bone2D>, &componentAdd<Bone2D>, &componentHas<Bone2D>},
};

const ComponentBinding* findComponentBinding(const char* name)
{
    for (const ComponentBinding& binding : kComponentBindings)
        for (const char* alias : binding.names)
            if (alias && !std::strcmp(name, alias))
                return &binding;
    return nullptr;
}

/* Generic component lookup.  The Zen compiler lowers
** node.get_component<RigidBody>() to node.get_component(RigidBody), so the first
** native argument is the requested host class. */
int natNodeGetComponent(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    if (!node || !state || nargs < 1 || !zen::is_class(args[0]))
    {
        args[0] = zen::val_nil();
        return 1;
    }

    zen::ObjClass* requested = zen::as_class(args[0]);
    const char* name = requested->name ? requested->name->chars : "";

    const ComponentBinding* binding = findComponentBinding(name);
    Component* component = binding ? binding->get(*node) : nullptr;
    args[0] = component ? state->instanceFor(requested, component) : zen::val_nil();
    return 1;
}

int natNodeAddComponent(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    if (!node || !state || nargs < 1 || !zen::is_class(args[0]))
    {
        args[0] = zen::val_nil();
        return 1;
    }

    zen::ObjClass* requested = zen::as_class(args[0]);
    const char* name = requested->name ? requested->name->chars : "";
    const ComponentBinding* binding = findComponentBinding(name);
    Component* component = (binding && binding->add) ? binding->add(*node) : nullptr;
    args[0] = component ? state->instanceFor(requested, component) : zen::val_nil();
    return 1;
}

int natNodeHasComponent(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    bool has = false;
    if (node && nargs >= 1 && zen::is_class(args[0]))
    {
        zen::ObjClass* requested = zen::as_class(args[0]);
        const char* name = requested->name ? requested->name->chars : "";
        if (const ComponentBinding* binding = findComponentBinding(name))
            has = binding->has(*node);
    }
    args[0] = zen::val_bool(has);
    return 1;
}

int natNodeRemoveComponent(zen::VM*, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    bool removed = false;
    if (node && nargs >= 1 && zen::is_class(args[0]))
    {
        zen::ObjClass* requested = zen::as_class(args[0]);
        const char* name = requested->name ? requested->name->chars : "";
        if (const ComponentBinding* binding = findComponentBinding(name))
            if (Component* component = binding->get(*node))
                removed = node->removeComponent(component);
    }
    args[0] = zen::val_bool(removed);
    return 1;
}

int natNodeGetId(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_int(node ? (int64_t)node->id() : 0);
    return 1;
}

int natNodeSetName(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
    {
        char small[16];
        node->setName(valueToCString(vm, args[0], small, sizeof(small)));
    }
    return 0;
}

int natNodeGetTag(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(node ? node->tag().c_str() : ""));
    return 1;
}

int natNodeSetTag(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    if (node && nargs >= 1)
    {
        char small[16];
        node->setTag(valueToCString(vm, args[0], small, sizeof(small)));
    }
    return 0;
}

int natNodeGetGlobalPosition(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    const Math::Vec2 position = node ? node->globalPosition() : Math::Vec2(0.0f);
    args[0] = zen::val_float(position.x);
    args[1] = zen::val_float(position.y);
    return 2;
}

int natNodeGetRight(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    const Math::Vec2 direction = node ? node->right() : Math::Vec2(1.0f, 0.0f);
    args[0] = zen::val_float(direction.x);
    args[1] = zen::val_float(direction.y);
    return 2;
}

int natNodeGetUp(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    const Math::Vec2 direction = node ? node->up() : Math::Vec2(0.0f, 1.0f);
    args[0] = zen::val_float(direction.x);
    args[1] = zen::val_float(direction.y);
    return 2;
}

int natNodeIsActiveInHierarchy(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_bool(node && node->isActiveInHierarchy());
    return 1;
}

int natNodeIsVisibleInHierarchy(zen::VM*, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    args[0] = zen::val_bool(node && node->isVisibleInHierarchy());
    return 1;
}

int natNodeGetRoot(zen::VM* vm, zen::Value* args, int)
{
    GameObject* node = nodeFromSelf(args);
    ZenRuntime::Impl* state = stateFromVM(vm);
    Scene* scene = node ? node->scene() : nullptr;
    args[0] = (scene && state) ? state->instanceFor(state->nodeClass, &scene->root()) : zen::val_nil();
    return 1;
}

int natNodeReparent(zen::VM* vm, zen::Value* args, int nargs)
{
    GameObject* node = nodeFromSelf(args);
    Scene* scene = node ? node->scene() : nullptr;
    bool ok = false;
    if (scene && nargs >= 1)
    {
        GameObject* parent = zen::zen_instance_data<GameObject>(args[0]);
        if (parent)
            ok = scene->reparent(node, parent);
    }
    args[0] = zen::val_bool(ok);
    return 1;
}

int natButtonClicked(zen::VM*, zen::Value* args, int)
{
    UiButton* button = zen::zen_instance_data<UiButton>(args[-1]);
    args[0] = zen::val_bool(button && button->consumeClick());
    return 1;
}

int natButtonSetText(zen::VM* vm, zen::Value* args, int nargs)
{
    UiButton* button = zen::zen_instance_data<UiButton>(args[-1]);
    if (button && nargs >= 1)
    {
        char small[16];
        button->setText(valueToCString(vm, args[0], small, sizeof(small)));
    }
    return 0;
}

int natCheckBoxGetChecked(zen::VM*, zen::Value* args, int)
{
    UiCheckBox* check = zen::zen_instance_data<UiCheckBox>(args[-1]);
    args[0] = zen::val_bool(check && check->checked());
    return 1;
}

int natCheckBoxSetChecked(zen::VM*, zen::Value* args, int nargs)
{
    if (UiCheckBox* check = zen::zen_instance_data<UiCheckBox>(args[-1]))
        if (nargs >= 1)
            check->setChecked(zen::is_truthy(args[0]));
    return 0;
}

int natCheckBoxChanged(zen::VM*, zen::Value* args, int)
{
    UiCheckBox* check = zen::zen_instance_data<UiCheckBox>(args[-1]);
    args[0] = zen::val_bool(check && check->consumeChanged());
    return 1;
}

int natSliderGetValue(zen::VM*, zen::Value* args, int)
{
    UiSlider* slider = zen::zen_instance_data<UiSlider>(args[-1]);
    args[0] = zen::val_float(slider ? slider->value() : 0.0f);
    return 1;
}

int natSliderSetValue(zen::VM*, zen::Value* args, int nargs)
{
    if (UiSlider* slider = zen::zen_instance_data<UiSlider>(args[-1]))
        if (nargs >= 1)
            slider->setValue((float)zen::to_number(args[0]));
    return 0;
}

int natSliderChanged(zen::VM*, zen::Value* args, int)
{
    UiSlider* slider = zen::zen_instance_data<UiSlider>(args[-1]);
    args[0] = zen::val_bool(slider && slider->consumeChanged());
    return 1;
}

int natSpriteSetColor(zen::VM*, zen::Value* args, int nargs)
{
    SpriteComponent* sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
    if (sprite && nargs >= 4)
        sprite->setColor((unsigned char)zen::to_integer(args[0]), (unsigned char)zen::to_integer(args[1]),
                         (unsigned char)zen::to_integer(args[2]), (unsigned char)zen::to_integer(args[3]));
    return 0;
}

int natSpriteSetFlip(zen::VM*, zen::Value* args, int nargs)
{
    SpriteComponent* sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
    if (sprite && nargs >= 2)
        sprite->setFlip(zen::is_truthy(args[0]), zen::is_truthy(args[1]));
    return 0;
}

int natSpriteSetSize(zen::VM*, zen::Value* args, int nargs)
{
    SpriteComponent* sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
    if (sprite && nargs >= 2)
        sprite->setSize(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natSpriteSetWaterEnabled(zen::VM*, zen::Value* args, int nargs)
{
    SpriteComponent* sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
    if (sprite && nargs >= 1)
        sprite->setWaterEnabled(zen::is_truthy(args[0]));
    return 0;
}

int natSpriteSetWaterFlow(zen::VM*, zen::Value* args, int nargs)
{
    SpriteComponent* sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
    if (sprite && nargs >= 4)
    {
        WaterEffect& water = sprite->water();
        water.flowA = Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1]));
        water.flowB = Math::Vec2((float)zen::to_number(args[2]), (float)zen::to_number(args[3]));
    }
    return 0;
}

int natSpriteSetWaterStrength(zen::VM*, zen::Value* args, int nargs)
{
    SpriteComponent* sprite = zen::zen_instance_data<SpriteComponent>(args[-1]);
    if (sprite && nargs >= 1)
        sprite->water().strength = (float)zen::to_number(args[0]);
    return 0;
}

int natAnimationPlay(zen::VM* vm, zen::Value* args, int nargs)
{
    Animation2D* animation = zen::zen_instance_data<Animation2D>(args[-1]);
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

int natAnimationStop(zen::VM*, zen::Value* args, int)
{
    Animation2D* animation = zen::zen_instance_data<Animation2D>(args[-1]);
    if (animation)
        animation->stop();
    return 0;
}

int natAnimationIsPlaying(zen::VM*, zen::Value* args, int)
{
    Animation2D* animation = zen::zen_instance_data<Animation2D>(args[-1]);
    args[0] = zen::val_bool(animation ? animation->playing() : false);
    return 1;
}

int natAnimationCurrent(zen::VM* vm, zen::Value* args, int)
{
    Animation2D* animation = zen::zen_instance_data<Animation2D>(args[-1]);
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(animation ? animation->currentClip() : ""));
    return 1;
}

int natParticleStart(zen::VM*, zen::Value* args, int)
{
    ParticleComponent* particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
    if (particle)
        particle->system().Start();
    return 0;
}

int natParticleStop(zen::VM*, zen::Value* args, int)
{
    ParticleComponent* particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
    if (particle)
        particle->system().Stop();
    return 0;
}

int natParticleReset(zen::VM*, zen::Value* args, int)
{
    ParticleComponent* particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
    if (particle)
        particle->system().Reset();
    return 0;
}

int natParticleBurst(zen::VM*, zen::Value* args, int nargs)
{
    ParticleComponent* particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
    if (particle)
    {
        const int64_t count = nargs >= 1 ? zen::to_integer(args[0]) : 1;
        for (int64_t i = 0; i < count; ++i)
            particle->system().Emit(particle->system().EmitterPosition(), particle->system().GetPrefab());
    }
    return 0;
}

int natParticleIsPlaying(zen::VM*, zen::Value* args, int)
{
    ParticleComponent* particle = zen::zen_instance_data<ParticleComponent>(args[-1]);
    args[0] = zen::val_bool(particle ? particle->system().IsPlaying() : false);
    return 1;
}

int natGetNumber(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const double fallback = nargs >= 2 ? zen::to_number(args[1]) : 0.0;
    args[0] = zen::val_float(ZenBlackboard::getNumber(key, fallback));
    return 1;
}

int natSetNumber(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    if (nargs >= 2)
        ZenBlackboard::setNumber(valueToCString(vm, args[0], small, sizeof(small)), zen::to_number(args[1]));
    return 0;
}

int natGetString(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    char smallFallback[16];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const char* fallback = nargs >= 2 ? valueToCString(vm, args[1], smallFallback, sizeof(smallFallback)) : "";
    const ct::String value = ZenBlackboard::getString(key, fallback);
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(value.c_str()));
    return 1;
}

int natSetString(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    char smallValue[16];
    if (nargs >= 2)
        ZenBlackboard::setString(valueToCString(vm, args[0], small, sizeof(small)),
                                 valueToCString(vm, args[1], smallValue, sizeof(smallValue)));
    return 0;
}

int natGetFlag(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const bool fallback = nargs >= 2 && zen::is_truthy(args[1]);
    args[0] = zen::val_bool(ZenBlackboard::getBool(key, fallback));
    return 1;
}

int natSetFlag(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    if (nargs >= 2)
        ZenBlackboard::setBool(valueToCString(vm, args[0], small, sizeof(small)), zen::is_truthy(args[1]));
    return 0;
}

int natHasKey(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_bool(ZenBlackboard::has(key));
    return 1;
}

int natUserDataGetInt(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const int fallback = nargs >= 2 ? (int)zen::to_integer(args[1]) : 0;
    args[0] = zen::val_int(gZenUserData ? gZenUserData->getInt(key, fallback) : fallback);
    return 1;
}

int natUserDataSetInt(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    if (gZenUserData && nargs >= 2)
        gZenUserData->setInt(valueToCString(vm, args[0], small, sizeof(small)), (int)zen::to_integer(args[1]));
    return 0;
}

int natUserDataGetFloat(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const float fallback = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
    args[0] = zen::val_float(gZenUserData ? gZenUserData->getFloat(key, fallback) : fallback);
    return 1;
}

int natUserDataSetFloat(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    if (gZenUserData && nargs >= 2)
        gZenUserData->setFloat(valueToCString(vm, args[0], small, sizeof(small)), (float)zen::to_number(args[1]));
    return 0;
}

int natUserDataGetString(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    char smallFallback[64];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const char* fallback = nargs >= 2 ? valueToCString(vm, args[1], smallFallback, sizeof(smallFallback)) : "";
    const char* value = gZenUserData ? gZenUserData->getString(key, fallback) : fallback;
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(value));
    return 1;
}

int natUserDataSetString(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    char smallValue[64];
    if (gZenUserData && nargs >= 2)
        gZenUserData->setString(valueToCString(vm, args[0], small, sizeof(small)),
                                valueToCString(vm, args[1], smallValue, sizeof(smallValue)));
    return 0;
}

int natUserDataGetBool(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const bool fallback = nargs >= 2 && zen::is_truthy(args[1]);
    args[0] = zen::val_bool(gZenUserData ? gZenUserData->getBool(key, fallback) : fallback);
    return 1;
}

int natUserDataSetBool(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    if (gZenUserData && nargs >= 2)
        gZenUserData->setBool(valueToCString(vm, args[0], small, sizeof(small)), zen::is_truthy(args[1]));
    return 0;
}

int natUserDataHas(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* key = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_bool(gZenUserData && gZenUserData->has(key));
    return 1;
}

int natUserDataDelete(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    if (gZenUserData && nargs >= 1)
        gZenUserData->erase(valueToCString(vm, args[0], small, sizeof(small)));
    return 0;
}

int natUserDataClear(zen::VM*, zen::Value*, int)
{
    if (gZenUserData)
        gZenUserData->clear();
    return 0;
}

int natUserDataLoad(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const bool loaded =
        gZenUserData &&
        (nargs >= 1 ? gZenUserData->load(valueToCString(vm, args[0], small, sizeof(small))) : gZenUserData->load());
    args[0] = zen::val_bool(loaded);
    return 1;
}

int natUserDataSave(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const bool saved =
        gZenUserData &&
        (nargs >= 1 ? gZenUserData->save(valueToCString(vm, args[0], small, sizeof(small))) : gZenUserData->save());
    args[0] = zen::val_bool(saved);
    return 1;
}

int natUserDataReadText(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    char smallFallback[64];
    const char* fileName = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    const char* fallback = nargs >= 2 ? valueToCString(vm, args[1], smallFallback, sizeof(smallFallback)) : "";
    ct::String text = fallback;
    if (gZenUserData)
        gZenUserData->readText(fileName, text);
    args[0] = zen::val_obj((zen::Obj*)vm->make_string(text.c_str()));
    return 1;
}

int natUserDataWriteText(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    char smallText[64];
    const bool saved = gZenUserData && nargs >= 2 &&
                       gZenUserData->writeText(valueToCString(vm, args[0], small, sizeof(small)),
                                               ct::String(valueToCString(vm, args[1], smallText, sizeof(smallText))));
    args[0] = zen::val_bool(saved);
    return 1;
}

int natEmit(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[16];
    if (nargs >= 1)
        ZenBlackboard::emit(valueToCString(vm, args[0], small, sizeof(small)),
                            nargs >= 2 ? zen::to_number(args[1]) : 0.0);
    return 0;
}

int natKeyDown(zen::VM* vm, zen::Value* args, int nargs)
{
    (void)vm;
    const int code = nargs >= 1 ? keyCode(args[0]) : -1;
    args[0] = zen::val_bool(gZenInput && code >= 0 && gZenInput->KeyDown(code));
    return 1;
}

int natKeyPressed(zen::VM* vm, zen::Value* args, int nargs)
{
    (void)vm;
    const int code = nargs >= 1 ? keyCode(args[0]) : -1;
    args[0] = zen::val_bool(gZenInput && code >= 0 && gZenInput->KeyPressed(code));
    return 1;
}

int natKeyReleased(zen::VM* vm, zen::Value* args, int nargs)
{
    (void)vm;
    const int code = nargs >= 1 ? keyCode(args[0]) : -1;
    args[0] = zen::val_bool(gZenInput && code >= 0 && gZenInput->KeyReleased(code));
    return 1;
}

int natVirtualKeyAdd(zen::VM*, zen::Value* args, int nargs)
{
    if (gZenVirtualPad && nargs >= 5)
        gZenVirtualPad->AddVirtualKey(keyCode(args[0]), (float)zen::to_number(args[1]), (float)zen::to_number(args[2]),
                                      (float)zen::to_number(args[3]), (float)zen::to_number(args[4]));
    return 0;
}

int natVirtualKeysClear(zen::VM*, zen::Value*, int)
{
    if (gZenVirtualPad)
        gZenVirtualPad->ClearVirtualKeys();
    return 0;
}

int natVirtualKeysSetVisible(zen::VM*, zen::Value* args, int nargs)
{
    if (gZenVirtualPad && nargs >= 1)
        gZenVirtualPad->SetVirtualKeysVisible(zen::is_truthy(args[0]));
    return 0;
}

int natVirtualKeysVisible(zen::VM*, zen::Value* args, int)
{
    args[0] = zen::val_bool(gZenVirtualPad && gZenVirtualPad->VirtualKeysVisible());
    return 1;
}

int natActionDown(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* action = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_bool(gZenInput && GetInputActions().Down(*gZenInput, action));
    return 1;
}

int natActionPressed(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* action = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_bool(gZenInput && GetInputActions().Pressed(*gZenInput, action));
    return 1;
}

int natActionReleased(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[64];
    const char* action = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_bool(gZenInput && GetInputActions().Released(*gZenInput, action));
    return 1;
}

int natFadeIn(zen::VM*, zen::Value* args, int nargs)
{
    FadeIn(nargs >= 1 ? static_cast<float>(zen::to_number(args[0])) : 0.0f);
    return 0;
}

int natFadeOut(zen::VM*, zen::Value* args, int nargs)
{
    FadeOut(nargs >= 1 ? static_cast<float>(zen::to_number(args[0])) : 0.0f);
    return 0;
}

int natIsFading(zen::VM* vm, zen::Value* args, int)
{
    args[0] = zen::val_bool(IsFading());
    return 1;
}

int natFadeProgress(zen::VM* vm, zen::Value* args, int)
{
    args[0] = zen::val_float(FadeProgress());
    return 1;
}

int natLoadScene(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[256];
    const char* path = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    if (path[0])
        GetSceneManager().Request(path);
    return 0;
}

int natAudioLoad(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[256];
    const char* path = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_int(GetAudio().LoadSound(path));
    return 1;
}

int natAudioLoadMusic(zen::VM* vm, zen::Value* args, int nargs)
{
    char small[256];
    const char* path = nargs >= 1 ? valueToCString(vm, args[0], small, sizeof(small)) : "";
    args[0] = zen::val_int(GetAudio().LoadMusic(path));
    return 1;
}

int natAudioPlay(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::SoundId sound = nargs >= 1 ? (AudioEngine::SoundId)zen::to_integer(args[0]) : 0;
    const float volume = nargs >= 2 ? (float)zen::to_number(args[1]) : 1.0f;
    const float pitch = nargs >= 3 ? (float)zen::to_number(args[2]) : 1.0f;
    const float pan = nargs >= 4 ? (float)zen::to_number(args[3]) : 0.0f;
    args[0] = zen::val_int(GetAudio().Play(sound, volume, pitch, pan));
    return 1;
}

int natAudioPlayAt(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::SoundId sound = nargs >= 1 ? (AudioEngine::SoundId)zen::to_integer(args[0]) : 0;
    const float x = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
    const float y = nargs >= 3 ? (float)zen::to_number(args[2]) : 0.0f;
    const float volume = nargs >= 4 ? (float)zen::to_number(args[3]) : 1.0f;
    const float pitch = nargs >= 5 ? (float)zen::to_number(args[4]) : 1.0f;
    const float minDistance = nargs >= 6 ? (float)zen::to_number(args[5]) : 64.0f;
    const float maxDistance = nargs >= 7 ? (float)zen::to_number(args[6]) : 1024.0f;
    args[0] = zen::val_int(GetAudio().PlayAt(sound, Math::Vec2(x, y), volume, pitch, minDistance, maxDistance));
    return 1;
}

int natAudioPlayMusic(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::SoundId sound = nargs >= 1 ? (AudioEngine::SoundId)zen::to_integer(args[0]) : 0;
    const bool loop = nargs < 2 || zen::to_integer(args[1]) != 0;
    const float volume = nargs >= 3 ? (float)zen::to_number(args[2]) : 1.0f;
    args[0] = zen::val_int(GetAudio().PlayMusic(sound, loop, volume));
    return 1;
}

int natAudioCrossfadeMusic(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::SoundId sound = nargs >= 1 ? (AudioEngine::SoundId)zen::to_integer(args[0]) : 0;
    const bool loop = nargs < 2 || zen::to_integer(args[1]) != 0;
    const float volume = nargs >= 3 ? (float)zen::to_number(args[2]) : 1.0f;
    const float seconds = nargs >= 4 ? (float)zen::to_number(args[3]) : 1.0f;
    args[0] = zen::val_int(GetAudio().CrossfadeMusic(sound, loop, volume, seconds));
    return 1;
}

int natAudioStop(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::VoiceId voice = nargs >= 1 ? (AudioEngine::VoiceId)zen::to_integer(args[0]) : 0;
    args[0] = zen::val_bool(GetAudio().Stop(voice));
    return 1;
}

int natAudioPause(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::VoiceId voice = nargs >= 1 ? (AudioEngine::VoiceId)zen::to_integer(args[0]) : 0;
    args[0] = zen::val_bool(GetAudio().Pause(voice));
    return 1;
}

int natAudioResume(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::VoiceId voice = nargs >= 1 ? (AudioEngine::VoiceId)zen::to_integer(args[0]) : 0;
    args[0] = zen::val_bool(GetAudio().Resume(voice));
    return 1;
}

int natAudioPlaying(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::VoiceId voice = nargs >= 1 ? (AudioEngine::VoiceId)zen::to_integer(args[0]) : 0;
    args[0] = zen::val_bool(GetAudio().IsPlaying(voice));
    return 1;
}

int natAudioFadeIn(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::VoiceId voice = nargs >= 1 ? (AudioEngine::VoiceId)zen::to_integer(args[0]) : 0;
    const float seconds = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
    args[0] = zen::val_bool(GetAudio().FadeIn(voice, seconds));
    return 1;
}

int natAudioFadeOut(zen::VM*, zen::Value* args, int nargs)
{
    const AudioEngine::VoiceId voice = nargs >= 1 ? (AudioEngine::VoiceId)zen::to_integer(args[0]) : 0;
    const float seconds = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
    const bool stopWhenDone = nargs < 3 || zen::to_integer(args[2]) != 0;
    args[0] = zen::val_bool(GetAudio().FadeOut(voice, seconds, stopWhenDone));
    return 1;
}

int natAudioSetListenerPosition(zen::VM*, zen::Value* args, int nargs)
{
    const float x = nargs >= 1 ? (float)zen::to_number(args[0]) : 0.0f;
    const float y = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
    args[0] = zen::val_bool(GetAudio().SetListenerPosition(Math::Vec2(x, y)));
    return 1;
}

int natAudioStopAll(zen::VM*, zen::Value*, int)
{
    GetAudio().StopAll();
    return 0;
}

int natAudioStopMusic(zen::VM*, zen::Value*, int)
{
    GetAudio().StopMusic();
    return 0;
}

int natAudioSetMasterVolume(zen::VM*, zen::Value* args, int nargs)
{
    GetAudio().SetMasterVolume(nargs >= 1 ? (float)zen::to_number(args[0]) : 1.0f);
    return 0;
}

int natAudioSetSfxVolume(zen::VM*, zen::Value* args, int nargs)
{
    GetAudio().SetSfxVolume(nargs >= 1 ? (float)zen::to_number(args[0]) : 1.0f);
    return 0;
}

int natAudioSetMusicVolume(zen::VM*, zen::Value* args, int nargs)
{
    GetAudio().SetMusicVolume(nargs >= 1 ? (float)zen::to_number(args[0]) : 1.0f);
    return 0;
}

int natAudioSetMasterMuted(zen::VM*, zen::Value* args, int nargs)
{
    GetAudio().SetMasterMuted(nargs >= 1 && zen::to_integer(args[0]) != 0);
    return 0;
}

int natAudioSetSfxMuted(zen::VM*, zen::Value* args, int nargs)
{
    GetAudio().SetSfxMuted(nargs >= 1 && zen::to_integer(args[0]) != 0);
    return 0;
}

int natAudioSetMusicMuted(zen::VM*, zen::Value* args, int nargs)
{
    GetAudio().SetMusicMuted(nargs >= 1 && zen::to_integer(args[0]) != 0);
    return 0;
}

int natAudioMasterMuted(zen::VM* vm, zen::Value* args, int)
{
    args[0] = zen::val_bool(GetAudio().MasterMuted());
    return 1;
}

int natAudioSfxMuted(zen::VM* vm, zen::Value* args, int)
{
    args[0] = zen::val_bool(GetAudio().SfxMuted());
    return 1;
}

int natAudioMusicMuted(zen::VM* vm, zen::Value* args, int)
{
    args[0] = zen::val_bool(GetAudio().MusicMuted());
    return 1;
}

int natMouseDown(zen::VM*, zen::Value* args, int nargs)
{
    const int button = nargs >= 1 ? (int)zen::to_integer(args[0]) : 0;
    const bool inside = !gZenGameViewport.valid || (gZenInput && gZenInput->MouseX() >= gZenGameViewport.x &&
                                                    gZenInput->MouseX() < gZenGameViewport.x + gZenGameViewport.width &&
                                                    gZenInput->MouseY() >= gZenGameViewport.y &&
                                                    gZenInput->MouseY() < gZenGameViewport.y + gZenGameViewport.height);
    args[0] = zen::val_bool(gZenInput && inside && gZenInput->MouseDown(button));
    return 1;
}

int natMousePressed(zen::VM*, zen::Value* args, int nargs)
{
    const int button = nargs >= 1 ? (int)zen::to_integer(args[0]) : 0;
    const bool inside = !gZenGameViewport.valid || (gZenInput && gZenInput->MouseX() >= gZenGameViewport.x &&
                                                    gZenInput->MouseX() < gZenGameViewport.x + gZenGameViewport.width &&
                                                    gZenInput->MouseY() >= gZenGameViewport.y &&
                                                    gZenInput->MouseY() < gZenGameViewport.y + gZenGameViewport.height);
    args[0] = zen::val_bool(gZenInput && inside && gZenInput->MousePressed(button));
    return 1;
}

int natMouseX(zen::VM*, zen::Value* args, int)
{
    args[0] =
        zen::val_float(gZenInput ? gZenInput->MouseX() - (gZenGameViewport.valid ? gZenGameViewport.x : 0.0f) : 0.0);
    return 1;
}

int natMouseY(zen::VM*, zen::Value* args, int)
{
    args[0] =
        zen::val_float(gZenInput ? gZenInput->MouseY() - (gZenGameViewport.valid ? gZenGameViewport.y : 0.0f) : 0.0);
    return 1;
}

int natViewportWidth(zen::VM*, zen::Value* args, int)
{
    args[0] = zen::val_float(gZenGameViewport.valid ? gZenGameViewport.width : 1280.0f);
    return 1;
}

int natViewportHeight(zen::VM*, zen::Value* args, int)
{
    args[0] = zen::val_float(gZenGameViewport.valid ? gZenGameViewport.height : 720.0f);
    return 1;
}

int natGetFps(zen::VM*, zen::Value* args, int)
{
    args[0] = zen::val_float(gZenFps);
    return 1;
}

int natProfilerVisible(zen::VM*, zen::Value* args, int)
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

int natScreenToWorld(zen::VM*, zen::Value* args, int nargs)
{
    const float x = nargs >= 1 ? (float)zen::to_number(args[0]) : 0.0f;
    const float y = nargs >= 2 ? (float)zen::to_number(args[1]) : 0.0f;
    const Math::Vec2 point = screenToWorld(x, y);
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natMouseWorldPosition(zen::VM*, zen::Value* args, int)
{
    const float x = gZenInput ? gZenInput->MouseX() - (gZenGameViewport.valid ? gZenGameViewport.x : 0.0f) : 0.0f;
    const float y = gZenInput ? gZenInput->MouseY() - (gZenGameViewport.valid ? gZenGameViewport.y : 0.0f) : 0.0f;
    const Math::Vec2 point = screenToWorld(x, y);
    args[0] = zen::val_float(point.x);
    args[1] = zen::val_float(point.y);
    return 2;
}

int natWorldViewRect(zen::VM*, zen::Value* args, int)
{
    const float width = gZenGameViewport.valid ? gZenGameViewport.width : 1280.0f;
    const float height = gZenGameViewport.valid ? gZenGameViewport.height : 720.0f;
    const Math::Vec2 corners[4] = {screenToWorld(0.0f, 0.0f), screenToWorld(width, 0.0f), screenToWorld(width, height),
                                   screenToWorld(0.0f, height)};
    float minX = corners[0].x;
    float minY = corners[0].y;
    float maxX = corners[0].x;
    float maxY = corners[0].y;
    for (int i = 1; i < 4; ++i)
    {
        if (corners[i].x < minX)
            minX = corners[i].x;
        if (corners[i].y < minY)
            minY = corners[i].y;
        if (corners[i].x > maxX)
            maxX = corners[i].x;
        if (corners[i].y > maxY)
            maxY = corners[i].y;
    }
    args[0] = zen::val_float(minX);
    args[1] = zen::val_float(minY);
    args[2] = zen::val_float(maxX);
    args[3] = zen::val_float(maxY);
    return 4;
}

int natWheelY(zen::VM*, zen::Value* args, int)
{
    args[0] = zen::val_float(gZenInput ? gZenInput->WheelY() : 0.0);
    return 1;
}

// AStarGrid2D and AStar2D are script-owned values, not components: a Zen
// script constructs one with ClassName() and the VM frees it via the native
// dtor when the wrapping instance is garbage collected, following the
// ctor/dtor ClassBuilder pattern zen's own embedding tests use for owned
// native data (test_embedding.cpp's Sprite), since no component in this file
// is script-constructible.
AStarGrid2D* gridFromSelf(zen::Value* args)
{
    return zen::zen_instance_data<AStarGrid2D>(args[-1]);
}

void* natAStarGridCtor(zen::VM*, int, zen::Value*)
{
    return new AStarGrid2D();
}

void natAStarGridDtor(zen::VM*, void* data)
{
    delete static_cast<AStarGrid2D*>(data);
}

int natAStarGridSetSize(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 2)
            grid->SetSize((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]));
    return 0;
}

int natAStarGridSetCellSize(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 2)
            grid->SetCellSize((float)zen::to_number(args[0]), (float)zen::to_number(args[1]));
    return 0;
}

int natAStarGridSetOffset(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 2)
            grid->SetOffset(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    return 0;
}

int natAStarGridSetHeuristic(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 1)
            grid->SetHeuristic(static_cast<AStarGrid2D::Heuristic>((int)zen::to_integer(args[0])));
    return 0;
}

int natAStarGridSetDiagonalMode(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 1)
            grid->SetDiagonalMode(static_cast<AStarGrid2D::DiagonalMode>((int)zen::to_integer(args[0])));
    return 0;
}

int natAStarGridClear(zen::VM*, zen::Value* args, int)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        grid->Clear();
    return 0;
}

int natAStarGridSetSolid(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 2)
            grid->SetSolid((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]),
                           nargs < 3 || zen::is_truthy(args[2]));
    return 0;
}

int natAStarGridIsSolid(zen::VM*, zen::Value* args, int nargs)
{
    AStarGrid2D* grid = gridFromSelf(args);
    args[0] = zen::val_bool(grid && nargs >= 2 &&
                            grid->IsSolid((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1])));
    return 1;
}

int natAStarGridFillSolidRegion(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 4)
            grid->FillSolidRegion((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]),
                                  (int)zen::to_integer(args[2]), (int)zen::to_integer(args[3]),
                                  nargs < 5 || zen::is_truthy(args[4]));
    return 0;
}

int natAStarGridSetWeightScale(zen::VM*, zen::Value* args, int nargs)
{
    if (AStarGrid2D* grid = gridFromSelf(args))
        if (nargs >= 3)
            grid->SetWeightScale((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]),
                                 (float)zen::to_number(args[2]));
    return 0;
}

int natAStarGridGetPointPosition(zen::VM*, zen::Value* args, int nargs)
{
    AStarGrid2D* grid = gridFromSelf(args);
    const Math::Vec2 pos = grid && nargs >= 2
                               ? grid->GetPointPosition((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]))
                               : Math::Vec2(0.0f, 0.0f);
    args[0] = zen::val_float(pos.x);
    args[1] = zen::val_float(pos.y);
    return 2;
}

int natAStarGridGetPointPath(zen::VM* vm, zen::Value* args, int nargs)
{
    AStarGrid2D* grid = gridFromSelf(args);
    ct::Vector<Math::Vec2> path;
    if (grid && nargs >= 4)
    {
        const bool allowPartial = nargs >= 5 && zen::is_truthy(args[4]);
        grid->GetPointPath(IVec2((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1])),
                           IVec2((int)zen::to_integer(args[2]), (int)zen::to_integer(args[3])), path, allowPartial);
    }
    args[0] = packPointArray(vm, path);
    return 1;
}

AStar2D* astarGraphFromSelf(zen::Value* args)
{
    return zen::zen_instance_data<AStar2D>(args[-1]);
}

void* natAStarGraphCtor(zen::VM*, int, zen::Value*)
{
    return new AStar2D();
}

void natAStarGraphDtor(zen::VM*, void* data)
{
    delete static_cast<AStar2D*>(data);
}

int natAStarGraphAddPoint(zen::VM*, zen::Value* args, int nargs)
{
    if (AStar2D* graph = astarGraphFromSelf(args))
        if (nargs >= 3)
            graph->AddPoint((int)zen::to_integer(args[0]),
                            Math::Vec2((float)zen::to_number(args[1]), (float)zen::to_number(args[2])),
                            nargs >= 4 ? (float)zen::to_number(args[3]) : 1.0f);
    return 0;
}

int natAStarGraphRemovePoint(zen::VM*, zen::Value* args, int nargs)
{
    if (AStar2D* graph = astarGraphFromSelf(args))
        if (nargs >= 1)
            graph->RemovePoint((int)zen::to_integer(args[0]));
    return 0;
}

int natAStarGraphHasPoint(zen::VM*, zen::Value* args, int nargs)
{
    AStar2D* graph = astarGraphFromSelf(args);
    args[0] = zen::val_bool(graph && nargs >= 1 && graph->HasPoint((int)zen::to_integer(args[0])));
    return 1;
}

int natAStarGraphConnectPoints(zen::VM*, zen::Value* args, int nargs)
{
    if (AStar2D* graph = astarGraphFromSelf(args))
        if (nargs >= 2)
            graph->ConnectPoints((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]),
                                 nargs < 3 || zen::is_truthy(args[2]));
    return 0;
}

int natAStarGraphDisconnectPoints(zen::VM*, zen::Value* args, int nargs)
{
    if (AStar2D* graph = astarGraphFromSelf(args))
        if (nargs >= 2)
            graph->DisconnectPoints((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]),
                                    nargs < 3 || zen::is_truthy(args[2]));
    return 0;
}

int natAStarGraphSetPointDisabled(zen::VM*, zen::Value* args, int nargs)
{
    if (AStar2D* graph = astarGraphFromSelf(args))
        if (nargs >= 1)
            graph->SetPointDisabled((int)zen::to_integer(args[0]), nargs < 2 || zen::is_truthy(args[1]));
    return 0;
}

int natAStarGraphGetClosestPoint(zen::VM*, zen::Value* args, int nargs)
{
    AStar2D* graph = astarGraphFromSelf(args);
    const int id =
        graph && nargs >= 2
            ? graph->GetClosestPoint(Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])),
                                     nargs >= 3 && zen::is_truthy(args[2]))
            : -1;
    args[0] = zen::val_int(id);
    return 1;
}

int natAStarGraphGetPointPath(zen::VM* vm, zen::Value* args, int nargs)
{
    AStar2D* graph = astarGraphFromSelf(args);
    ct::Vector<Math::Vec2> path;
    if (graph && nargs >= 2)
    {
        const bool allowPartial = nargs >= 3 && zen::is_truthy(args[2]);
        graph->GetPointPath((int)zen::to_integer(args[0]), (int)zen::to_integer(args[1]), path, allowPartial);
    }
    args[0] = packPointArray(vm, path);
    return 1;
}

int natAStarGraphClear(zen::VM*, zen::Value* args, int)
{
    if (AStar2D* graph = astarGraphFromSelf(args))
        graph->Clear();
    return 0;
}

int natAStarGraphGetPointCount(zen::VM*, zen::Value* args, int)
{
    AStar2D* graph = astarGraphFromSelf(args);
    args[0] = zen::val_int(graph ? graph->GetPointCount() : 0);
    return 1;
}

int natNavPath(zen::VM* vm, zen::Value* args, int nargs)
{
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    ct::Vector<Math::Vec2> path;
    if (scene && nargs >= 4)
        Navigation2D::GetPath(*scene, Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])),
                              Math::Vec2((float)zen::to_number(args[2]), (float)zen::to_number(args[3])), path);
    args[0] = packPointArray(vm, path);
    return 1;
}

int natNavPointFree(zen::VM*, zen::Value* args, int nargs)
{
    Scene* scene = gZenCallbackNode ? gZenCallbackNode->scene() : nullptr;
    bool inside = false;
    if (scene && nargs >= 2)
        inside = Navigation2D::Contains(
            *scene, Math::Vec2((float)zen::to_number(args[0]), (float)zen::to_number(args[1])));
    args[0] = zen::val_bool(inside);
    return 1;
}
} // namespace

// A component is about to leave its object: drop the script handle cached for
// its address. The handles are persistent objects, so nothing else would ever
// free them, and the address is reused by the next component allocated there.
static void forgetRemovedComponent(Component* component, void* user)
{
    static_cast<ZenRuntime::Impl*>(user)->forgetInstance(component);
}

void ZenRuntime::Impl::initialize()
{
    zen_host_set_writer(&zenHostWriter, nullptr);
    Component::SetRemovedCallback(&forgetRemovedComponent, this);

    vm.open_lib_globals(&zen::zen_lib_base);
    vm.register_lib(&zen::zen_lib_math);
    vm.register_lib(&zen::zen_lib_time);
    vm.register_lib(&zen::zen_lib_json);
    vm.register_lib(&zen::zen_lib_net);
    vm.register_lib(&zen::zen_lib_http);

    vm.def_global("__k2d", zen::val_ptr(this));
    struct KeyConstant
    {
        const char* name;
        Key key;
    };
    static const KeyConstant keyConstants[] = {
        {"KEY_A", Key::A},
        {"KEY_B", Key::B},
        {"KEY_C", Key::C},
        {"KEY_D", Key::D},
        {"KEY_E", Key::E},
        {"KEY_F", Key::F},
        {"KEY_G", Key::G},
        {"KEY_H", Key::H},
        {"KEY_I", Key::I},
        {"KEY_J", Key::J},
        {"KEY_K", Key::K},
        {"KEY_L", Key::L},
        {"KEY_M", Key::M},
        {"KEY_N", Key::N},
        {"KEY_O", Key::O},
        {"KEY_P", Key::P},
        {"KEY_Q", Key::Q},
        {"KEY_R", Key::R},
        {"KEY_S", Key::S},
        {"KEY_T", Key::T},
        {"KEY_U", Key::U},
        {"KEY_V", Key::V},
        {"KEY_W", Key::W},
        {"KEY_X", Key::X},
        {"KEY_Y", Key::Y},
        {"KEY_Z", Key::Z},
        {"KEY_0", Key::Num0},
        {"KEY_1", Key::Num1},
        {"KEY_2", Key::Num2},
        {"KEY_3", Key::Num3},
        {"KEY_4", Key::Num4},
        {"KEY_5", Key::Num5},
        {"KEY_6", Key::Num6},
        {"KEY_7", Key::Num7},
        {"KEY_8", Key::Num8},
        {"KEY_9", Key::Num9},
        {"KEY_SPACE", Key::Space},
        {"KEY_ENTER", Key::Enter},
        {"KEY_ESCAPE", Key::Escape},
        {"KEY_TAB", Key::Tab},
        {"KEY_BACKSPACE", Key::Backspace},
        {"KEY_LEFT", Key::Left},
        {"KEY_RIGHT", Key::Right},
        {"KEY_UP", Key::Up},
        {"KEY_DOWN", Key::Down},
        {"KEY_F1", Key::F1},
        {"KEY_F2", Key::F2},
        {"KEY_F3", Key::F3},
        {"KEY_F4", Key::F4},
        {"KEY_F5", Key::F5},
        {"KEY_F6", Key::F6},
        {"KEY_F7", Key::F7},
        {"KEY_F8", Key::F8},
        {"KEY_F9", Key::F9},
        {"KEY_F10", Key::F10},
        {"KEY_F11", Key::F11},
        {"KEY_F12", Key::F12},
        {"KEY_LEFT_CTRL", Key::LeftCtrl},
        {"KEY_RIGHT_CTRL", Key::RightCtrl},
        {"KEY_LEFT_SHIFT", Key::LeftShift},
        {"KEY_RIGHT_SHIFT", Key::RightShift},
        {"KEY_LEFT_ALT", Key::LeftAlt},
    };
    for (const KeyConstant& entry : keyConstants)
        vm.def_global(entry.name, zen::val_int(static_cast<int>(entry.key)));

    auto component = vm.def_class("Component");
    component.method("is_active", &natComponentIsActive, 0);
    component.method("set_active", &natComponentSetActive, 1);
    component.persistent(true).constructable(false);
    component.end();

    auto scriptComponent = vm.def_class("ScriptComponent");
    scriptComponent.parent("Component");
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
    node.method("place_free", &natNodePlaceFree, 2);
    node.method("place_meeting", &natNodePlaceMeeting, 2);
    node.method("move_and_collide", &natNodeMoveAndCollide, 2);
    node.method("set_character_velocity", &natNodeSetCharacterVelocity, 2);
    node.method("get_character_velocity", &natNodeGetCharacterVelocity, 0);
    node.method("move_and_slide", &natNodeMoveAndSlide, -1);
    node.method("slide_collision_count", &natNodeSlideCollisionCount, 0);
    node.method("slide_collision", &natNodeSlideCollision, 1);
    node.method("get_sprite", &natNodeGetSprite, 0);
    node.method("get_animation", &natNodeGetAnimation, 0);
    node.method("get_camera", &natNodeGetCamera, 0);
    node.method("get_particle", &natNodeGetParticle, 0);
    node.method("get_button", &natNodeGetButton, 0);
    node.method("get_checkbox", &natNodeGetCheckBox, 0);
    node.method("get_slider", &natNodeGetSlider, 0);
    node.method("get_component", &natNodeGetComponent, 1);
    node.method("add_component", &natNodeAddComponent, 1);
    node.method("has_component", &natNodeHasComponent, 1);
    node.method("remove_component", &natNodeRemoveComponent, 1);
    node.method("get_id", &natNodeGetId, 0);
    node.method("set_name", &natNodeSetName, 1);
    node.method("get_tag", &natNodeGetTag, 0);
    node.method("set_tag", &natNodeSetTag, 1);
    node.method("get_global_position", &natNodeGetGlobalPosition, 0);
    node.method("get_right", &natNodeGetRight, 0);
    node.method("get_up", &natNodeGetUp, 0);
    node.method("is_active_in_hierarchy", &natNodeIsActiveInHierarchy, 0);
    node.method("is_visible_in_hierarchy", &natNodeIsVisibleInHierarchy, 0);
    node.method("get_root", &natNodeGetRoot, 0);
    node.method("reparent", &natNodeReparent, 1);
    node.persistent(true).constructable(false);
    nodeClass = node.end();

    auto sprite = vm.def_class("Sprite");
    sprite.parent("Component");
    sprite.method("set_color", &natSpriteSetColor, 4);
    sprite.method("set_flip", &natSpriteSetFlip, 2);
    sprite.method("set_size", &natSpriteSetSize, 2);
    sprite.method("set_water_enabled", &natSpriteSetWaterEnabled, 1);
    sprite.method("set_water_flow", &natSpriteSetWaterFlow, 4);
    sprite.method("set_water_strength", &natSpriteSetWaterStrength, 1);
    sprite.persistent(true).constructable(false);
    spriteClass = sprite.end();

    auto animation = vm.def_class("Animation");
    animation.parent("Component");
    animation.method("play", &natAnimationPlay, -1);
    animation.method("stop", &natAnimationStop, 0);
    animation.method("is_playing", &natAnimationIsPlaying, 0);
    animation.method("current", &natAnimationCurrent, 0);
    animation.persistent(true).constructable(false);
    animationClass = animation.end();

    auto camera = vm.def_class("Camera");
    camera.parent("Component");
    camera.method("start_shake", &natCameraStartShake, 4);
    camera.method("stop_shake", &natCameraStopShake, 0);
    camera.method("add_trauma", &natCameraAddTrauma, 1);
    camera.method("set_trauma_profile", &natCameraSetTraumaProfile, 4);
    camera.method("clear_trauma", &natCameraClearTrauma, 0);
    camera.method("start_zoom_punch", &natCameraStartZoomPunch, 2);
    camera.method("stop_zoom_punch", &natCameraStopZoomPunch, 0);
    camera.method("is_shaking", &natCameraIsShaking, 0);
    camera.persistent(true).constructable(false);
    cameraClass = camera.end();

    auto rigidBody = vm.def_class("RigidBody");
    rigidBody.parent("Component");
    rigidBody.method("get_velocity", &natBodyGetVelocity, 0);
    rigidBody.method("set_velocity", &natBodySetVelocity, 2);
    rigidBody.method("get_angular_velocity", &natBodyGetAngularVelocity, 0);
    rigidBody.method("set_angular_velocity", &natBodySetAngularVelocity, 1);
    rigidBody.method("apply_force", &natBodyApplyForce, 2);
    rigidBody.method("apply_impulse", &natBodyApplyImpulse, 2);
    rigidBody.method("apply_torque", &natBodyApplyTorque, 1);
    rigidBody.method("get_gravity_scale", &natBodyGetGravityScale, 0);
    rigidBody.method("set_gravity_scale", &natBodySetGravityScale, 1);
    rigidBody.method("set_type", &natBodySetType, 1);
    rigidBody.method("is_awake", &natBodyIsAwake, 0);
    rigidBody.method("wake", &natBodyWake, 0);
    rigidBody.persistent(true).constructable(false);
    rigidBodyClass = rigidBody.end();
    /* Keep Body as a source-compatible alias for older scripts. */
    vm.def_global("Body", zen::val_obj((zen::Obj*)rigidBodyClass));

    auto particle = vm.def_class("Particle");
    particle.parent("Component");
    particle.method("start", &natParticleStart, 0);
    particle.method("stop", &natParticleStop, 0);
    particle.method("reset", &natParticleReset, 0);
    particle.method("burst", &natParticleBurst, 1);
    particle.method("is_playing", &natParticleIsPlaying, 0);
    particle.persistent(true).constructable(false);
    particleClass = particle.end();

    auto button = vm.def_class("Button");
    button.parent("Component");
    button.method("clicked", &natButtonClicked, 0);
    button.method("set_text", &natButtonSetText, 1);
    button.persistent(true).constructable(false);
    buttonClass = button.end();

    auto checkBox = vm.def_class("CheckBox");
    checkBox.parent("Component");
    checkBox.method("is_checked", &natCheckBoxGetChecked, 0);
    checkBox.method("set_checked", &natCheckBoxSetChecked, 1);
    checkBox.method("changed", &natCheckBoxChanged, 0);
    checkBox.persistent(true).constructable(false);
    checkBoxClass = checkBox.end();

    auto slider = vm.def_class("Slider");
    slider.parent("Component");
    slider.method("get_value", &natSliderGetValue, 0);
    slider.method("set_value", &natSliderSetValue, 1);
    slider.method("changed", &natSliderChanged, 0);
    slider.persistent(true).constructable(false);
    sliderClass = slider.end();

    auto defineHandle = [&](const char* name, const char* parent = "Component") -> zen::ObjClass* {
        auto handle = vm.def_class(name);
        if (parent)
            handle.parent(parent);
        handle.persistent(true).constructable(false);
        return handle.end();
    };

    auto characterBody = vm.def_class("CharacterBody");
    characterBody.parent("Component");
    characterBody.method("get_velocity", &natCharacterGetVelocity, 0);
    characterBody.method("set_velocity", &natCharacterSetVelocity, 2);
    characterBody.method("get_safe_margin", &natCharacterGetSafeMargin, 0);
    characterBody.method("set_safe_margin", &natCharacterSetSafeMargin, 1);
    characterBody.method("get_max_slides", &natCharacterGetMaxSlides, 0);
    characterBody.method("set_max_slides", &natCharacterSetMaxSlides, 1);
    characterBody.method("is_on_floor", &natCharacterIsOnFloor, 0);
    characterBody.method("is_on_wall", &natCharacterIsOnWall, 0);
    characterBody.method("is_on_ceiling", &natCharacterIsOnCeiling, 0);
    characterBody.persistent(true).constructable(false);
    zen::ObjClass* characterBodyClass = characterBody.end();

    auto collider = vm.def_class("Collider");
    collider.parent("Component");
    collider.method("get_offset", &natColliderGetOffset, 0);
    collider.method("set_offset", &natColliderSetOffset, 2);
    collider.method("is_sensor", &natColliderIsSensor, 0);
    collider.method("set_sensor", &natColliderSetSensor, 1);
    collider.method("get_filter", &natColliderGetFilter, 0);
    collider.method("set_filter", &natColliderSetFilter, 2);
    collider.method("get_shape_count", &natColliderShapeCount, 0);
    collider.method("is_attached", &natColliderAttached, 0);
    collider.persistent(true).constructable(false);
    zen::ObjClass* colliderClass = collider.end();

    auto boxCollider = vm.def_class("BoxCollider");
    boxCollider.parent("Collider");
    boxCollider.method("get_size", &natBoxColliderGetSize, 0);
    boxCollider.method("set_size", &natBoxColliderSetSize, 2);
    boxCollider.persistent(true).constructable(false);
    zen::ObjClass* boxColliderClass = boxCollider.end();

    auto circleCollider = vm.def_class("CircleCollider");
    circleCollider.parent("Collider");
    circleCollider.method("get_radius", &natCircleColliderGetRadius, 0);
    circleCollider.method("set_radius", &natCircleColliderSetRadius, 1);
    circleCollider.persistent(true).constructable(false);
    zen::ObjClass* circleColliderClass = circleCollider.end();

    auto edgeCollider = vm.def_class("EdgeCollider");
    edgeCollider.parent("Collider");
    edgeCollider.method("get_points", &natEdgeColliderGetPoints, 0);
    edgeCollider.method("set_points", &natEdgeColliderSetPoints, 4);
    edgeCollider.persistent(true).constructable(false);
    zen::ObjClass* edgeColliderClass = edgeCollider.end();

    auto polygonCollider = vm.def_class("PolygonCollider");
    polygonCollider.parent("Collider");
    polygonCollider.method("set_points", &natPolygonColliderSetPoints, 1);
    polygonCollider.method("point_count", &natPolygonColliderPointCount, 0);
    polygonCollider.method("get_point", &natPolygonColliderGetPoint, 1);
    polygonCollider.method("set_regular", &natPolygonColliderSetRegular, 2);
    polygonCollider.persistent(true).constructable(false);
    zen::ObjClass* polygonColliderClass = polygonCollider.end();

    auto chainCollider = vm.def_class("ChainCollider");
    chainCollider.parent("Collider");
    chainCollider.method("set_points", &natChainColliderSetPoints, 1);
    chainCollider.method("point_count", &natChainColliderPointCount, 0);
    chainCollider.method("get_point", &natChainColliderGetPoint, 1);
    chainCollider.method("get_loop", &natChainColliderGetLoop, 0);
    chainCollider.method("set_loop", &natChainColliderSetLoop, 1);
    chainCollider.persistent(true).constructable(false);
    zen::ObjClass* chainColliderClass = chainCollider.end();

    (void)characterBodyClass;
    (void)colliderClass;

    auto panel = vm.def_class("Panel");
    panel.parent("Component");
    panel.method("set_color", &natUiPanelSetColor, -1);
    panel.persistent(true).constructable(false);
    zen::ObjClass* panelClass = panel.end();

    auto label = vm.def_class("Label");
    label.parent("Component");
    label.method("set_text", &natUiLabelSetText, 1);
    label.method("get_text", &natUiLabelGetText, 0);
    label.method("set_font_size", &natUiLabelSetFontSize, 1);
    label.method("get_font_size", &natUiLabelGetFontSize, 0);
    label.persistent(true).constructable(false);
    zen::ObjClass* labelClass = label.end();

    // Light is a ComponentType tag shared by two otherwise-unrelated C++
    // classes (Light2D, DirectionalLight2D) with no common base beneath
    // Component — unlike Collider, there is no shared field to bind here.
    // Scripts that know which kind they have already get full access via
    // get_component<Light2D>() / get_component<DirectionalLight2D>().
    zen::ObjClass* lightClass = defineHandle("Light");
    // UiCanvas has no fields at all beyond the Component it inherits — it is
    // purely a marker that roots a UI subtree.
    zen::ObjClass* canvasClass = defineHandle("UiCanvas");

    auto circleShape = vm.def_class("CircleShape");
    circleShape.parent("Component");
    circleShape.method("get_radius", &natCircleShapeGetRadius, 0);
    circleShape.method("set_radius", &natCircleShapeSetRadius, 1);
    circleShape.method("get_mode", &natCircleShapeGetMode, 0);
    circleShape.method("set_mode", &natCircleShapeSetMode, 1);
    circleShape.method("get_line_width", &natCircleShapeGetLineWidth, 0);
    circleShape.method("set_line_width", &natCircleShapeSetLineWidth, 1);
    circleShape.method("get_color", &natCircleShapeGetColor, 0);
    circleShape.method("set_color", &natCircleShapeSetColor, -1);
    circleShape.persistent(true).constructable(false);
    zen::ObjClass* circleShapeClass = circleShape.end();

    auto rectShape = vm.def_class("RectShape");
    rectShape.parent("Component");
    rectShape.method("get_size", &natRectShapeGetSize, 0);
    rectShape.method("set_size", &natRectShapeSetSize, 2);
    rectShape.method("get_mode", &natRectShapeGetMode, 0);
    rectShape.method("set_mode", &natRectShapeSetMode, 1);
    rectShape.method("get_line_width", &natRectShapeGetLineWidth, 0);
    rectShape.method("set_line_width", &natRectShapeSetLineWidth, 1);
    rectShape.method("get_color", &natRectShapeGetColor, 0);
    rectShape.method("set_color", &natRectShapeSetColor, -1);
    rectShape.persistent(true).constructable(false);
    zen::ObjClass* rectShapeClass = rectShape.end();

    auto capsuleShape = vm.def_class("CapsuleShape");
    capsuleShape.parent("Component");
    capsuleShape.method("get_size", &natCapsuleShapeGetSize, 0);
    capsuleShape.method("set_size", &natCapsuleShapeSetSize, 2);
    capsuleShape.method("get_mode", &natCapsuleShapeGetMode, 0);
    capsuleShape.method("set_mode", &natCapsuleShapeSetMode, 1);
    capsuleShape.method("get_line_width", &natCapsuleShapeGetLineWidth, 0);
    capsuleShape.method("set_line_width", &natCapsuleShapeSetLineWidth, 1);
    capsuleShape.method("get_color", &natCapsuleShapeGetColor, 0);
    capsuleShape.method("set_color", &natCapsuleShapeSetColor, -1);
    capsuleShape.persistent(true).constructable(false);
    zen::ObjClass* capsuleShapeClass = capsuleShape.end();

    auto navigationRegion = vm.def_class("NavigationRegion");
    navigationRegion.parent("Component");
    navigationRegion.method("set_points", &natNavRegionSetPoints, 1);
    navigationRegion.method("point_count", &natNavRegionPointCount, 0);
    navigationRegion.method("get_point", &natNavRegionGetPoint, 1);
    navigationRegion.method("is_valid", &natNavRegionIsValid, 0);
    navigationRegion.persistent(true).constructable(false);
    zen::ObjClass* navigationRegionClass = navigationRegion.end();

    auto spriteBatch = vm.def_class("SpriteBatch");
    spriteBatch.parent("Component");
    spriteBatch.method("add", &natSpriteBatchAdd, -1);
    spriteBatch.method("remove", &natSpriteBatchRemove, 1);
    spriteBatch.method("clear", &natSpriteBatchClear, 0);
    spriteBatch.method("count", &natSpriteBatchCount, 0);
    spriteBatch.method("set_source", &natSpriteBatchSetSource, 5);
    spriteBatch.method("set_flip", &natSpriteBatchSetFlip, 3);
    spriteBatch.method("set_color", &natSpriteBatchSetColor, -1);
    spriteBatch.persistent(true).constructable(false);
    zen::ObjClass* spriteBatchClass = spriteBatch.end();

    auto polygon = vm.def_class("Polygon2D");
    polygon.parent("Component");
    polygon.method("set_points", &natPolygonSetPoints, 1);
    polygon.method("point_count", &natPolygonPointCount, 0);
    polygon.method("get_point", &natPolygonGetPoint, 1);
    polygon.method("is_valid", &natPolygonIsValid, 0);
    polygon.method("get_color", &natPolygonGetColor, 0);
    polygon.method("set_color", &natPolygonSetColor, -1);
    polygon.persistent(true).constructable(false);
    zen::ObjClass* polygonClass = polygon.end();

    auto line = vm.def_class("Line2D");
    line.parent("Component");
    line.method("set_points", &natLineSetPoints, 1);
    line.method("point_count", &natLinePointCount, 0);
    line.method("get_point", &natLineGetPoint, 1);
    line.method("get_width", &natLineGetWidth, 0);
    line.method("set_width", &natLineSetWidth, 1);
    line.method("get_color", &natLineGetColor, 0);
    line.method("set_color", &natLineSetColor, -1);
    line.persistent(true).constructable(false);
    zen::ObjClass* lineClass = line.end();

    auto ninePatch = vm.def_class("NinePatch");
    ninePatch.parent("Component");
    ninePatch.method("get_size", &natNinePatchGetSize, 0);
    ninePatch.method("set_size", &natNinePatchSetSize, 2);
    ninePatch.method("get_color", &natNinePatchGetColor, 0);
    ninePatch.method("set_color", &natNinePatchSetColor, -1);
    ninePatch.persistent(true).constructable(false);
    zen::ObjClass* ninePatchClass = ninePatch.end();

    auto directionalLight = vm.def_class("DirectionalLight2D");
    directionalLight.parent("Component");
    directionalLight.method("get_color", &natDirectionalLightGetColor, 0);
    directionalLight.method("set_color", &natDirectionalLightSetColor, -1);
    directionalLight.method("get_energy", &natDirectionalLightGetEnergy, 0);
    directionalLight.method("set_energy", &natDirectionalLightSetEnergy, 1);
    directionalLight.persistent(true).constructable(false);
    zen::ObjClass* directionalLightClass = directionalLight.end();

    auto occluder = vm.def_class("LightOccluder");
    occluder.parent("Component");
    occluder.method("set_points", &natOccluderSetPoints, 1);
    occluder.method("point_count", &natOccluderPointCount, 0);
    occluder.method("get_point", &natOccluderGetPoint, 1);
    occluder.persistent(true).constructable(false);
    zen::ObjClass* occluderClass = occluder.end();

    auto motionTween = vm.def_class("MotionTween");
    motionTween.parent("Component");
    motionTween.method("play", &natMotionTweenPlay, -1);
    motionTween.method("stop", &natMotionTweenStop, 0);
    motionTween.method("pause", &natMotionTweenPause, -1);
    motionTween.method("is_playing", &natMotionTweenIsPlaying, 0);
    motionTween.method("is_paused", &natMotionTweenIsPaused, 0);
    motionTween.method("get_time", &natMotionTweenGetTime, 0);
    motionTween.method("get_loop", &natMotionTweenGetLoop, 0);
    motionTween.method("set_loop", &natMotionTweenSetLoop, 1);
    motionTween.method("get_one_shot", &natMotionTweenGetOneShot, 0);
    motionTween.method("set_one_shot", &natMotionTweenSetOneShot, 1);
    motionTween.method("clear_tracks", &natMotionTweenClearTracks, 0);
    motionTween.method("track_count", &natMotionTweenTrackCount, 0);
    motionTween.method("add_track", &natMotionTweenAddTrack, -1);
    motionTween.persistent(true).constructable(false);
    zen::ObjClass* motionTweenClass = motionTween.end();

    auto motionStreak = vm.def_class("MotionStreak");
    motionStreak.parent("Component");
    motionStreak.method("reset", &natMotionStreakReset, 0);
    motionStreak.method("get_lifetime", &natMotionStreakGetLifetime, 0);
    motionStreak.method("set_lifetime", &natMotionStreakSetLifetime, 1);
    motionStreak.method("get_width", &natMotionStreakGetWidth, 0);
    motionStreak.method("set_width", &natMotionStreakSetWidth, 1);
    motionStreak.method("get_min_distance", &natMotionStreakGetMinDistance, 0);
    motionStreak.method("set_min_distance", &natMotionStreakSetMinDistance, 1);
    motionStreak.method("get_color", &natMotionStreakGetColor, 0);
    motionStreak.method("set_color", &natMotionStreakSetColor, -1);
    motionStreak.persistent(true).constructable(false);
    zen::ObjClass* motionStreakClass = motionStreak.end();

    auto tileMap = vm.def_class("TileMap");
    tileMap.parent("Component");
    tileMap.method("get_tile", &natTileMapGetTile, 2);
    tileMap.method("set_tile", &natTileMapSetTile, 3);
    tileMap.method("has_collision", &natTileMapHasCollision, 2);
    tileMap.method("set_collision", &natTileMapSetCollision, 3);
    tileMap.method("get_columns", &natTileMapGetColumns, 0);
    tileMap.method("get_rows", &natTileMapGetRows, 0);
    tileMap.method("get_cell_size", &natTileMapGetCellSize, 0);
    tileMap.method("world_to_cell", &natTileMapWorldToCell, 2);
    tileMap.method("cell_to_world", &natTileMapCellToWorld, 2);
    tileMap.persistent(true).constructable(false);
    zen::ObjClass* tileMapClass = tileMap.end();

    auto light2D = vm.def_class("Light2D");
    light2D.parent("Component");
    light2D.method("get_color", &natLight2DGetColor, 0);
    light2D.method("set_color", &natLight2DSetColor, -1);
    light2D.method("get_energy", &natLight2DGetEnergy, 0);
    light2D.method("set_energy", &natLight2DSetEnergy, 1);
    light2D.method("get_radius", &natLight2DGetRadius, 0);
    light2D.method("set_radius", &natLight2DSetRadius, 1);
    light2D.persistent(true).constructable(false);
    zen::ObjClass* light2DClass = light2D.end();

    auto audioPlayer = vm.def_class("AudioPlayer");
    audioPlayer.parent("Component");
    audioPlayer.method("play", &natAudioPlayerPlay, 0);
    audioPlayer.method("stop", &natAudioPlayerStop, 0);
    audioPlayer.method("pause", &natAudioPlayerPause, 0);
    audioPlayer.method("resume", &natAudioPlayerResume, 0);
    audioPlayer.method("is_playing", &natAudioPlayerIsPlaying, 0);
    audioPlayer.method("get_volume", &natAudioPlayerGetVolume, 0);
    audioPlayer.method("set_volume", &natAudioPlayerSetVolume, 1);
    audioPlayer.method("get_loop", &natAudioPlayerGetLoop, 0);
    audioPlayer.method("set_loop", &natAudioPlayerSetLoop, 1);
    audioPlayer.persistent(true).constructable(false);
    zen::ObjClass* audioPlayerClass = audioPlayer.end();

    auto navigationAgent = vm.def_class("NavigationAgent");
    navigationAgent.parent("Component");
    navigationAgent.method("set_target", &natNavAgentSetTarget, 2);
    navigationAgent.method("get_target", &natNavAgentGetTarget, 0);
    navigationAgent.method("get_follow_position", &natNavAgentFollowPosition, 0);
    navigationAgent.method("set_follow_target", &natNavAgentSetFollowTarget, 1);
    navigationAgent.method("clear_follow_target", &natNavAgentClearFollowTarget, 0);
    navigationAgent.method("repath", &natNavAgentRepath, 0);
    navigationAgent.method("clear_path", &natNavAgentClearPath, 0);
    navigationAgent.method("has_path", &natNavAgentHasPath, 0);
    navigationAgent.method("is_finished", &natNavAgentIsFinished, 0);
    navigationAgent.method("next_position", &natNavAgentNextPosition, 0);
    navigationAgent.method("advance", &natNavAgentAdvance, 0);
    navigationAgent.method("path_count", &natNavAgentPathCount, 0);
    navigationAgent.method("path_point", &natNavAgentPathPoint, 1);
    navigationAgent.method("get_max_speed", &natNavAgentGetMaxSpeed, 0);
    navigationAgent.method("set_max_speed", &natNavAgentSetMaxSpeed, 1);
    navigationAgent.method("get_auto_move", &natNavAgentGetAutoMove, 0);
    navigationAgent.method("set_auto_move", &natNavAgentSetAutoMove, 1);
    navigationAgent.method("get_orient_to_path", &natNavAgentGetOrientToPath, 0);
    navigationAgent.method("set_orient_to_path", &natNavAgentSetOrientToPath, 1);
    navigationAgent.method("get_rotation_lerp_speed", &natNavAgentGetRotationLerpSpeed, 0);
    navigationAgent.method("set_rotation_lerp_speed", &natNavAgentSetRotationLerpSpeed, 1);
    navigationAgent.method("get_rotation_offset", &natNavAgentGetRotationOffset, 0);
    navigationAgent.method("set_rotation_offset", &natNavAgentSetRotationOffset, 1);
    navigationAgent.persistent(true).constructable(false);
    zen::ObjClass* navigationAgentClass = navigationAgent.end();

    auto steering = vm.def_class("Steering");
    steering.parent("Component");
    steering.method("get_weight", &natSteeringGetWeight, 0);
    steering.method("set_weight", &natSteeringSetWeight, 1);
    steering.method("get_target", &natSteeringGetTarget, 0);
    steering.method("set_target", &natSteeringSetTarget, 2);
    steering.method("set_target_object", &natSteeringSetTargetObject, 1);
    steering.method("clear_target_object", &natSteeringClearTargetObject, 0);
    steering.persistent(true).constructable(false);
    zen::ObjClass* steeringClass = steering.end();

    auto seek = vm.def_class("Seek");
    seek.parent("Steering");
    seek.persistent(true).constructable(false);
    zen::ObjClass* seekClass = seek.end();

    auto flee = vm.def_class("Flee");
    flee.parent("Steering");
    flee.method("get_radius", &natFleeGetRadius, 0);
    flee.method("set_radius", &natFleeSetRadius, 1);
    flee.persistent(true).constructable(false);
    zen::ObjClass* fleeClass = flee.end();

    auto arrive = vm.def_class("Arrive");
    arrive.parent("Steering");
    arrive.method("get_slow_radius", &natArriveGetSlowRadius, 0);
    arrive.method("set_slow_radius", &natArriveSetSlowRadius, 1);
    arrive.method("get_stop_radius", &natArriveGetStopRadius, 0);
    arrive.method("set_stop_radius", &natArriveSetStopRadius, 1);
    arrive.persistent(true).constructable(false);
    zen::ObjClass* arriveClass = arrive.end();

    auto wander = vm.def_class("Wander");
    wander.parent("Steering");
    wander.method("get_jitter", &natWanderGetJitter, 0);
    wander.method("set_jitter", &natWanderSetJitter, 1);
    wander.method("get_radius", &natWanderGetRadius, 0);
    wander.method("set_radius", &natWanderSetRadius, 1);
    wander.method("get_distance", &natWanderGetDistance, 0);
    wander.method("set_distance", &natWanderSetDistance, 1);
    wander.persistent(true).constructable(false);
    zen::ObjClass* wanderClass = wander.end();

    auto separation = vm.def_class("Separation");
    separation.parent("Steering");
    separation.method("get_radius", &natSeparationGetRadius, 0);
    separation.method("set_radius", &natSeparationSetRadius, 1);
    separation.method("get_mask", &natSeparationGetMask, 0);
    separation.method("set_mask", &natSeparationSetMask, 1);
    separation.persistent(true).constructable(false);
    zen::ObjClass* separationClass = separation.end();

    auto obstacleAvoidance = vm.def_class("ObstacleAvoidance");
    obstacleAvoidance.parent("Steering");
    obstacleAvoidance.method("get_look_ahead", &natAvoidanceGetLookAhead, 0);
    obstacleAvoidance.method("set_look_ahead", &natAvoidanceSetLookAhead, 1);
    obstacleAvoidance.method("get_mask", &natAvoidanceGetMask, 0);
    obstacleAvoidance.method("set_mask", &natAvoidanceSetMask, 1);
    obstacleAvoidance.persistent(true).constructable(false);
    zen::ObjClass* obstacleAvoidanceClass = obstacleAvoidance.end();

    auto skeleton = vm.def_class("Skeleton");
    skeleton.parent("Component");
    skeleton.method("play", &natSkeletonPlay, -1);
    skeleton.method("stop", &natSkeletonStop, 0);
    skeleton.method("pause", &natSkeletonPause, 0);
    skeleton.method("resume", &natSkeletonResume, 0);
    skeleton.method("seek", &natSkeletonSeek, 1);
    skeleton.method("is_playing", &natSkeletonIsPlaying, 0);
    skeleton.method("current", &natSkeletonCurrent, 0);
    skeleton.method("get_time", &natSkeletonGetTime, 0);
    skeleton.method("get_speed", &natSkeletonGetSpeed, 0);
    skeleton.method("set_speed", &natSkeletonSetSpeed, 1);
    skeleton.method("clip_count", &natSkeletonClipCount, 0);
    skeleton.method("find_bone", &natSkeletonFindBone, 1);
    skeleton.method("reset_to_rest", &natSkeletonResetToRest, 0);
    skeleton.method("solve_ik", &natSkeletonSolveIK, -1);
    skeleton.persistent(true).constructable(false);
    zen::ObjClass* skeletonClass = skeleton.end();

    auto bone = vm.def_class("Bone");
    bone.parent("Component");
    bone.method("get_length", &natBoneGetLength, 0);
    bone.method("set_length", &natBoneSetLength, 1);
    bone.method("get_rest_position", &natBoneGetRestPosition, 0);
    bone.method("get_rest_rotation", &natBoneGetRestRotation, 0);
    bone.method("save_rest_pose", &natBoneSaveRestPose, 0);
    bone.method("reset_to_rest", &natBoneResetToRest, 0);
    bone.persistent(true).constructable(false);
    boneClass = bone.end();
    (void)canvasClass;

    const struct Alias { const char* name; zen::ObjClass* klass; } aliases[] = {
        {"RigidBody2D", rigidBodyClass}, {"SpriteComponent", spriteClass},
        {"Animation2D", animationClass}, {"CameraComponent", cameraClass},
        {"ParticleComponent", particleClass}, {"CharacterBody2D", characterBodyClass},
        {"Collider2D", colliderClass}, {"BoxCollider2D", boxColliderClass},
        {"CircleCollider2D", circleColliderClass}, {"EdgeCollider2D", edgeColliderClass},
        {"PolygonCollider2D", polygonColliderClass}, {"ChainCollider2D", chainColliderClass},
        {"TileMapComponent", tileMapClass}, {"NinePatchComponent", ninePatchClass},
        {"LightOccluder2D", occluderClass},
        {"UiPanel", panelClass}, {"UiLabel", labelClass},
        {"UiButton", buttonClass}, {"UiCheckBox", checkBoxClass}, {"UiSlider", sliderClass},
        {"NavigationRegion2D", navigationRegionClass}, {"NavigationAgent2D", navigationAgentClass},
        {"MotionTween2D", motionTweenClass}, {"MotionStreak2D", motionStreakClass},
        {"Skeleton2D", skeletonClass}, {"Bone2D", boneClass},
        {"Steering2D", steeringClass}, {"Seek2D", seekClass}, {"Flee2D", fleeClass},
        {"Arrive2D", arriveClass}, {"Wander2D", wanderClass}, {"Separation2D", separationClass},
        {"ObstacleAvoidance2D", obstacleAvoidanceClass},
    };
    (void)light2DClass;
    for (const Alias& alias : aliases)
        vm.def_global(alias.name, zen::val_obj((zen::Obj*)alias.klass));

    vm.def_native("get_number", &natGetNumber, -1);
    vm.def_native("set_number", &natSetNumber, 2);
    vm.def_native("get_string", &natGetString, -1);
    vm.def_native("set_string", &natSetString, 2);
    vm.def_native("get_flag", &natGetFlag, -1);
    vm.def_native("set_flag", &natSetFlag, 2);
    vm.def_native("has_key", &natHasKey, 1);
    vm.def_native("user_data_get_int", &natUserDataGetInt, -1);
    vm.def_native("user_data_set_int", &natUserDataSetInt, 2);
    vm.def_native("user_data_get_float", &natUserDataGetFloat, -1);
    vm.def_native("user_data_set_float", &natUserDataSetFloat, 2);
    vm.def_native("user_data_get_string", &natUserDataGetString, -1);
    vm.def_native("user_data_set_string", &natUserDataSetString, 2);
    vm.def_native("user_data_get_bool", &natUserDataGetBool, -1);
    vm.def_native("user_data_set_bool", &natUserDataSetBool, 2);
    vm.def_native("user_data_has", &natUserDataHas, 1);
    vm.def_native("user_data_delete", &natUserDataDelete, 1);
    vm.def_native("user_data_clear", &natUserDataClear, 0);
    vm.def_native("user_data_load", &natUserDataLoad, -1);
    vm.def_native("user_data_save", &natUserDataSave, -1);
    vm.def_native("user_data_read_text", &natUserDataReadText, -1);
    vm.def_native("user_data_write_text", &natUserDataWriteText, 2);
    vm.def_native("emit", &natEmit, -1);

    vm.def_native("key_down", &natKeyDown, 1);
    vm.def_native("key_pressed", &natKeyPressed, 1);
    vm.def_native("key_released", &natKeyReleased, 1);
    vm.def_native("virtual_key_add", &natVirtualKeyAdd, 5);
    vm.def_native("virtual_keys_clear", &natVirtualKeysClear, 0);
    vm.def_native("virtual_keys_set_visible", &natVirtualKeysSetVisible, 1);
    vm.def_native("virtual_keys_visible", &natVirtualKeysVisible, 0);
    // Names retained from the previous ZenEngine input module.
    vm.def_native("input_add_virtual_key", &natVirtualKeyAdd, 5);
    vm.def_native("input_clear_virtual_keys", &natVirtualKeysClear, 0);
    vm.def_native("input_set_virtual_keys_visible", &natVirtualKeysSetVisible, 1);
    vm.def_native("input_get_virtual_keys_visible", &natVirtualKeysVisible, 0);
    vm.def_native("action_down", &natActionDown, 1);
    vm.def_native("action_pressed", &natActionPressed, 1);
    vm.def_native("action_released", &natActionReleased, 1);
    vm.def_native("fade_in", &natFadeIn, 1);
    vm.def_native("fade_out", &natFadeOut, 1);
    vm.def_native("is_fading", &natIsFading, 0);
    vm.def_native("fade_progress", &natFadeProgress, 0);
    vm.def_native("load_scene", &natLoadScene, 1);
    vm.def_native("audio_load", &natAudioLoad, 1);
    vm.def_native("audio_load_music", &natAudioLoadMusic, 1);
    vm.def_native("audio_play", &natAudioPlay, -1);
    vm.def_native("audio_play_at", &natAudioPlayAt, -1);
    vm.def_native("audio_play_music", &natAudioPlayMusic, -1);
    vm.def_native("audio_crossfade_music", &natAudioCrossfadeMusic, -1);
    vm.def_native("audio_stop", &natAudioStop, 1);
    vm.def_native("audio_pause", &natAudioPause, 1);
    vm.def_native("audio_resume", &natAudioResume, 1);
    vm.def_native("audio_playing", &natAudioPlaying, 1);
    vm.def_native("audio_fade_in", &natAudioFadeIn, 2);
    vm.def_native("audio_fade_out", &natAudioFadeOut, -1);
    vm.def_native("audio_stop_all", &natAudioStopAll, 0);
    vm.def_native("audio_stop_music", &natAudioStopMusic, 0);
    vm.def_native("audio_set_master_volume", &natAudioSetMasterVolume, 1);
    vm.def_native("audio_set_sfx_volume", &natAudioSetSfxVolume, 1);
    vm.def_native("audio_set_music_volume", &natAudioSetMusicVolume, 1);
    vm.def_native("audio_set_master_muted", &natAudioSetMasterMuted, 1);
    vm.def_native("audio_set_sfx_muted", &natAudioSetSfxMuted, 1);
    vm.def_native("audio_set_music_muted", &natAudioSetMusicMuted, 1);
    vm.def_native("audio_master_muted", &natAudioMasterMuted, 0);
    vm.def_native("audio_sfx_muted", &natAudioSfxMuted, 0);
    vm.def_native("audio_music_muted", &natAudioMusicMuted, 0);
    vm.def_native("audio_set_listener_position", &natAudioSetListenerPosition, 2);
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
    vm.def_native("get_active_camera", &natGetActiveCamera, 0);
    vm.def_native("set_gravity", &natSetGravity, 2);
    vm.def_native("get_gravity", &natGetGravity, 0);
    vm.def_native("nav_path", &natNavPath, 4);
    vm.def_native("nav_point_free", &natNavPointFree, 2);

    struct AStarEnumConstant
    {
        const char* name;
        int value;
    };
    static const AStarEnumConstant astarEnumConstants[] = {
        {"ASTAR_HEURISTIC_EUCLIDEAN", (int)AStarGrid2D::Heuristic::Euclidean},
        {"ASTAR_HEURISTIC_MANHATTAN", (int)AStarGrid2D::Heuristic::Manhattan},
        {"ASTAR_HEURISTIC_OCTILE", (int)AStarGrid2D::Heuristic::Octile},
        {"ASTAR_HEURISTIC_CHEBYSHEV", (int)AStarGrid2D::Heuristic::Chebyshev},
        {"ASTAR_DIAGONAL_ALWAYS", (int)AStarGrid2D::DiagonalMode::Always},
        {"ASTAR_DIAGONAL_NEVER", (int)AStarGrid2D::DiagonalMode::Never},
        {"ASTAR_DIAGONAL_AT_LEAST_ONE_WALKABLE", (int)AStarGrid2D::DiagonalMode::AtLeastOneWalkable},
        {"ASTAR_DIAGONAL_ONLY_IF_NO_OBSTACLES", (int)AStarGrid2D::DiagonalMode::OnlyIfNoObstacles},
    };
    for (const AStarEnumConstant& entry : astarEnumConstants)
        vm.def_global(entry.name, zen::val_int(entry.value));

    auto astarGrid = vm.def_class("AStarGrid");
    astarGrid.ctor(&natAStarGridCtor);
    astarGrid.dtor(&natAStarGridDtor);
    astarGrid.method("set_size", &natAStarGridSetSize, 2);
    astarGrid.method("set_cell_size", &natAStarGridSetCellSize, 2);
    astarGrid.method("set_offset", &natAStarGridSetOffset, 2);
    astarGrid.method("set_heuristic", &natAStarGridSetHeuristic, 1);
    astarGrid.method("set_diagonal_mode", &natAStarGridSetDiagonalMode, 1);
    astarGrid.method("clear", &natAStarGridClear, 0);
    astarGrid.method("set_solid", &natAStarGridSetSolid, -1);
    astarGrid.method("is_solid", &natAStarGridIsSolid, 2);
    astarGrid.method("fill_solid_region", &natAStarGridFillSolidRegion, -1);
    astarGrid.method("set_weight_scale", &natAStarGridSetWeightScale, 3);
    astarGrid.method("get_point_position", &natAStarGridGetPointPosition, 2);
    astarGrid.method("get_point_path", &natAStarGridGetPointPath, -1);
    astarGrid.end();

    auto astarGraph = vm.def_class("AStarGraph");
    astarGraph.ctor(&natAStarGraphCtor);
    astarGraph.dtor(&natAStarGraphDtor);
    astarGraph.method("add_point", &natAStarGraphAddPoint, -1);
    astarGraph.method("remove_point", &natAStarGraphRemovePoint, 1);
    astarGraph.method("has_point", &natAStarGraphHasPoint, 1);
    astarGraph.method("connect_points", &natAStarGraphConnectPoints, -1);
    astarGraph.method("disconnect_points", &natAStarGraphDisconnectPoints, -1);
    astarGraph.method("set_point_disabled", &natAStarGraphSetPointDisabled, -1);
    astarGraph.method("get_closest_point", &natAStarGraphGetClosestPoint, -1);
    astarGraph.method("get_point_path", &natAStarGraphGetPointPath, -1);
    astarGraph.method("clear", &natAStarGraphClear, 0);
    astarGraph.method("get_point_count", &natAStarGraphGetPointCount, 0);
    astarGraph.end();
}

ZenScriptComponent::ZenScriptComponent()
    : ScriptComponent(ComponentEventUpdate | ComponentEventRender), mState(new State())
{
    ZenRuntime::instance().impl().liveInstances.push_back(&mState->instance);
}

ZenScriptComponent::~ZenScriptComponent()
{
    destroyInstance();
    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();
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

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();
    if (mState->started && mState->scriptClass->slotDestroy >= 0)
    {
        RunningScript running;
        impl.vm.invoke(mState->instance, mState->scriptClass->slotDestroy, nullptr, 0);
    }
    mState->instance = zen::val_nil();
    mState->started = false;
}

bool ZenScriptComponent::loadSource(const char* source, const char* scriptName)
{
    if (!source)
        return false;

    return loadFromSource(source, scriptName ? scriptName : "script");
}

bool ZenScriptComponent::loadFile(const char* path)
{
    if (!path || !path[0])
        return false;

    ZenRuntime& runtime = ZenRuntime::instance();
    if (ZenScriptClass* cached = runtime.impl().findClass(path))
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
    // Zen's lexer consumes a C string.  Keep the byte count for bytecode
    // detection, but always reserve the trailing NUL for source scripts.
    if (!FileSystem::Instance().LoadFile(path, buffer, true))
        return false;

    mScriptPath = path;
    mSourceTimestamp = fileTimestamp(path);
    if (zen::is_bytecode_buffer(buffer.Data(), buffer.Size()))
        return loadFromBytecode(buffer.Data(), buffer.Size(), path);
    return loadFromSource(buffer.Text(), path);
}

bool ZenScriptComponent::loadFromSource(const char* source, const char* path)
{
    ZenRuntime& runtime = ZenRuntime::instance();
    ZenRuntime::Impl& impl = runtime.impl();

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

bool ZenScriptComponent::loadFromBytecode(const unsigned char* data, std::size_t size, const char* path)
{
    ZenRuntime& runtime = ZenRuntime::instance();
    ZenRuntime::Impl& impl = runtime.impl();

    destroyInstance();
    mState->scriptClass = nullptr;
    mState->loaded = false;
    mState->pending = false;

    ZenScriptClass compiled;
    if (!impl.loadBytecodeClass(data, size, path, compiled))
        return false;

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
    ZenRuntime& runtime = ZenRuntime::instance();

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

    ZenRuntime::Impl& impl = runtime.impl();
    if (zen::is_nil(mState->self))
        mState->self = impl.instanceFor(impl.nodeClass, owner());

    if (zen::is_nil(mState->instance))
    {
        zen::ObjClass* scriptClass = zen::as_class(mState->scriptClass->klass);
        const bool derivesFromScriptComponent = scriptClass && scriptClass->parent == impl.scriptComponentClass;
        mState->instance = impl.vm.make_instance(scriptClass);
        if (!zen::is_instance(mState->instance))
        {
            mState->instance = zen::val_nil();
            return false;
        }
        if (derivesFromScriptComponent)
        {
            zen::ObjInstance* instance = zen::as_instance(mState->instance);
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

bool ZenScriptComponent::callEvent(const char* event, double value)
{
    ZenCallbackScope callbackScope(owner());
    if ((!mState->loaded && !mState->pending) || !ensureInstance() || !event)
        return false;

    ZenScriptClass* scriptClass = mState->scriptClass;
    if (scriptClass->slotEvent < 0)
        return false;

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();

    zen::Value args[2] = {zen::val_obj((zen::Obj*)impl.vm.make_string(event)), zen::val_float(value)};
    RunningScript running;
    impl.vm.invoke(mState->instance, scriptClass->slotEvent, args, 2);
    return !impl.vm.had_error();
}

bool ZenScriptComponent::callCollision(GameObject* other, bool began)
{
    ZenCallbackScope callbackScope(owner());
    if ((!mState->loaded && !mState->pending) || !ensureInstance())
        return false;

    ZenScriptClass* scriptClass = mState->scriptClass;
    if (!scriptClass || scriptClass->slotCollision < 0)
        return false;

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();

    zen::Value args[2] = {other ? impl.instanceFor(impl.nodeClass, other) : zen::val_nil(), zen::val_bool(began)};
    RunningScript running;
    impl.vm.invoke(mState->instance, scriptClass->slotCollision, args, 2);
    return !impl.vm.had_error();
}

namespace
{
void routeCollision(const CollisionInfo& info, void*)
{
    if (!gZenScriptsEnabled || !info.self)
        return;
    const size_t count = info.self->componentCount<ZenScriptComponent>();
    for (size_t i = 0; i < count; ++i)
        if (ZenScriptComponent* script = info.self->getComponentAt<ZenScriptComponent>(i))
            script->callCollision(info.other, info.began);
}
} // namespace

void RouteZenScriptCollisions(Scene& scene)
{
    scene.setCollisionCallback(&routeCollision, nullptr);
}

bool ZenScriptComponent::callAnimationEvent(const char* name)
{
    ZenCallbackScope callbackScope(owner());
    if ((!mState->loaded && !mState->pending) || !ensureInstance() || !name)
        return false;

    ZenScriptClass* scriptClass = mState->scriptClass;
    if (!scriptClass || scriptClass->slotAnimationEvent < 0)
        return false;

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();

    zen::Value args[1] = {zen::val_obj((zen::Obj*)impl.vm.make_string(name))};
    RunningScript running;
    impl.vm.invoke(mState->instance, scriptClass->slotAnimationEvent, args, 1);
    return !impl.vm.had_error();
}

bool ZenScriptComponent::callAnimationFinished(const char* clip)
{
    ZenCallbackScope callbackScope(owner());
    if ((!mState->loaded && !mState->pending) || !ensureInstance() || !clip)
        return false;

    ZenScriptClass* scriptClass = mState->scriptClass;
    if (!scriptClass || scriptClass->slotAnimationFinished < 0)
        return false;

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();

    zen::Value args[1] = {zen::val_obj((zen::Obj*)impl.vm.make_string(clip))};
    RunningScript running;
    impl.vm.invoke(mState->instance, scriptClass->slotAnimationFinished, args, 1);
    return !impl.vm.had_error();
}

namespace
{
void routeAnimationEvent(GameObject* object, const char* clip, const char* event, bool finished, void*)
{
    if (!gZenScriptsEnabled || !object)
        return;
    const size_t count = object->componentCount<ZenScriptComponent>();
    for (size_t i = 0; i < count; ++i)
    {
        ZenScriptComponent* script = object->getComponentAt<ZenScriptComponent>(i);
        if (!script)
            continue;
        if (finished)
            script->callAnimationFinished(clip);
        else
            script->callAnimationEvent(event);
    }
}
} // namespace

void RouteZenScriptAnimationEvents(Scene& scene)
{
    scene.setAnimationEventCallback(&routeAnimationEvent, nullptr);
}

bool ZenScriptComponent::callFunction(const char* name, double value)
{
    ZenCallbackScope callbackScope(owner());
    if ((!mState->loaded && !mState->pending) || !ensureInstance() || !name)
        return false;

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();
    zen::Value args[1] = {zen::val_float(value)};
    RunningScript running;
    impl.vm.invoke(mState->instance, name, args, 1);
    return !impl.vm.had_error();
}

bool ZenScriptComponent::hasFunction(const char* name) const
{
    if (!mState->loaded || !mState->scriptClass || !name)
        return false;

    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();
    const int slot = impl.vm.find_selector(name, (int)std::strlen(name));
    if (slot < 0)
        return false;

    zen::ObjClass* klass =
        zen::is_class(mState->scriptClass->klass) ? zen::as_class(mState->scriptClass->klass) : nullptr;
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

const ZenScriptProperty* ZenScriptComponent::declaredPropertyAt(std::size_t index) const
{
    if (!mState->scriptClass || index >= mState->scriptClass->properties.size())
        return nullptr;
    return &mState->scriptClass->properties[index];
}

const ZenScriptProperty* ZenScriptComponent::declaredProperty(const char* name) const
{
    if (!mState->scriptClass || !name)
        return nullptr;
    const ct::Vector<ZenScriptProperty>& declared = mState->scriptClass->properties;
    for (size_t i = 0; i < declared.size(); ++i)
        if (declared[i].name == name)
            return &declared[i];
    return nullptr;
}

std::size_t ZenScriptComponent::overrideCount() const
{
    return mOverrides.size();
}

const ZenScriptProperty* ZenScriptComponent::overrideAt(std::size_t index) const
{
    return index < mOverrides.size() ? &mOverrides[index] : nullptr;
}

const ZenScriptProperty* ZenScriptComponent::findOverride(const char* name) const
{
    if (!name)
        return nullptr;
    for (size_t i = 0; i < mOverrides.size(); ++i)
        if (mOverrides[i].name == name)
            return &mOverrides[i];
    return nullptr;
}

ZenScriptProperty& ZenScriptComponent::overrideSlot(const char* name)
{
    for (size_t i = 0; i < mOverrides.size(); ++i)
        if (mOverrides[i].name == name)
            return mOverrides[i];

    ZenScriptProperty added;
    added.name = name;
    mOverrides.push_back(added);
    return mOverrides[mOverrides.size() - 1];
}

void ZenScriptComponent::setNumberOverride(const char* name, double value, bool integer)
{
    if (!name || !name[0])
        return;
    ZenScriptProperty& prop = overrideSlot(name);
    prop.kind = ZenScriptProperty::Kind::Number;
    prop.number = value;
    prop.integer = integer;
    applyOverrides();
}

void ZenScriptComponent::setStringOverride(const char* name, const char* value)
{
    if (!name || !name[0])
        return;
    ZenScriptProperty& prop = overrideSlot(name);
    prop.kind = ZenScriptProperty::Kind::String;
    prop.text = value ? value : "";
    applyOverrides();
}

void ZenScriptComponent::setBoolOverride(const char* name, bool value)
{
    if (!name || !name[0])
        return;
    ZenScriptProperty& prop = overrideSlot(name);
    prop.kind = ZenScriptProperty::Kind::Bool;
    prop.flag = value;
    applyOverrides();
}

void ZenScriptComponent::clearOverride(const char* name)
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

    if (const ZenScriptProperty* declared = declaredProperty(key.c_str()))
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
        const ZenScriptProperty* declared = declaredProperty(cleared[i].name.c_str());
        if (declared)
            writeProperty(*declared);
        else
            destroyInstance();
    }
}

bool ZenScriptComponent::writeProperty(const ZenScriptProperty& prop)
{
    if (zen::is_nil(mState->instance))
        return false;

    zen::ObjInstance* inst = zen::as_instance(mState->instance);
    if (!inst || !inst->klass || !inst->klass->field_names)
        return false;

    zen::ObjClass* klass = inst->klass;
    int field = -1;
    for (int f = 0; f < klass->num_fields; ++f)
    {
        if (klass->field_names[f] && std::strcmp(klass->field_names[f]->chars, prop.name.c_str()) == 0)
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
        inst->fields[field] = prop.integer ? zen::val_int((int64_t)prop.number) : zen::val_float(prop.number);
        break;
    case ZenScriptProperty::Kind::String:
        inst->fields[field] = zen::val_obj((zen::Obj*)ZenRuntime::instance().impl().vm.make_string(prop.text.c_str()));
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

    ZenScriptClass* scriptClass = mState->scriptClass;
    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();
    if (!mState->started)
    {
        mState->started = true;
        if (scriptClass->slotStart >= 0)
        {
            RunningScript running(ScriptProfilePhase::Update);
            impl.vm.invoke(mState->instance, scriptClass->slotStart, nullptr, 0);
        }
    }

    if (scriptClass->slotUpdate >= 0)
    {
        zen::Value dt = zen::val_float(deltaTime);
        RunningScript running(ScriptProfilePhase::Update);
        impl.vm.invoke(mState->instance, scriptClass->slotUpdate, &dt, 1);
    }
}

void ZenScriptComponent::onRender(RenderQueue& queue)
{
    ZenCallbackScope callbackScope(owner());
    if (!gZenScriptsEnabled || (!mState->loaded && !mState->pending) || !owner())
        return;
    if (!ensureInstance())
        return;

    ZenScriptClass* scriptClass = mState->scriptClass;
    ZenRuntime::Impl& impl = ZenRuntime::instance().impl();
    const auto invokeDraw = [&](int slot, int zIndex, bool screenSpace)
    {
        if (slot < 0)
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
        ZenDrawContext* previousContext = gZenDrawContext;
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

ct::Vector<BlackboardEntry>& blackboardEntries()
{
    static ct::Vector<BlackboardEntry> entries;
    return entries;
}

ct::Vector<PendingEvent>& pendingEvents()
{
    static ct::Vector<PendingEvent> events;
    return events;
}

ZenBlackboard::Handler gHostHandler = nullptr;
void* gHostHandlerUser = nullptr;

BlackboardEntry* findEntry(const char* key)
{
    if (!key)
        return nullptr;
    ct::Vector<BlackboardEntry>& entries = blackboardEntries();
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].key == key)
            return &entries[i];
    return nullptr;
}

BlackboardEntry& entryFor(const char* key)
{
    if (BlackboardEntry* existing = findEntry(key))
        return *existing;
    BlackboardEntry entry;
    entry.key = key ? key : "";
    blackboardEntries().push_back(entry);
    return blackboardEntries().back();
}
} // namespace

void ZenBlackboard::setNumber(const char* key, double value)
{
    BlackboardEntry& entry = entryFor(key);
    entry.number = value;
    entry.kind = 0;
}

void ZenBlackboard::setString(const char* key, const char* value)
{
    BlackboardEntry& entry = entryFor(key);
    entry.text = value ? value : "";
    entry.kind = 1;
}

void ZenBlackboard::setBool(const char* key, bool value)
{
    BlackboardEntry& entry = entryFor(key);
    entry.flag = value;
    entry.kind = 2;
}

double ZenBlackboard::getNumber(const char* key, double fallback)
{
    const BlackboardEntry* entry = findEntry(key);
    if (!entry)
        return fallback;
    if (entry->kind == 2)
        return entry->flag ? 1.0 : 0.0;
    return entry->kind == 0 ? entry->number : fallback;
}

ct::String ZenBlackboard::getString(const char* key, const char* fallback)
{
    const BlackboardEntry* entry = findEntry(key);
    if (!entry || entry->kind != 1)
        return ct::String(fallback ? fallback : "");
    return entry->text;
}

bool ZenBlackboard::getBool(const char* key, bool fallback)
{
    const BlackboardEntry* entry = findEntry(key);
    if (!entry)
        return fallback;
    if (entry->kind == 2)
        return entry->flag;
    return entry->kind == 0 ? entry->number != 0.0 : fallback;
}

bool ZenBlackboard::has(const char* key)
{
    return findEntry(key) != nullptr;
}

void ZenBlackboard::remove(const char* key)
{
    ct::Vector<BlackboardEntry>& entries = blackboardEntries();
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
    const ct::Vector<BlackboardEntry>& entries = blackboardEntries();
    return index < entries.size() ? entries[index].key : ct::String();
}

ZenBlackboard::Kind ZenBlackboard::kindOf(const char* key)
{
    const BlackboardEntry* entry = findEntry(key);
    if (!entry)
        return Kind::Number;
    if (entry->kind == 1)
        return Kind::String;
    if (entry->kind == 2)
        return Kind::Bool;
    return Kind::Number;
}

void ZenBlackboard::emit(const char* event, double value)
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

void ZenBlackboard::setHostHandler(Handler handler, void* user)
{
    gHostHandler = handler;
    gHostHandlerUser = user;
}

void BroadcastZenScriptEvent(GameObject& root, const char* event, double value)
{
    const size_t count = root.componentCount<ZenScriptComponent>();
    for (size_t i = 0; i < count; ++i)
        if (ZenScriptComponent* script = root.getComponentAt<ZenScriptComponent>(i))
            if (script->active())
                script->callEvent(event, value);

    for (size_t i = 0; i < root.childCount(); ++i)
        BroadcastZenScriptEvent(*root.child(i), event, value);
}

void DispatchZenScriptEvents(GameObject& root)
{
    ct::Vector<PendingEvent>& events = pendingEvents();
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

void SetZenScriptInput(Input* input)
{
    gZenInput = input;
}

void SetZenScriptVirtualPad(VirtualPad* pad)
{
    gZenVirtualPad = pad;
}

void SetZenScriptGameViewport(float x, float y, float width, float height)
{
    gZenGameViewport.x = x;
    gZenGameViewport.y = y;
    gZenGameViewport.width = width > 0.0f ? width : 0.0f;
    gZenGameViewport.height = height > 0.0f ? height : 0.0f;
    gZenGameViewport.valid = gZenGameViewport.width > 0.0f && gZenGameViewport.height > 0.0f;
}

void SetZenScriptGameCamera(const Camera2D* camera)
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

void SetZenScriptAssets(Assets* assets)
{
    gZenAssets = assets;
}

void SetZenScriptUserData(UserData* userData)
{
    gZenUserData = userData;
}

void SetZenScriptOutput(void (*fn)(const char* text, bool isError, void* user), void* user)
{
    gZenOutput = fn;
    gZenOutputUser = user;
}

namespace
{
Component* createZenScript(GameObject& owner)
{
    return owner.addComponent<ZenScriptComponent>();
}

void writeZenScript(const Component& component, ct::Json& data, Assets*)
{
    const ZenScriptComponent& script = static_cast<const ZenScriptComponent&>(component);
    data.set("path", ct::Json(script.scriptPath().c_str()));

    if (script.overrideCount() == 0)
        return;

    ct::Json properties = ct::Json::array();
    for (size_t i = 0; i < script.overrideCount(); ++i)
    {
        const ZenScriptProperty* prop = script.overrideAt(i);
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

void readZenScript(Component& component, const ct::Json& data, Assets*)
{
    ZenScriptComponent& script = static_cast<ZenScriptComponent&>(component);
    const char* path = data["path"].as_cstr("");
    if (path[0])
        script.loadFile(path);

    const ct::Json& properties = data["properties"];
    if (!properties.is_array())
        return;

    for (size_t i = 0; i < properties.size(); ++i)
    {
        const ct::Json& entry = properties[i];
        const char* name = entry["name"].as_cstr("");
        if (!name[0])
            continue;
        const ct::Json& value = entry["value"];
        if (value.is_bool())
            script.setBoolOverride(name, value.as_bool());
        else if (value.is_string())
            script.setStringOverride(name, value.as_cstr(""));
        else if (value.is_number())
            script.setNumberOverride(name, value.as_double(), !value.is_real());
    }
}

bool matchZenScript(const Component& component)
{
    return dynamic_cast<const ZenScriptComponent*>(&component) != nullptr;
}
} // namespace

void RegisterZenScriptSerializer()
{
    Serializer::RegisterType(ComponentType::Script, "ZenScript", &createZenScript, &writeZenScript, &readZenScript,
                             &matchZenScript);
}

} // namespace k2d
