#include "ProjectExport.h"

#include <k2d/KPak.h>
#include <k2d/ZenRuntime.h>

#include <ct/json.hpp>
#include <ct/vector.hpp>

#include <platform.h>

#include <miniz.h>

#include <cstdlib>
#include <cstring>

namespace k2d
{

namespace
{

const char* const kPackedExtensions[] = {".png",  ".jpg",  ".jpeg", ".bmp",  ".tga",       ".gif",       ".webp",
                                         ".wav",  ".ogg",  ".mp3",  ".flac", ".ttf",       ".otf",       ".fnt",
                                         ".json", ".txt",  ".csv",  ".xml",  ".tmx",       ".tsx",       ".atlas",
                                         ".glsl", ".vert", ".frag", ".k2dproj", ".k2dscene", ".k2dprefab",
                                         ".k2dtileset"};
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

ct::Json readProject(const char* projectRoot)
{
    char* text = file_read_text(join(ct::String(projectRoot), "project.k2dproj").c_str());
    const ct::Json project = text ? ct::Json::parse(text) : ct::Json();
    if (text)
        fs_free(text);
    return project;
}

ct::String gameTitle(const ct::Json& project, const char* projectRoot)
{
    const char* named = project["name"].as_cstr("");
    return named[0] ? ct::String(named) : ct::String(path_filename(projectRoot));
}

// A file name every system takes.
ct::String plainName(const ct::String& source)
{
    ct::String name;
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        const char c = source[i];
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                           c == '_';
        name += plain ? c : '_';
    }
    return name.empty() ? ct::String("game") : name;
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

void makeRelative(ct::Json& node, const ct::String& root)
{
    if (node.is_object())
    {
        for (auto& member : node.members())
            makeRelative(member.value, root);
    }
    else if (node.is_array())
    {
        for (std::size_t i = 0; i < node.size(); ++i)
            makeRelative(node[i], root);
    }
    else if (node.is_string())
    {
        const ct::String value = slashes(node.as_cstr(""));
        if (startsWith(value, root + "/"))
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

void MakePathsRelative(ct::Json& document, const char* projectRoot)
{
    char absolute[4096];
    if (!projectRoot || !projectRoot[0] || !path_absolute(absolute, sizeof absolute, projectRoot))
        return;
    ct::String root = slashes(absolute);
    while (root.size() > 1 && root[root.size() - 1] == '/')
        root = root.substr(0, root.size() - 1);
    makeRelative(document, root);
}

int FixPathsInFiles(const char* root)
{
    char absolute[4096];
    if (!root || !path_absolute(absolute, sizeof absolute, root))
        return -1;
    Collector collector;
    collector.root = slashes(absolute);
    while (collector.root.size() > 1 && collector.root[collector.root.size() - 1] == '/')
        collector.root = collector.root.substr(0, collector.root.size() - 1);
    if (!fs_enumerate_directory(collector.root.c_str(), true, collect, &collector))
        return -1;
    int changed = 0;
    for (const ct::String& relative : collector.files)
    {
        if (!path_has_extension(relative.c_str(), ".k2dscene") && !path_has_extension(relative.c_str(), ".k2dprefab") &&
            !path_has_extension(relative.c_str(), ".k2dtileset"))
            continue;
        const ct::String path = join(collector.root, relative.c_str());
        char* text = file_read_text(path.c_str());
        if (!text)
            continue;
        ct::Json::Error parseError;
        ct::Json document = ct::Json::parse(text, &parseError);
        const ct::String before = parseError ? ct::String() : document.dump(2);
        fs_free(text);
        if (parseError)
            continue;
        makeRelative(document, collector.root);
        const ct::String after = document.dump(2);
        if (after != before && file_write_text(path.c_str(), after.c_str()))
        {
            log_info("Paths made relative in %s", relative.c_str());
            ++changed;
        }
    }
    return changed;
}

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
        if (path_has_extension(path.c_str(), ".k2dscene") || path_has_extension(path.c_str(), ".k2dprefab") ||
            path_has_extension(path.c_str(), ".k2dtileset"))
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
            makeRelative(document, collector.root);
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

namespace k2d
{

bool ExportDesktop(const char* projectRoot, const char* runnerPath, const char* outDir, ct::String& gamePath,
                   ct::String& error)
{
    ct::Vector<unsigned char> runner;
    if (!readFile(runnerPath, runner))
    {
        error = ct::String("No runner at ") + runnerPath + ": build k2d_runner";
        return false;
    }
    ct::String name = plainName(gameTitle(readProject(projectRoot), projectRoot));
    if (path_has_extension(runnerPath, ".exe"))
        name += ".exe";

    if (!fs_create_directory(outDir))
    {
        error = ct::String("Could not make ") + outDir;
        return false;
    }
    gamePath = join(ct::String(outDir), name.c_str());
    if (!file_write(gamePath.c_str(), runner.data(), runner.size()) || !fs_make_executable(gamePath.c_str()))
    {
        error = "Could not write " + gamePath;
        return false;
    }
    if (!WriteProjectPack(projectRoot, join(ct::String(outDir), "data.kpak").c_str(), error))
        return false;
    log_info("Exported the desktop game to %s", gamePath.c_str());
    return true;
}

namespace
{

#if defined(_WIN32)
const char* const kExe = ".exe";
#else
const char* const kExe = "";
#endif

// Runs a program and waits for it; on Windows a .bat goes through cmd.
bool runTool(const ct::String& program, const ct::Vector<ct::String>& args, ct::String& error)
{
    ct::Vector<const char*> argv;
#if defined(_WIN32)
    const bool batch = path_has_extension(program.c_str(), ".bat");
    if (batch)
    {
        argv.push_back("/c");
        argv.push_back(program.c_str());
    }
    const char* path = batch ? "cmd.exe" : program.c_str();
#else
    const char* path = program.c_str();
#endif
    for (const ct::String& arg : args)
        argv.push_back(arg.c_str());
    const int64_t id = process_spawn(path, argv.data(), static_cast<int>(argv.size()), false);
    if (id <= 0)
    {
        error = "Could not run " + program + ": " + platform_get_error();
        return false;
    }
    while (process_is_running(id))
        time_sleep(20);
    const int code = process_exit_code(id);
    if (code != 0)
    {
        char text[32];
        std::snprintf(text, sizeof text, " (exit code %d)", code);
        error = ct::String(path_filename(program.c_str())) + " failed" + text;
        return false;
    }
    return true;
}

// 35.0.1 after 9.0.0: the numbers in the names compared one by one.
bool newerVersion(const char* a, const char* b)
{
    while (*a || *b)
    {
        char* endA = nullptr;
        char* endB = nullptr;
        const long numberA = std::strtol(a, &endA, 10);
        const long numberB = std::strtol(b, &endB, 10);
        if (numberA != numberB)
            return numberA > numberB;
        if (endA == a && endB == b)
            return false;
        a = *endA ? endA + 1 : endA;
        b = *endB ? endB + 1 : endB;
    }
    return false;
}

struct NewestFolder
{
    const char* prefix;
    const char* needs;
    ct::String best;
};

bool considerFolder(const char* path, PathType type, void* user)
{
    NewestFolder& search = *static_cast<NewestFolder*>(user);
    const char* name = path_filename(path);
    const std::size_t prefix = std::strlen(search.prefix);
    if (type != PATH_TYPE_DIRECTORY || std::strncmp(name, search.prefix, prefix) != 0)
        return true;
    if (search.needs[0] && !file_exists(join(ct::String(path), search.needs).c_str()))
        return true;
    if (search.best.empty() || newerVersion(name + prefix, path_filename(search.best.c_str()) + prefix))
        search.best = path;
    return true;
}

// The newest folder of directory whose name starts with prefix and that holds needs.
ct::String newestFolder(const ct::String& directory, const char* prefix, const char* needs)
{
    NewestFolder search{prefix, needs, ct::String()};
    fs_enumerate_directory(directory.c_str(), false, considerFolder, &search);
    return search.best;
}

ct::String homeDirectory()
{
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? ct::String(home) : ct::String();
}

// The package name Android wants: lowercase words of letters and digits, none starting with a digit.
ct::String packageName(const ct::String& title)
{
    ct::String id;
    for (std::size_t i = 0; i < title.size(); ++i)
    {
        const char c = title[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9' && !id.empty()))
            id += c;
        else if (c >= 'A' && c <= 'Z')
            id += static_cast<char>(c - 'A' + 'a');
    }
    return "org.kinetix2d." + (id.empty() ? ct::String("game") : id);
}

ct::String xmlText(const ct::String& text)
{
    ct::String out;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '&')
            out += "&amp;";
        else if (c == '<')
            out += "&lt;";
        else if (c == '"')
            out += "&quot;";
        else
            out += c;
    }
    return out;
}

} // namespace

ct::String FindAndroidSdk()
{
    const ct::String home = homeDirectory();
    const ct::String candidates[] = {
        std::getenv("ANDROID_HOME") ? ct::String(std::getenv("ANDROID_HOME")) : ct::String(),
        std::getenv("ANDROID_SDK_ROOT") ? ct::String(std::getenv("ANDROID_SDK_ROOT")) : ct::String(),
        home.empty() ? ct::String() : join(home, "Android/Sdk"),
        home.empty() ? ct::String() : join(home, "android/android-sdk"),
    };
    for (const ct::String& sdk : candidates)
    {
        PathInfo info;
        if (!sdk.empty() && fs_get_path_info(join(sdk, "build-tools").c_str(), &info) &&
            info.type == PATH_TYPE_DIRECTORY)
            return sdk;
    }
    return ct::String();
}

ct::String AndroidPackage(const char* projectRoot)
{
    return packageName(gameTitle(readProject(projectRoot), projectRoot));
}

bool ExportAndroid(const char* projectRoot, const char* runnerLib, const char* sdkDir, const char* outDir,
                   ct::String& apkPath, ct::String& error)
{
    ct::Vector<unsigned char> runner;
    if (!readFile(runnerLib, runner))
    {
        error = ct::String("No Android runner at ") + runnerLib + ": build it with tools/android/build-apk.sh";
        return false;
    }
    const ct::String sdk(sdkDir ? sdkDir : "");
    const ct::String aapt2Name = ct::String("aapt2") + kExe;
    const ct::String tools = sdk.empty() ? ct::String() : newestFolder(join(sdk, "build-tools"), "", aapt2Name.c_str());
    const ct::String platform = sdk.empty() ? ct::String() : newestFolder(join(sdk, "platforms"), "android-", "android.jar");
    if (tools.empty() || platform.empty())
    {
        error = "No Android SDK with build-tools and a platform: set ANDROID_HOME";
        return false;
    }
#if defined(_WIN32)
    const ct::String apksigner = join(tools, "apksigner.bat");
#else
    const ct::String apksigner = join(tools, "apksigner");
#endif
    const ct::String zipalign = join(tools, (ct::String("zipalign") + kExe).c_str());
    const ct::String target(path_filename(platform.c_str()) + std::strlen("android-"));

    const ct::Json project = readProject(projectRoot);
    const ct::String title = gameTitle(project, projectRoot);
    const ct::String name = plainName(title);
    const ct::String package = packageName(title);
    const ct::Json& display = project["display"];
    const bool portrait = display["height"].as_int(720) > display["width"].as_int(1280);

    const ct::String out(outDir);
    const ct::String stage = join(out, "stage");
    const ct::String assets = join(stage, "assets");
    if (!fs_create_directory(assets.c_str()))
    {
        error = "Could not make " + assets;
        return false;
    }
    if (!WriteProjectPack(projectRoot, join(assets, "data.kpak").c_str(), error))
        return false;

    const ct::String manifest = join(stage, "AndroidManifest.xml");
    const ct::String xml =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
        "    package=\"" + package + "\" android:versionCode=\"1\" android:versionName=\"1.0\">\n"
        "    <uses-feature android:glEsVersion=\"0x00030000\" android:required=\"true\" />\n"
        "    <uses-feature android:name=\"android.hardware.touchscreen\" android:required=\"false\" />\n"
        "    <uses-feature android:name=\"android.hardware.gamepad\" android:required=\"false\" />\n"
        "    <application android:label=\"" + xmlText(title) + "\" android:hasCode=\"false\" android:extractNativeLibs=\"true\"\n"
        "        android:theme=\"@android:style/Theme.NoTitleBar.Fullscreen\">\n"
        "        <activity android:name=\"android.app.NativeActivity\" android:exported=\"true\"\n"
        "            android:screenOrientation=\"" + (portrait ? "sensorPortrait" : "sensorLandscape") + "\" android:launchMode=\"singleTask\"\n"
        "            android:configChanges=\"orientation|screenSize|screenLayout|smallestScreenSize|keyboard|keyboardHidden|navigation|uiMode\">\n"
        "            <meta-data android:name=\"android.app.lib_name\" android:value=\"k2d_runner\" />\n"
        "            <intent-filter>\n"
        "                <action android:name=\"android.intent.action.MAIN\" />\n"
        "                <category android:name=\"android.intent.category.LAUNCHER\" />\n"
        "            </intent-filter>\n"
        "        </activity>\n"
        "    </application>\n"
        "</manifest>\n";
    if (!file_write(manifest.c_str(), xml.c_str(), xml.size()))
    {
        error = "Could not write " + manifest;
        return false;
    }

    // The pack is compressed already: stored as it is, so the runner reads it without inflating.
    const ct::String unsignedApk = join(out, "unsigned.apk");
    const ct::String alignedApk = join(out, "aligned.apk");
    if (!runTool(join(tools, aapt2Name.c_str()),
                 {"link", "-o", unsignedApk, "--manifest", manifest, "-I", join(platform, "android.jar"), "-A", assets,
                  "-0", "kpak", "--min-sdk-version", "26", "--target-sdk-version", target},
                 error))
        return false;

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    const bool added = mz_zip_reader_init_file(&zip, unsignedApk.c_str(), 0) &&
                       mz_zip_writer_init_from_reader(&zip, unsignedApk.c_str()) &&
                       mz_zip_writer_add_mem(&zip, "lib/arm64-v8a/libk2d_runner.so", runner.data(), runner.size(),
                                             MZ_NO_COMPRESSION) &&
                       mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    if (!added)
    {
        error = "Could not add the runner to " + unsignedApk;
        return false;
    }
    if (!runTool(zipalign, {"-p", "-f", "4", unsignedApk, alignedApk}, error))
        return false;

    const ct::String keystore = join(homeDirectory(), ".android/debug.keystore");
    if (!file_exists(keystore.c_str()))
    {
        fs_create_directory(join(homeDirectory(), ".android").c_str());
        if (!runTool("keytool", {"-genkeypair", "-keystore", keystore, "-storepass", "android", "-keypass", "android",
                                 "-alias", "androiddebugkey", "-keyalg", "RSA", "-keysize", "2048", "-validity",
                                 "10000", "-dname", "CN=Android Debug,O=Android,C=US"},
                     error))
            return false;
    }
    apkPath = join(out, (name + ".apk").c_str());
    if (!runTool(apksigner, {"sign", "--ks", keystore, "--ks-pass", "pass:android", "--key-pass", "pass:android",
                             "--out", apkPath, alignedApk},
                 error))
        return false;
    fs_remove_path(unsignedApk.c_str());
    fs_remove_path(alignedApk.c_str());
    fs_remove_path((apkPath + ".idsig").c_str());
    log_info("Exported the Android game to %s (%s)", apkPath.c_str(), package.c_str());
    return true;
}

} // namespace k2d
