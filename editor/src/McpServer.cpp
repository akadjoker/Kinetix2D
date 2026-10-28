#include "McpServer.h"

#include "EditorWindow.h"
#include "core/EditorFileSystem.h"

#include <k2d/ClassDB.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/Image.h>
#include <k2d/OpenGL.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>

#include <platform.h>

#include <cstdio>
#include <cstring>

namespace k2d::editor
{

namespace
{

const char* const kProtocolVersion = "2025-06-18";

// The tools as tools/list gives them.
const char* const kTools = R"JSON([
 {"name":"scene_info","description":"The open scene: its file, whether it has unsaved changes, whether the game is playing, how many objects it holds, the project root.","inputSchema":{"type":"object","properties":{}}},
 {"name":"scene_tree","description":"The objects of the scene as a tree: for each its path (child indices joined by dots, e.g. \"0.2\"), name, position, rotation, scale, visibility and component class names. Objects are referred to by that path or by name in the other tools.","inputSchema":{"type":"object","properties":{"depth":{"type":"integer","description":"How deep to go; all levels when left out."}}}},
 {"name":"get_object","description":"One object in full: transform, and every component with every property (name, type, value; enums also list their names; ranges their limits).","inputSchema":{"type":"object","properties":{"object":{"type":"string","description":"Path (\"0.2\") or name."}},"required":["object"]}},
 {"name":"set_property","description":"Change a property. component is the class name from get_object (e.g. SpriteComponent) or \"Transform\" for name/position/rotation/scale/visible/active. Values: numbers, booleans, strings, [x,y] for vectors, [r,g,b,a] in 0..1 for colours, a name or an index for enums, a project path for textures. One undo step.","inputSchema":{"type":"object","properties":{"object":{"type":"string"},"component":{"type":"string"},"index":{"type":"integer","description":"Which of several components of that class, from 0."},"property":{"type":"string"},"value":{}},"required":["object","component","property","value"]}},
 {"name":"create_object","description":"A new empty object, at x, y in the world, under parent (or at the top). Returns its path.","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"parent":{"type":"string"},"x":{"type":"number"},"y":{"type":"number"}},"required":["name"]}},
 {"name":"add_component","description":"Add a component by type name (see component_types). For a script give type \"ZenScript\" and script, a project path to a .py file.","inputSchema":{"type":"object","properties":{"object":{"type":"string"},"type":{"type":"string"},"script":{"type":"string"}},"required":["object","type"]}},
 {"name":"delete_object","description":"Delete an object and its children. One undo step.","inputSchema":{"type":"object","properties":{"object":{"type":"string"}},"required":["object"]}},
 {"name":"component_types","description":"The component type names add_component takes.","inputSchema":{"type":"object","properties":{}}},
 {"name":"run","description":"Control the game: play (from the scene as it is), stop (back to the scene as it was before play), pause, resume, or step a number of frames of 1/60 s while paused.","inputSchema":{"type":"object","properties":{"action":{"type":"string","enum":["play","stop","pause","resume","step"]},"frames":{"type":"integer"}},"required":["action"]}},
 {"name":"console","description":"Lines of the editor console (engine log, script output and errors). Each has a number; pass since to get only newer ones.","inputSchema":{"type":"object","properties":{"since":{"type":"integer"},"limit":{"type":"integer"}}}},
 {"name":"screenshot","description":"A PNG of the scene view (the editor camera) or the game view (the game camera), as last drawn.","inputSchema":{"type":"object","properties":{"view":{"type":"string","enum":["scene","game"]}}}},
 {"name":"save_scene","description":"Save the open scene to its file.","inputSchema":{"type":"object","properties":{}}},
 {"name":"list_assets","description":"Files and folders in a folder of the project (project paths).","inputSchema":{"type":"object","properties":{"folder":{"type":"string","description":"Project path; the project root when left out."}}}},
 {"name":"read_file","description":"A text file of the project, such as a script.","inputSchema":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}},
 {"name":"write_file","description":"Write a text file of the project (a script, say). While the game plays, changed scripts reload.","inputSchema":{"type":"object","properties":{"path":{"type":"string"},"text":{"type":"string"}},"required":["path","text"]}},
 {"name":"undo","description":"Undo the last edit, whoever made it.","inputSchema":{"type":"object","properties":{}}},
 {"name":"input","description":"Play the game: press keys and move or click the mouse while it runs. key is a letter, a digit, space, enter, escape, tab, backspace, left, right, up, down, shift, ctrl, alt or f1..f12; action is tap (down for one update, the default), down or up. For the mouse give x, y in game coordinates (as the game camera sees them) and optionally button (left, right, middle) with an action. Use run step to advance a paused game between inputs.","inputSchema":{"type":"object","properties":{"key":{"type":"string"},"action":{"type":"string","enum":["tap","down","up"]},"x":{"type":"number"},"y":{"type":"number"},"button":{"type":"string","enum":["left","right","middle"]}}}},
 {"name":"select","description":"Select an object in the editor, so the user sees it in the hierarchy and inspector.","inputSchema":{"type":"object","properties":{"object":{"type":"string"}},"required":["object"]}},
 {"name":"instantiate_prefab","description":"Put an instance of a prefab (a project path to a .k2dprefab) in the scene, at x, y in the world when given, under parent when given. Returns its path. One undo step.","inputSchema":{"type":"object","properties":{"path":{"type":"string"},"x":{"type":"number"},"y":{"type":"number"},"parent":{"type":"string"}},"required":["path"]}},
 {"name":"save_prefab","description":"Save an object and its children as a prefab file (a project path ending in .k2dprefab).","inputSchema":{"type":"object","properties":{"object":{"type":"string"},"path":{"type":"string"}},"required":["object","path"]}},
 {"name":"reparent","description":"Move an object under another (parent), or to the top when parent is empty. It keeps its local position. One undo step.","inputSchema":{"type":"object","properties":{"object":{"type":"string"},"parent":{"type":"string"}},"required":["object"]}}
])JSON";

const char* const kInstructions =
    "Kinetix2D editor. Objects are named by path (child indices joined by dots, from scene_tree) or by name. "
    "Every change is an undo step in the editor the user is looking at; nothing is saved until save_scene. "
    "World coordinates are pixels, y grows downwards. Scripts are zenpy classes deriving ScriptComponent "
    "(on_start, on_update(self, dt), ...).";

ct::Json text(const ct::String& value)
{
    ct::Json item = ct::Json::object();
    item.set("type", ct::Json("text"));
    item.set("text", ct::Json(value.c_str()));
    ct::Json content = ct::Json::array();
    content.push_back(item);
    ct::Json result = ct::Json::object();
    result.set("content", content);
    return result;
}

ct::Json fail(const ct::String& message, bool& isError)
{
    isError = true;
    return text(message);
}

ct::Json vec2(const Math::Vec2& v)
{
    ct::Json a = ct::Json::array();
    a.push_back(ct::Json(static_cast<double>(v.x)));
    a.push_back(ct::Json(static_cast<double>(v.y)));
    return a;
}

ct::String base64(const ct::Vector<unsigned char>& bytes)
{
    static const char* const table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    ct::String out;
    std::size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3)
    {
        const unsigned v = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += table[(v >> 6) & 63];
        out += table[v & 63];
    }
    if (i + 1 == bytes.size())
    {
        const unsigned v = bytes[i] << 16;
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += "==";
    }
    else if (i + 2 == bytes.size())
    {
        const unsigned v = (bytes[i] << 16) | (bytes[i + 1] << 8);
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += table[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

const char* typeName(PropertyType type)
{
    switch (type)
    {
    case PropertyType::Bool: return "bool";
    case PropertyType::Int: return "int";
    case PropertyType::Float: return "float";
    case PropertyType::Vec2: return "vec2";
    case PropertyType::Color: return "color";
    case PropertyType::String: return "string";
    case PropertyType::ResourcePath: return "path";
    case PropertyType::Nil: break;
    }
    return "nil";
}

ct::Json valueJson(const PropertyValue& value, const PropertyInfo& info)
{
    switch (value.type())
    {
    case PropertyType::Bool: return ct::Json(value.asBool());
    case PropertyType::Int:
        if (info.hint == PropertyHint::Enum)
            return ct::Json(info.enumName(static_cast<std::size_t>(value.asInt())).c_str());
        return ct::Json(value.asInt());
    case PropertyType::Float: return ct::Json(static_cast<double>(value.asFloat()));
    case PropertyType::Vec2: return vec2(value.asVec2());
    case PropertyType::Color:
    {
        const Color c = value.asColor();
        ct::Json a = ct::Json::array();
        a.push_back(ct::Json(static_cast<double>(c.r)));
        a.push_back(ct::Json(static_cast<double>(c.g)));
        a.push_back(ct::Json(static_cast<double>(c.b)));
        a.push_back(ct::Json(static_cast<double>(c.a)));
        return a;
    }
    case PropertyType::String:
    case PropertyType::ResourcePath: return ct::Json(value.asString().c_str());
    case PropertyType::Nil: break;
    }
    return ct::Json();
}

// A JSON value as the property's type wants it; false when it cannot be.
bool jsonValue(const ct::Json& json, const PropertyInfo& info, PropertyValue& out, ct::String& why)
{
    auto number = [&](std::size_t i, double fallback) {
        return json.is_array() && json.size() > i ? json[i].as_double(fallback) : fallback;
    };
    switch (info.type)
    {
    case PropertyType::Bool:
        out = PropertyValue::from(json.is_bool() ? json.as_bool() : json.as_int(0) != 0);
        return true;
    case PropertyType::Int:
        if (info.hint == PropertyHint::Enum && json.is_string())
        {
            for (std::size_t i = 0; i < info.enumCount(); ++i)
                if (info.enumName(i) == json.as_cstr())
                {
                    out = PropertyValue::from(static_cast<int>(i));
                    return true;
                }
            why = "not one of the names: ";
            for (std::size_t i = 0; i < info.enumCount(); ++i)
                why += (i ? ", " : "") + info.enumName(i);
            return false;
        }
        if (!json.is_number())
            break;
        out = PropertyValue::from(static_cast<int>(json.as_int()));
        return true;
    case PropertyType::Float:
        if (!json.is_number())
            break;
        out = PropertyValue::from(static_cast<float>(json.as_double()));
        return true;
    case PropertyType::Vec2:
        if (json.is_object())
            out = PropertyValue::from(Math::Vec2(static_cast<float>(json["x"].as_double()),
                                                 static_cast<float>(json["y"].as_double())));
        else if (json.is_array() && json.size() >= 2)
            out = PropertyValue::from(Math::Vec2(static_cast<float>(number(0, 0)), static_cast<float>(number(1, 0))));
        else
            break;
        return true;
    case PropertyType::Color:
        if (!json.is_array() || json.size() < 3)
            break;
        out = PropertyValue::from(Color(static_cast<float>(number(0, 1)), static_cast<float>(number(1, 1)),
                                        static_cast<float>(number(2, 1)), static_cast<float>(number(3, 1))));
        return true;
    case PropertyType::String:
        out = PropertyValue::from(ct::String(json.as_cstr()));
        return true;
    case PropertyType::ResourcePath:
        out = PropertyValue::resourcePath(json.as_cstr());
        return true;
    case PropertyType::Nil:
        break;
    }
    why = ct::String("expected a ") + typeName(info.type);
    return false;
}

} // namespace

bool McpServer::start(std::uint16_t port)
{
    stop();
    ct::Address address;
    if (!ct::Address::parse("127.0.0.1", port, address) || !mServer.listen(address))
    {
        log_error("MCP: could not listen on 127.0.0.1:%u", static_cast<unsigned>(port));
        return false;
    }
    mServer.route("*", "/mcp", [this](const ct::HttpRequest& request, ct::HttpResponse& response) {
        handle(request, response);
    });
    mRunning = true;
    mPort = port;
    log_info("MCP server on http://127.0.0.1:%u/mcp", static_cast<unsigned>(port));
    return true;
}

void McpServer::stop()
{
    if (!mRunning)
        return;
    mServer.stop();
    mServer.poll(0);
    mRunning = false;
    log_info("MCP server stopped");
}

void McpServer::poll()
{
    if (mRunning)
        mServer.poll(0);
}

void McpServer::handle(const ct::HttpRequest& request, ct::HttpResponse& response)
{
    // Only pages from this machine may call in (no DNS rebinding from a web page).
    const ct::String origin(request.header("Origin"));
    if (!origin.empty() && origin.find("://localhost") == ct::String::npos &&
        origin.find("://127.0.0.1") == ct::String::npos)
    {
        response.status = 403;
        return;
    }
    if (request.method != "POST")
    {
        // No server-to-client stream: everything is answered in the POST.
        response.status = 405;
        response.set("Allow", "POST");
        return;
    }
    ct::Json::Error error;
    const ct::Json message = ct::Json::parse(request.body.c_str(), &error);
    ct::Json reply;
    if (error)
    {
        reply = ct::Json::object();
        reply.set("jsonrpc", ct::Json("2.0"));
        reply.set("id", ct::Json());
        ct::Json e = ct::Json::object();
        e.set("code", ct::Json(-32700));
        e.set("message", ct::Json("Parse error"));
        reply.set("error", e);
    }
    else if (message.is_array())
    {
        reply = ct::Json::array();
        for (std::size_t i = 0; i < message.size(); ++i)
        {
            ct::Json one = dispatch(message[i]);
            if (!one.is_null())
                reply.push_back(one);
        }
        if (reply.size() == 0)
            reply = ct::Json();
    }
    else
        reply = dispatch(message);

    if (reply.is_null())
    {
        response.status = 202;   // notifications and responses get no answer
        return;
    }
    response.status = 200;
    response.set("Content-Type", "application/json");
    response.body = reply.dump();
}

ct::Json McpServer::dispatch(const ct::Json& message)
{
    const ct::String method = message["method"].as_cstr();
    const ct::Json* id = message.find("id");
    if (!id || method.empty())
        return ct::Json();   // a notification, or a client's response

    ct::Json reply = ct::Json::object();
    reply.set("jsonrpc", ct::Json("2.0"));
    reply.set("id", *id);
    const ct::Json& params = message["params"];
    if (method == "initialize")
    {
        ct::Json result = ct::Json::object();
        const char* asked = params["protocolVersion"].as_cstr(kProtocolVersion);
        result.set("protocolVersion", ct::Json(asked[0] ? asked : kProtocolVersion));
        ct::Json tools = ct::Json::object();
        tools.set("listChanged", ct::Json(false));
        ct::Json capabilities = ct::Json::object();
        capabilities.set("tools", tools);
        result.set("capabilities", capabilities);
        ct::Json info = ct::Json::object();
        info.set("name", ct::Json("kinetix2d-editor"));
        info.set("version", ct::Json("0.1.0"));
        result.set("serverInfo", info);
        result.set("instructions", ct::Json(kInstructions));
        reply.set("result", result);
    }
    else if (method == "ping")
        reply.set("result", ct::Json::object());
    else if (method == "tools/list")
    {
        ct::Json result = ct::Json::object();
        result.set("tools", ct::Json::parse(kTools));
        reply.set("result", result);
    }
    else if (method == "tools/call")
    {
        bool isError = false;
        ct::Json result = callTool(params["name"].as_cstr(), params["arguments"], isError);
        if (isError)
            result.set("isError", ct::Json(true));
        reply.set("result", result);
    }
    else
    {
        ct::Json e = ct::Json::object();
        e.set("code", ct::Json(-32601));
        e.set("message", ct::Json((ct::String("Method not found: ") + method).c_str()));
        reply.set("error", e);
    }
    return reply;
}

ct::Json McpServer::callTool(const ct::String& name, const ct::Json& arguments, bool& isError)
{
    if (name == "scene_info") return sceneInfo();
    if (name == "scene_tree") return sceneTree(arguments);
    if (name == "get_object") return getObject(arguments, isError);
    if (name == "set_property") return setProperty(arguments, isError);
    if (name == "create_object") return createObject(arguments, isError);
    if (name == "add_component") return addComponent(arguments, isError);
    if (name == "delete_object") return deleteObject(arguments, isError);
    if (name == "component_types") return componentTypes();
    if (name == "run") return run(arguments, isError);
    if (name == "console") return console(arguments);
    if (name == "screenshot") return screenshot(arguments, isError);
    if (name == "save_scene") return saveScene(isError);
    if (name == "list_assets") return listAssets(arguments, isError);
    if (name == "read_file") return readFile(arguments, isError);
    if (name == "write_file") return writeFile(arguments, isError);
    if (name == "undo") return undo();
    if (name == "input") return input(arguments, isError);
    if (name == "select") return selectObject(arguments, isError);
    if (name == "reparent") return reparent(arguments, isError);
    if (name == "instantiate_prefab") return instantiatePrefab(arguments, isError);
    if (name == "save_prefab") return savePrefab(arguments, isError);
    return fail(ct::String("No tool named ") + name, isError);
}

// ── Objects ────────────────────────────────────────────────────────────────

ct::String McpServer::pathOf(const GameObject* object) const
{
    ct::String path;
    while (object && object->parent())
    {
        char index[16];
        std::snprintf(index, sizeof index, "%d", static_cast<int>(object->parent()->childIndex(object)));
        path = path.empty() ? ct::String(index) : ct::String(index) + "." + path;
        object = object->parent();
    }
    return path;
}

GameObject* McpServer::resolve(const ct::Json& reference) const
{
    const ct::String ref = reference.is_number() ? ct::String() : ct::String(reference.as_cstr());
    GameObject& root = mEditor.mScene.root();
    if (reference.is_number())
        return static_cast<std::size_t>(reference.as_int()) < root.childCount()
                   ? root.child(static_cast<std::size_t>(reference.as_int()))
                   : nullptr;
    if (ref.empty())
        return nullptr;
    bool isPath = true;
    for (char c : ref)
        isPath = isPath && ((c >= '0' && c <= '9') || c == '.');
    if (isPath)
    {
        GameObject* object = &root;
        std::size_t start = 0;
        while (start <= ref.size())
        {
            std::size_t end = ref.find('.', start);
            if (end == ct::String::npos)
                end = ref.size();
            const int index = std::atoi(ref.substr(start, end - start).c_str());
            if (index < 0 || static_cast<std::size_t>(index) >= object->childCount())
                return nullptr;
            object = object->child(static_cast<std::size_t>(index));
            start = end + 1;
        }
        return object;
    }
    return mEditor.mScene.find(ref.c_str());
}

Component* McpServer::findComponent(GameObject& object, const ct::String& className, int index) const
{
    int seen = 0;
    for (std::size_t t = 0; t < static_cast<std::size_t>(ComponentType::Count); ++t)
        for (std::size_t i = 0; i < object.rawComponentCount(static_cast<ComponentType>(t)); ++i)
        {
            Component* component = object.rawComponent(static_cast<ComponentType>(t), i);
            const ClassInfo* info = component ? ClassDB::find(*component) : nullptr;
            if (info && info->name == className && seen++ == index)
                return component;
        }
    return nullptr;
}

ct::Json McpServer::sceneInfo()
{
    ct::Json info = ct::Json::object();
    info.set("file", ct::Json(mEditor.mScenePath.c_str()));
    info.set("unsavedChanges", ct::Json(mEditor.mDirty));
    info.set("playing", ct::Json(mEditor.mPlaying));
    info.set("paused", ct::Json(mEditor.mPaused));
    info.set("objects", ct::Json(static_cast<std::int64_t>(mEditor.mScene.objectCount())));
    info.set("projectRoot", ct::Json(mEditor.mProjectRoot.c_str()));
    info.set("selected", ct::Json(mEditor.mSelected ? pathOf(mEditor.mSelected).c_str() : ""));
    return text(info.dump(2));
}

ct::Json McpServer::sceneTree(const ct::Json& arguments)
{
    const int depth = static_cast<int>(arguments["depth"].as_int(1000));
    ct::Function<ct::Json(GameObject&, int)> describe = [&](GameObject& object, int level) {
        ct::Json node = ct::Json::object();
        node.set("path", ct::Json(pathOf(&object).c_str()));
        node.set("name", ct::Json(object.name().c_str()));
        node.set("position", vec2(object.position()));
        node.set("rotation", ct::Json(static_cast<double>(object.rotationDegrees())));
        node.set("scale", vec2(object.scale()));
        if (!object.visible())
            node.set("visible", ct::Json(false));
        if (!object.active())
            node.set("active", ct::Json(false));
        ct::Json components = ct::Json::array();
        for (std::size_t t = 0; t < static_cast<std::size_t>(ComponentType::Count); ++t)
            for (std::size_t i = 0; i < object.rawComponentCount(static_cast<ComponentType>(t)); ++i)
                if (const Component* c = object.rawComponent(static_cast<ComponentType>(t), i))
                {
                    const ClassInfo* info = ClassDB::find(*c);
                    components.push_back(ct::Json(info ? info->name.c_str() : "?"));
                }
        node.set("components", components);
        if (object.childCount() > 0)
        {
            if (level < depth)
            {
                ct::Json children = ct::Json::array();
                for (std::size_t i = 0; i < object.childCount(); ++i)
                    children.push_back(describe(*object.child(i), level + 1));
                node.set("children", children);
            }
            else
                node.set("childCount", ct::Json(static_cast<std::int64_t>(object.childCount())));
        }
        return node;
    };
    ct::Json roots = ct::Json::array();
    GameObject& root = mEditor.mScene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        roots.push_back(describe(*root.child(i), 1));
    return text(roots.dump(2));
}

ct::Json McpServer::getObject(const ct::Json& arguments, bool& isError)
{
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    ct::Json result = ct::Json::object();
    result.set("path", ct::Json(pathOf(object).c_str()));
    result.set("name", ct::Json(object->name().c_str()));
    ct::Json transform = ct::Json::object();
    transform.set("position", vec2(object->position()));
    transform.set("globalPosition", vec2(object->globalPosition()));
    transform.set("rotation", ct::Json(static_cast<double>(object->rotationDegrees())));
    transform.set("scale", vec2(object->scale()));
    transform.set("visible", ct::Json(object->visible()));
    transform.set("active", ct::Json(object->active()));
    result.set("Transform", transform);
    ct::Json components = ct::Json::array();
    for (std::size_t t = 0; t < static_cast<std::size_t>(ComponentType::Count); ++t)
        for (std::size_t i = 0; i < object->rawComponentCount(static_cast<ComponentType>(t)); ++i)
        {
            const Component* c = object->rawComponent(static_cast<ComponentType>(t), i);
            const ClassInfo* info = c ? ClassDB::find(*c) : nullptr;
            ct::Json component = ct::Json::object();
            component.set("class", ct::Json(info ? info->name.c_str() : "?"));
            if (const ZenScriptComponent* script = c && t == static_cast<std::size_t>(ComponentType::Script)
                                                       ? static_cast<const ZenScriptComponent*>(c)
                                                       : nullptr)
                component.set("script", ct::Json(script->scriptPath().c_str()));
            ct::Json properties = ct::Json::array();
            for (std::size_t p = 0; info && p < info->properties.size(); ++p)
            {
                const ClassProperty& property = info->properties[p];
                ct::Json entry = ct::Json::object();
                entry.set("name", ct::Json(property.info.name.c_str()));
                entry.set("type", ct::Json(typeName(property.info.type)));
                entry.set("value", valueJson(property.get(*c), property.info));
                if (property.info.hint == PropertyHint::Enum)
                {
                    ct::Json names = ct::Json::array();
                    for (std::size_t e = 0; e < property.info.enumCount(); ++e)
                        names.push_back(ct::Json(property.info.enumName(e).c_str()));
                    entry.set("options", names);
                }
                else if (property.info.hint == PropertyHint::Range)
                {
                    entry.set("min", ct::Json(static_cast<double>(property.info.rangeMin)));
                    entry.set("max", ct::Json(static_cast<double>(property.info.rangeMax)));
                }
                properties.push_back(entry);
            }
            component.set("properties", properties);
            components.push_back(component);
        }
    result.set("components", components);
    ct::Json children = ct::Json::array();
    for (std::size_t i = 0; i < object->childCount(); ++i)
        children.push_back(ct::Json(object->child(i)->name().c_str()));
    result.set("children", children);
    return text(result.dump(2));
}

ct::Json McpServer::setProperty(const ct::Json& arguments, bool& isError)
{
    if (mEditor.mPlaying)
        return fail("Stop the game first: edits during play are thrown away on stop", isError);
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    const ct::String componentName = arguments["component"].as_cstr();
    const ct::String property = arguments["property"].as_cstr();
    const ct::Json& value = arguments["value"];

    if (componentName == "Transform")
    {
        auto pair = [&](Math::Vec2& out) {
            if (value.is_array() && value.size() >= 2)
                out = Math::Vec2(static_cast<float>(value[0].as_double()), static_cast<float>(value[1].as_double()));
            else if (value.is_object())
                out = Math::Vec2(static_cast<float>(value["x"].as_double()), static_cast<float>(value["y"].as_double()));
            else
                return false;
            return true;
        };
        Math::Vec2 v;
        if (property == "name" && value.is_string())
            object->setName(value.as_cstr());
        else if (property == "position" && pair(v))
            object->setPosition(v);
        else if (property == "scale" && pair(v))
            object->setScale(v);
        else if (property == "rotation" && value.is_number())
            object->setRotationDegrees(static_cast<float>(value.as_double()));
        else if (property == "visible")
            object->setVisible(value.as_bool(true));
        else if (property == "active")
            object->setActive(value.as_bool(true));
        else
            return fail("Transform takes name, position [x,y], rotation, scale [x,y], visible, active", isError);
    }
    else
    {
        Component* component = findComponent(*object, componentName, static_cast<int>(arguments["index"].as_int(0)));
        const ClassInfo* info = component ? ClassDB::find(*component) : nullptr;
        if (!info)
            return fail(ct::String("The object has no ") + componentName, isError);
        const ClassProperty* target = info->find(property.c_str());
        if (!target)
            return fail(ct::String(componentName) + " has no property " + property, isError);
        PropertyValue converted;
        ct::String why;
        if (!jsonValue(value, target->info, converted, why))
            return fail(ct::String("Bad value for ") + property + ": " + why, isError);
        target->set(*component, converted);
    }
    mEditor.commit((ct::String("MCP: set ") + property).c_str());
    mEditor.refreshTree();
    mEditor.showObject(mEditor.mSelected);
    return text(ct::String("Set ") + componentName + "." + property + " on " + pathOf(object));
}

ct::Json McpServer::createObject(const ct::Json& arguments, bool& isError)
{
    if (mEditor.mPlaying)
        return fail("Stop the game first", isError);
    GameObject* parent = nullptr;
    if (arguments.contains("parent") && !(parent = resolve(arguments["parent"])))
        return fail("No such parent", isError);
    GameObject* object = mEditor.mScene.createObject(arguments["name"].as_cstr("GameObject"), parent);
    if (!object)
        return fail("Could not create the object", isError);
    const Math::Vec2 world(static_cast<float>(arguments["x"].as_double(0.0)),
                           static_cast<float>(arguments["y"].as_double(0.0)));
    object->setPosition(object->parent()->globalTransform().AffineInverse().Transform(world));
    mEditor.commit("MCP: create object");
    mEditor.select(object);
    return text(ct::String("Created ") + object->name() + " at path " + pathOf(object));
}

ct::Json McpServer::addComponent(const ct::Json& arguments, bool& isError)
{
    if (mEditor.mPlaying)
        return fail("Stop the game first", isError);
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    const ct::String type = arguments["type"].as_cstr();
    if (type == "ZenScript" || type == "Script")
    {
        const ct::String path = arguments["script"].as_cstr();
        if (path.empty())
            return fail("A script needs script: the project path of a .py file", isError);
        ZenScriptComponent* script = object->addComponent<ZenScriptComponent>();
        const bool loaded = script->loadFile(path.c_str());
        mEditor.commit("MCP: add script");
        mEditor.refreshTree();
        mEditor.showObject(mEditor.mSelected);
        if (!loaded)
            return fail(ct::String("Added, but ") + path + " did not load: see console", isError);
        return text(ct::String("Added script ") + path + " to " + pathOf(object));
    }
    if (!Serializer::Create(*object, type.c_str()))
        return fail(ct::String("No component type ") + type + " (see component_types)", isError);
    mEditor.commit("MCP: add component");
    mEditor.refreshTree();
    mEditor.showObject(mEditor.mSelected);
    return text(ct::String("Added ") + type + " to " + pathOf(object));
}

ct::Json McpServer::deleteObject(const ct::Json& arguments, bool& isError)
{
    if (mEditor.mPlaying)
        return fail("Stop the game first", isError);
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    const ct::String name = object->name();
    mEditor.select(object);
    mEditor.runHierarchyAction(HierarchyAction::Delete);
    return text(ct::String("Deleted ") + name);
}

ct::Json McpServer::componentTypes()
{
    ct::Json types = ct::Json::array();
    for (std::size_t i = 0; i < Serializer::TypeCount(); ++i)
        types.push_back(ct::Json(Serializer::TypeName(i)));
    return text(types.dump());
}

// ── Running ────────────────────────────────────────────────────────────────

ct::Json McpServer::run(const ct::Json& arguments, bool& isError)
{
    const ct::String action = arguments["action"].as_cstr();
    if (action == "play")
        mEditor.startPlay();
    else if (action == "stop")
        mEditor.stopPlay();
    else if (action == "pause" || action == "resume")
        mEditor.setPaused(action == "pause");
    else if (action == "step")
    {
        if (!mEditor.mPlaying)
            mEditor.startPlay();
        mEditor.setPaused(true);
        const int frames = static_cast<int>(arguments["frames"].as_int(1));
        for (int i = 0; i < frames && i < 6000; ++i)
        {
            mEditor.mScene.update(1.0f / 60.0f);
            DispatchZenScriptEvents(mEditor.mScene.root());
            mEditor.mGameInput.NewFrame();
        }
    }
    else
        return fail("action is play, stop, pause, resume or step", isError);
    mEditor.updateRunBar();
    char state[96];
    std::snprintf(state, sizeof state, "%s%s, %d objects", mEditor.mPlaying ? "playing" : "stopped",
                  mEditor.mPaused ? " (paused)" : "", static_cast<int>(mEditor.mScene.objectCount()));
    return text(state);
}

ct::Json McpServer::console(const ct::Json& arguments)
{
    const ct::Vector<ct::String>& lines = mEditor.mLogHistory;
    const std::size_t first = mEditor.mLogFirst;
    const std::size_t end = first + lines.size();
    std::size_t since = static_cast<std::size_t>(arguments["since"].as_int(0));
    const std::size_t limit = static_cast<std::size_t>(arguments["limit"].as_int(100));
    if (since < first)
        since = first;
    if (end > since + limit)
        since = end - limit;
    ct::String out;
    for (std::size_t n = since; n < end; ++n)
    {
        char number[24];
        std::snprintf(number, sizeof number, "%d ", static_cast<int>(n));
        out += number;
        out += lines[n - first];
        out += "\n";
    }
    char next[48];
    std::snprintf(next, sizeof next, "(next since: %d)", static_cast<int>(end));
    out += next;
    return text(out);
}

ct::Json McpServer::screenshot(const ct::Json& arguments, bool& isError)
{
    const bool game = ct::String(arguments["view"].as_cstr("scene")) == "game";
    RenderTarget& target = game ? mEditor.mGameTarget : mEditor.mTarget;
    const int w = target.width(), h = target.height();
    if (w <= 0 || h <= 0)
        return fail(game ? "The game view has not been drawn yet: show its tab" : "The scene view has not been drawn yet",
                    isError);
    ct::Vector<unsigned char> pixels;
    pixels.resize(static_cast<std::size_t>(w) * h * 4);
    target.bind();
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // GL rows go bottom up, and the image is opaque.
    ct::Vector<unsigned char> flipped;
    flipped.resize(pixels.size());
    for (int y = 0; y < h; ++y)
        std::memcpy(&flipped[static_cast<std::size_t>(y) * w * 4], &pixels[static_cast<std::size_t>(h - 1 - y) * w * 4],
                    static_cast<std::size_t>(w) * 4);
    for (std::size_t i = 3; i < flipped.size(); i += 4)
        flipped[i] = 255;
    ct::Vector<unsigned char> png;
    if (!EncodePng(w, h, flipped.data(), w * 4, png))
        return fail("Could not encode the image", isError);
    ct::Json image = ct::Json::object();
    image.set("type", ct::Json("image"));
    image.set("data", ct::Json(base64(png).c_str()));
    image.set("mimeType", ct::Json("image/png"));
    ct::Json content = ct::Json::array();
    content.push_back(image);
    ct::Json result = ct::Json::object();
    result.set("content", content);
    return result;
}

ct::Json McpServer::saveScene(bool& isError)
{
    if (mEditor.mScenePath.empty())
        return fail("The scene has no file yet: save it once from the editor", isError);
    if (!mEditor.saveScene(mEditor.mScenePath.c_str()))
        return fail("Could not save: see console", isError);
    return text(ct::String("Saved ") + mEditor.mScenePath);
}

// ── Files ──────────────────────────────────────────────────────────────────

bool McpServer::projectPath(const ct::String& relative, ct::String& absolute) const
{
    if (mEditor.mProjectRoot.empty() || relative.find("..") != ct::String::npos ||
        EditorFileSystem::isAbsolute(relative))
        return false;
    absolute = relative.empty() ? mEditor.mProjectRoot : EditorFileSystem::join(mEditor.mProjectRoot, relative.c_str());
    return true;
}

ct::Json McpServer::listAssets(const ct::Json& arguments, bool& isError)
{
    ct::String folder;
    if (!projectPath(arguments["folder"].as_cstr(""), folder))
        return fail("Give a project path inside the project", isError);
    ct::Vector<EditorFileEntry> entries;
    if (!EditorFileSystem::listDirectory(folder, entries))
        return fail("Cannot read that folder", isError);
    ct::Json list = ct::Json::array();
    for (const EditorFileEntry& entry : entries)
        if (!entry.name.empty() && entry.name[0] != '.')
        {
            ct::String path = EditorFileSystem::relativeTo(mEditor.mProjectRoot, entry.path);
            if (entry.directory)
                path += "/";
            list.push_back(ct::Json(path.c_str()));
        }
    return text(list.dump(1));
}

ct::Json McpServer::readFile(const ct::Json& arguments, bool& isError)
{
    ct::String path;
    if (!projectPath(arguments["path"].as_cstr(), path))
        return fail("Give a project path inside the project", isError);
    FileBuffer file;
    if (!FileSystem::Instance().LoadFile(path.c_str(), file, true))
        return fail("Cannot read that file", isError);
    return text(ct::String(file.Text(), file.Size()));
}

ct::Json McpServer::writeFile(const ct::Json& arguments, bool& isError)
{
    ct::String path;
    if (!projectPath(arguments["path"].as_cstr(), path))
        return fail("Give a project path inside the project", isError);
    if (!FileSystem::Instance().SaveTextFile(path.c_str(), ct::String(arguments["text"].as_cstr())))
        return fail("Cannot write that file", isError);
    log_info("MCP wrote %s", arguments["path"].as_cstr());
    if (mEditor.mPlaying)
        ReloadChangedZenScripts();
    mEditor.mAssetsPanel->refresh();
    return text(ct::String("Wrote ") + arguments["path"].as_cstr());
}

ct::Json McpServer::undo()
{
    mEditor.undo();
    return text(ct::String("Undone; next undo: ") + mEditor.mCommands.undoName());
}

// ── Playing, selection, hierarchy ──────────────────────────────────────────

namespace
{
int keyFromName(const ct::String& raw)
{
    ct::String name;
    for (char c : raw)
        name += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z')
        return static_cast<int>(Key::A) + (name[0] - 'a');
    if (name.size() == 1 && name[0] >= '1' && name[0] <= '9')
        return static_cast<int>(Key::Num1) + (name[0] - '1');
    if (name == "0")
        return static_cast<int>(Key::Num0);
    if (name.size() >= 2 && name[0] == 'f')
    {
        const int n = std::atoi(name.c_str() + 1);
        if (n >= 1 && n <= 12)
            return static_cast<int>(Key::F1) + n - 1;
    }
    static const struct
    {
        const char* name;
        Key key;
    } kNames[] = {{"space", Key::Space},     {"enter", Key::Enter},   {"return", Key::Enter}, {"escape", Key::Escape},
                  {"esc", Key::Escape},      {"tab", Key::Tab},       {"backspace", Key::Backspace},
                  {"left", Key::Left},       {"right", Key::Right},   {"up", Key::Up},         {"down", Key::Down},
                  {"shift", Key::LeftShift}, {"ctrl", Key::LeftCtrl}, {"alt", Key::LeftAlt}};
    for (const auto& entry : kNames)
        if (name == entry.name)
            return static_cast<int>(entry.key);
    return -1;
}
} // namespace

ct::Json McpServer::input(const ct::Json& arguments, bool& isError)
{
    if (!mEditor.mPlaying)
        return fail("The game is not playing: run play first", isError);
    const ct::String action = arguments["action"].as_cstr("tap");
    ct::String done;
    if (arguments.contains("key"))
    {
        const int key = keyFromName(arguments["key"].as_cstr());
        if (key < 0)
            return fail(ct::String("Unknown key ") + arguments["key"].as_cstr(), isError);
        if (action == "up")
            mEditor.mGameInput.OnKey(key, false, false);
        else
        {
            mEditor.mGameInput.OnKey(key, true, false);
            if (action != "down")
                mEditor.mPendingReleases.push_back(key);
        }
        done += ct::String(action) + " " + arguments["key"].as_cstr();
    }
    if (arguments.contains("x") && arguments.contains("y"))
    {
        if (mEditor.mGameVirtualW <= 0.0f || mEditor.mGameVirtualH <= 0.0f)
            return fail("The game view has not been drawn: show its tab", isError);
        // Game coordinates to window points, through the game view's viewport.
        const float x = mEditor.mGameViewX +
                        static_cast<float>(arguments["x"].as_double()) * mEditor.mGameViewW / mEditor.mGameVirtualW;
        const float y = mEditor.mGameViewY +
                        static_cast<float>(arguments["y"].as_double()) * mEditor.mGameViewH / mEditor.mGameVirtualH;
        mEditor.mGameInput.OnMouseMove(x, y);
        done += done.empty() ? "mouse moved" : ", mouse moved";
    }
    if (arguments.contains("button"))
    {
        const ct::String which = arguments["button"].as_cstr();
        const int button = which == "right" ? 1 : (which == "middle" ? 2 : 0);
        if (action == "up")
            mEditor.mGameInput.OnMouseButton(button, false);
        else
        {
            mEditor.mGameInput.OnMouseButton(button, true);
            if (action != "down")
                mEditor.mGameInput.OnMouseButton(button, false);
        }
        done += ct::String(", ") + action + " " + which + " button";
    }
    if (done.empty())
        return fail("Give a key, a mouse position (x, y) or a button", isError);
    return text(done);
}

ct::Json McpServer::selectObject(const ct::Json& arguments, bool& isError)
{
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    mEditor.select(object);
    return text(ct::String("Selected ") + object->name());
}

ct::Json McpServer::reparent(const ct::Json& arguments, bool& isError)
{
    if (mEditor.mPlaying)
        return fail("Stop the game first", isError);
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    const ct::String parentRef = arguments["parent"].as_cstr("");
    GameObject* parent = parentRef.empty() ? nullptr : resolve(arguments["parent"]);
    if (!parentRef.empty() && !parent)
        return fail("No such parent", isError);
    for (GameObject* p = parent; p; p = p->parent())
        if (p == object)
            return fail("An object cannot go under itself or its own children", isError);
    mEditor.dropObject(object, parent, parent ? ig::retained::TreeView::DropZone::Inside
                                              : ig::retained::TreeView::DropZone::After);
    return text(ct::String("Moved ") + object->name() + " to path " + pathOf(object));
}

ct::Json McpServer::instantiatePrefab(const ct::Json& arguments, bool& isError)
{
    if (mEditor.mPlaying)
        return fail("Stop the game first", isError);
    ct::String absolute;
    const ct::String path = arguments["path"].as_cstr();
    if (!projectPath(path, absolute) || !EditorFileSystem::exists(absolute))
        return fail("Give the project path of a .k2dprefab", isError);
    GameObject* parent = nullptr;
    if (arguments.contains("parent") && !(parent = resolve(arguments["parent"])))
        return fail("No such parent", isError);
    GameObject* object = mEditor.instantiatePrefab(absolute, parent, false, 0.0f, 0.0f);
    if (!object)
        return fail("The prefab did not load: see console", isError);
    if (arguments.contains("x") || arguments.contains("y"))
    {
        const Math::Vec2 world(static_cast<float>(arguments["x"].as_double(0.0)),
                               static_cast<float>(arguments["y"].as_double(0.0)));
        object->setPosition(object->parent()->globalTransform().AffineInverse().Transform(world));
        mEditor.commit("MCP: place prefab");
        mEditor.showObject(object);
    }
    return text(ct::String("Instantiated ") + path + " as " + object->name() + " at path " + pathOf(object));
}

ct::Json McpServer::savePrefab(const ct::Json& arguments, bool& isError)
{
    GameObject* object = resolve(arguments["object"]);
    if (!object)
        return fail("No such object", isError);
    ct::String absolute;
    ct::String path = arguments["path"].as_cstr();
    if (EditorFileSystem::extension(path) != "k2dprefab")
        path += ".k2dprefab";
    if (!projectPath(path, absolute))
        return fail("Give a project path inside the project", isError);
    if (!mEditor.savePrefab(object, absolute))
        return fail("Could not save the prefab: see console", isError);
    return text(ct::String("Saved ") + object->name() + " as " + path);
}

} // namespace k2d::editor
