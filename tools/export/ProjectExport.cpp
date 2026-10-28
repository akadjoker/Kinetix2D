#include "ProjectExport.h"

#include <k2d/KPak.h>
#include <k2d/ZenRuntime.h>

#include <ct/json.hpp>
#include <ct/vector.hpp>

#include <platform.h>

#include <cstring>

namespace k2d
{

namespace
{

const char* const kPackedExtensions[] = {".png",  ".jpg",  ".jpeg", ".bmp",  ".tga",       ".gif",       ".webp",
                                         ".wav",  ".ogg",  ".mp3",  ".flac", ".ttf",       ".otf",       ".fnt",
                                         ".json", ".txt",  ".csv",  ".xml",  ".tmx",       ".tsx",       ".atlas",
                                         ".glsl", ".vert", ".frag", ".k2dproj", ".k2dscene", ".k2dprefab"};
const char* const kScriptExtensions[] = {".py", ".zen", ".zs"};
const char* const kBundlePath = ".k2d/web/scripts.zbc";
const char* const kManifestPath = ".k2d/web/scripts.json";

template <std::size_t N> bool hasExtension(const char* path, const char* const (&extensions)[N])
{
    for (const char* extension : extensions)
        if (path_has_extension(path, extension))
            return true;
    return false;
}

ct::String slashes(const char* path)
{
    ct::String out(path);
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i] == '\\')
            out[i] = '/';
    return out;
}

bool startsWith(const ct::String& text, const ct::String& prefix)
{
    return text.size() >= prefix.size() && std::strncmp(text.c_str(), prefix.c_str(), prefix.size()) == 0;
}

ct::String join(const ct::String& base, const char* relative)
{
    char buffer[4096];
    return path_join(buffer, sizeof buffer, base.c_str(), relative) ? ct::String(buffer) : ct::String(relative);
}

bool readFile(const char* path, ct::Vector<unsigned char>& out)
{
    size_t size = 0;
    uint8_t* data = file_read(path, &size);
    if (!data)
        return false;
    out.resize(size);
    if (size)
        std::memcpy(out.data(), data, size);
    fs_free(data);
    return true;
}

bool isIdentifierStart(char value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}

ct::String firstClassName(const char* source)
{
    for (const char* cursor = source; *cursor;)
    {
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        if (std::strncmp(cursor, "class", 5) == 0 && (cursor[5] == ' ' || cursor[5] == '\t'))
        {
            cursor += 5;
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            const char* begin = cursor;
            while (isIdentifierStart(*cursor) || (*cursor >= '0' && *cursor <= '9'))
                ++cursor;
            return ct::String(begin, static_cast<size_t>(cursor - begin));
        }
        while (*cursor && *cursor != '\n')
            ++cursor;
        if (*cursor == '\n')
            ++cursor;
    }
    return ct::String();
}

struct Collector
{
    ct::String root;
    ct::Vector<ct::String> files;
};

// Hidden folders (.k2d, .git) and earlier exports stay out of the pack.
bool collect(const char* path, PathType type, void* user)
{
    Collector& collector = *static_cast<Collector*>(user);
    const ct::String relative = slashes(path).substr(collector.root.size() + 1);
    if (relative[0] == '.' || std::strstr(relative.c_str(), "/.") || startsWith(relative, "export/"))
        return true;
    if (type == PATH_TYPE_FILE)
        collector.files.push_back(relative);
    return true;
}

// The editor may have written paths into the project as absolute ones; the
// runner resolves them from the project and its assets folder.
void makePortable(ct::Json& node, const ct::String& root)
{
    if (node.is_object())
    {
        for (auto& member : node.members())
            makePortable(member.value, root);
    }
    else if (node.is_array())
    {
        for (std::size_t i = 0; i < node.size(); ++i)
            makePortable(node[i], root);
    }
    else if (node.is_string())
    {
        const ct::String value = slashes(node.as_cstr(""));
        const ct::String assets = root + "/assets/";
        if (startsWith(value, assets))
            node = ct::Json(value.substr(assets.size()).c_str());
        else if (startsWith(value, root + "/"))
            node = ct::Json(value.substr(root.size() + 1).c_str());
    }
}

bool addScripts(KPakWriter& pack, const Collector& collector, const char* packPath, ct::String& error)
{
    ct::String source;
    ct::Json scripts = ct::Json::array();
    for (const ct::String& relative : collector.files)
    {
        if (!hasExtension(relative.c_str(), kScriptExtensions))
            continue;
        char* text = file_read_text(join(collector.root, relative.c_str()).c_str());
        if (!text)
        {
            error = "Could not read " + relative;
            return false;
        }
        const ct::String className = firstClassName(text);
        if (!className.empty())
        {
            source += "\n";
            source += text;
            source += "\n";
            // Scenes name a script from the project or from its assets folder,
            // and the runtime finds classes by the name as written.
            const ct::String names[2] = {relative,
                                         startsWith(relative, "assets/") ? relative.substr(7) : ct::String()};
            for (const ct::String& name : names)
            {
                if (name.empty())
                    continue;
                ct::Json script = ct::Json::object();
                script.set("path", ct::Json(name.c_str()));
                script.set("class", ct::Json(className.c_str()));
                scripts.push_back(script);
            }
        }
        else
            log_warn("%s has no script class; left out of the export", relative.c_str());
        fs_free(text);
    }
    if (scripts.size() == 0)
        return true;

    const ct::String bundle = ct::String(packPath) + ".zbc";
    ct::String compileError;
    if (!ZenRuntime::instance().compileSourceToBytecode(source.c_str(), "<k2d-export>", bundle.c_str(), true,
                                                        &compileError))
    {
        error = "Scripts did not compile: " + compileError;
        return false;
    }
    ct::Vector<unsigned char> bytecode;
    const bool read = readFile(bundle.c_str(), bytecode);
    fs_remove_path(bundle.c_str());
    if (!read)
    {
        error = "Could not read the compiled scripts";
        return false;
    }
    ct::Json manifest = ct::Json::object();
    manifest.set("format", ct::Json("k2d-zen-bytecode-bundle"));
    manifest.set("version", ct::Json(static_cast<int64_t>(1)));
    manifest.set("scripts", scripts);
    const ct::String text = manifest.dump(2);
    pack.AddData(kBundlePath, bytecode.data(), bytecode.size());
    pack.AddData(kManifestPath, text.c_str(), text.size());
    return true;
}

ct::String escapeHtml(const char* text)
{
    ct::String out;
    for (const char* c = text; *c; ++c)
    {
        if (*c == '<')
            out += "&lt;";
        else if (*c == '>')
            out += "&gt;";
        else if (*c == '&')
            out += "&amp;";
        else if (*c == '"')
            out += "&quot;";
        else
            out += *c;
    }
    return out;
}

const char* const kPageHead =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>";
const char* const kPageBody =
    "</title><style>html,body{margin:0;height:100%;background:#111;overflow:hidden}"
    "canvas{display:block;width:100%;height:100%;outline:0}"
    "#load{position:fixed;left:30%;top:50%;width:40%;height:6px;background:#333;border-radius:3px}"
    "#bar{height:100%;width:0;background:#5c9;border-radius:3px}</style></head><body>"
    "<canvas id=\"canvas\" tabindex=\"0\" oncontextmenu=\"event.preventDefault()\"></canvas>"
    "<div id=\"load\"><div id=\"bar\"></div></div><script>\n"
    "var Module = {canvas: document.getElementById('canvas'), arguments: [], preRun: [function () {\n"
    "  Module.addRunDependency('data.kpak');\n"
    "  fetch('data.kpak').then(function (response) {\n"
    "    if (!response.ok) throw new Error('data.kpak: ' + response.status);\n"
    "    var total = +response.headers.get('Content-Length') || 0, got = 0, parts = [];\n"
    "    var reader = response.body.getReader(), bar = document.getElementById('bar');\n"
    "    function pump() {\n"
    "      return reader.read().then(function (step) {\n"
    "        if (step.done) return;\n"
    "        parts.push(step.value); got += step.value.length;\n"
    "        if (total) bar.style.width = (100 * got / total) + '%';\n"
    "        return pump();\n"
    "      });\n"
    "    }\n"
    "    return pump().then(function () {\n"
    "      var bytes = new Uint8Array(got), at = 0;\n"
    "      parts.forEach(function (part) { bytes.set(part, at); at += part.length; });\n"
    "      return bytes;\n"
    "    });\n"
    "  }).then(function (bytes) {\n"
    "    Module.FS.writeFile('/data.kpak', bytes);\n"
    "    document.getElementById('load').remove();\n"
    "    Module.removeRunDependency('data.kpak');\n"
    "  }).catch(function (e) { document.body.innerHTML = '<pre style=\"color:#e66\">' + e + '</pre>'; });\n"
    "}]};\n"
    "document.getElementById('canvas').focus();\n"
    "</script><script async src=\"k2d_runner.js\"></script></body></html>\n";

} // namespace

bool WriteProjectPack(const char* projectRoot, const char* packPath, ct::String& error)
{
    char absolute[4096];
    if (!path_absolute(absolute, sizeof absolute, projectRoot))
    {
        error = ct::String("Not a folder: ") + projectRoot;
        return false;
    }
    Collector collector;
    collector.root = slashes(absolute);
    while (collector.root.size() > 1 && collector.root[collector.root.size() - 1] == '/')
        collector.root = collector.root.substr(0, collector.root.size() - 1);

    char* projectText = file_read_text(join(collector.root, "project.k2dproj").c_str());
    if (!projectText)
    {
        error = "No project.k2dproj in " + collector.root;
        return false;
    }
    const ct::Json project = ct::Json::parse(projectText);
    fs_free(projectText);
    const char* startup = project["startupScene"].as_cstr("");
    if (!startup[0] || !file_exists(join(collector.root, startup).c_str()))
    {
        error = "The project has no startup scene: open the scene the game starts with and use "
                "File > Set as Startup Scene";
        return false;
    }

    if (!fs_enumerate_directory(collector.root.c_str(), true, collect, &collector))
    {
        error = "Could not list " + collector.root;
        return false;
    }

    KPakWriter pack;
    std::size_t packed = 0;
    for (const ct::String& relative : collector.files)
    {
        if (!hasExtension(relative.c_str(), kPackedExtensions))
            continue;
        const ct::String path = join(collector.root, relative.c_str());
        if (path_has_extension(path.c_str(), ".k2dscene") || path_has_extension(path.c_str(), ".k2dprefab"))
        {
            char* text = file_read_text(path.c_str());
            if (!text)
            {
                error = "Could not read " + relative;
                return false;
            }
            ct::Json::Error parseError;
            ct::Json document = ct::Json::parse(text, &parseError);
            fs_free(text);
            if (parseError)
            {
                error = relative + " is not valid JSON";
                return false;
            }
            makePortable(document, collector.root);
            const ct::String portable = document.dump(2);
            pack.AddData(relative.c_str(), portable.c_str(), portable.size());
        }
        else if (!pack.AddFile(relative.c_str(), path.c_str()))
        {
            error = "Could not add " + relative;
            return false;
        }
        ++packed;
    }
    if (!addScripts(pack, collector, packPath, error))
        return false;
    if (!pack.Write(packPath))
    {
        error = ct::String("Could not write ") + packPath;
        return false;
    }
    log_info("Packed %zu files of %s into %s", packed, collector.root.c_str(), packPath);
    return true;
}

bool ExportWeb(const char* projectRoot, const char* runnerDir, const char* outDir, ct::String& error)
{
    const ct::String runner(runnerDir);
    const ct::String out(outDir);
    const char* const runnerFiles[] = {"k2d_runner.js", "k2d_runner.wasm"};
    for (const char* name : runnerFiles)
        if (!file_exists(join(runner, name).c_str()))
        {
            error = "No web runner in " + runner + ": build k2d_runner with Emscripten into that folder";
            return false;
        }
    if (!fs_create_directory(outDir))
    {
        error = "Could not make " + out;
        return false;
    }
    for (const char* name : runnerFiles)
    {
        ct::Vector<unsigned char> data;
        if (!readFile(join(runner, name).c_str(), data) || !file_write(join(out, name).c_str(), data.data(), data.size()))
        {
            error = ct::String("Could not copy ") + name;
            return false;
        }
    }
    if (!WriteProjectPack(projectRoot, join(out, "data.kpak").c_str(), error))
        return false;

    char* projectText = file_read_text(join(ct::String(projectRoot), "project.k2dproj").c_str());
    const ct::Json project = projectText ? ct::Json::parse(projectText) : ct::Json();
    if (projectText)
        fs_free(projectText);
    const char* name = project["name"].as_cstr("");
    const ct::String page =
        ct::String(kPageHead) + escapeHtml(name[0] ? name : path_filename(projectRoot)) + kPageBody;
    if (!file_write_text(join(out, "index.html").c_str(), page.c_str()))
    {
        error = "Could not write index.html";
        return false;
    }
    log_info("Exported the web game to %s", outDir);
    return true;
}

} // namespace k2d
