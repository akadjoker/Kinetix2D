#pragma once

#include <ct/http_server.hpp>
#include <ct/json.hpp>
#include <ct/string.hpp>

#include <cstdint>

namespace k2d
{
class Component;
class GameObject;
}

namespace k2d::editor
{

class EditorWindow;

// The editor as an MCP server (Streamable HTTP, JSON responses) on 127.0.0.1:
// an LLM client reads the scene, changes it, runs the game and looks at it,
// in the editor that is open. Polled from the editor's frame, so every tool
// runs on the main thread like any other edit, and every change it makes is
// one undo step.
class McpServer
{
public:
    explicit McpServer(EditorWindow& editor) : mEditor(editor) {}

    bool start(std::uint16_t port);
    void stop();
    bool running() const { return mRunning; }
    std::uint16_t port() const { return mPort; }
    // Serves what has arrived; called once a frame.
    void poll();

private:
    void handle(const ct::HttpRequest& request, ct::HttpResponse& response);
    ct::Json dispatch(const ct::Json& message);
    ct::Json callTool(const ct::String& name, const ct::Json& arguments, bool& isError);

    // Tools.
    ct::Json sceneInfo();
    ct::Json sceneTree(const ct::Json& arguments);
    ct::Json getObject(const ct::Json& arguments, bool& isError);
    ct::Json setProperty(const ct::Json& arguments, bool& isError);
    ct::Json createObject(const ct::Json& arguments, bool& isError);
    ct::Json addComponent(const ct::Json& arguments, bool& isError);
    ct::Json deleteObject(const ct::Json& arguments, bool& isError);
    ct::Json componentTypes();
    ct::Json run(const ct::Json& arguments, bool& isError);
    ct::Json console(const ct::Json& arguments);
    ct::Json screenshot(const ct::Json& arguments, bool& isError);
    ct::Json saveScene(bool& isError);
    ct::Json listAssets(const ct::Json& arguments, bool& isError);
    ct::Json readFile(const ct::Json& arguments, bool& isError);
    ct::Json writeFile(const ct::Json& arguments, bool& isError);
    ct::Json undo();
    ct::Json input(const ct::Json& arguments, bool& isError);
    ct::Json selectObject(const ct::Json& arguments, bool& isError);
    ct::Json reparent(const ct::Json& arguments, bool& isError);

    GameObject* resolve(const ct::Json& reference) const;
    ct::String pathOf(const GameObject* object) const;
    Component* findComponent(GameObject& object, const ct::String& className, int index) const;
    bool projectPath(const ct::String& relative, ct::String& absolute) const;

    EditorWindow& mEditor;
    ct::HttpServer mServer;
    bool mRunning = false;
    std::uint16_t mPort = 0;
};

} // namespace k2d::editor
