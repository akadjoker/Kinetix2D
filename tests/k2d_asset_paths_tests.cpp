#include "core/AssetPaths.h"
#include "core/EditorFileSystem.h"
#include "k2d_test_paths.h"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>

#include <cstdio>
#include <cstring>

#if !defined(_WIN32)
#include <unistd.h>
#endif

using k2d::editor::AssetMove;

namespace
{
int gFailures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "  ok  " : "  FAIL", what);
    if (!ok)
        ++gFailures;
}

ct::String moved(const AssetMove& move, const char* path)
{
    ct::String out;
    return move.apply("/game", path, out) ? out : ct::String("-");
}

void testApply()
{
    const AssetMove file{"assets/hero.png", "assets/chars/hero.png", false};
    check(moved(file, "assets/hero.png") == "assets/chars/hero.png", "a file by its project path");
    check(moved(file, "hero.png") == "chars/hero.png", "a file by its path under assets/");
    check(moved(file, "/game/assets/hero.png") == "/game/assets/chars/hero.png", "a file by its absolute path");
    check(moved(file, "assets\\hero.png") == "assets/chars/hero.png", "backslashes");
    check(moved(file, "./assets/hero.png") == "assets/chars/hero.png", "a leading ./");
    check(moved(file, "assets/hero.png.bak") == "-" && moved(file, "assets/hero") == "-", "only the same file");
    check(moved(file, "/other/assets/hero.png") == "-", "an absolute path outside the project");

    const AssetMove folder{"assets/enemies", "assets/foes", true};
    check(moved(folder, "assets/enemies/bat.png") == "assets/foes/bat.png", "a file in a moved folder");
    check(moved(folder, "enemies/a/b.py") == "foes/a/b.py", "deeper, under assets/");
    check(moved(folder, "assets/enemies2/bat.png") == "-", "a folder whose name starts the same");
    check(moved(folder, "enemies") == "-" && moved(folder, "assets/enemies") == "-",
          "the folder's own name (a tag, a group) is not a path");

    const AssetMove out{"assets/a.png", "art/a.png", false};
    check(moved(out, "a.png") == "art/a.png", "out of assets/: the project path, which the root search path finds");
}

void testRewriteJson()
{
    ct::Json::Error error;
    ct::Json scene = ct::Json::parse(
        R"({"root":{"name":"enemies","tag":"enemies","components":[{"type":"Sprite","data":{"texture":"assets/enemies/bat.png"}},)"
        R"({"type":"ZenScript","data":{"path":"enemies/bat.py"}}],"children":[{"texture":"assets/enemies2/x.png"}]}})",
        &error);
    const AssetMove folder{"assets/enemies", "assets/foes", true};
    check(!error && k2d::editor::namesAsset(scene, "/game", folder), "a scene that names the folder");
    check(k2d::editor::rewritePaths(scene, "/game", folder) == 2, "two strings rewritten");
    check(std::strcmp(scene["root"]["components"][0]["data"]["texture"].as_cstr(), "assets/foes/bat.png") == 0 &&
              std::strcmp(scene["root"]["components"][1]["data"]["path"].as_cstr(), "foes/bat.py") == 0,
          "the texture and the script moved");
    check(std::strcmp(scene["root"]["tag"].as_cstr(), "enemies") == 0 &&
              std::strcmp(scene["root"]["children"][0]["texture"].as_cstr(), "assets/enemies2/x.png") == 0,
          "the tag and the other folder left alone");
    check(!k2d::editor::namesAsset(scene, "/game", folder), "nothing names the old folder after");
}

bool write(const ct::String& path, const char* text)
{
    return k2d::FileSystem::Instance().SaveTextFile(path.c_str(), text);
}

ct::String read(const ct::String& path)
{
    k2d::FileBuffer file;
    return k2d::FileSystem::Instance().LoadFile(path.c_str(), file, true) ? ct::String(file.Text()) : ct::String();
}

void testProjectFiles()
{
    using k2d::editor::EditorFileSystem;
    const ct::String root = k2d_tests::tempPath("k2d_asset_paths_project");
    EditorFileSystem::makeDirectory(EditorFileSystem::join(root, "assets/scripts"));
    EditorFileSystem::makeDirectory(EditorFileSystem::join(root, "scenes"));
    EditorFileSystem::makeDirectory(EditorFileSystem::join(root, "export/web"));
    EditorFileSystem::makeDirectory(EditorFileSystem::join(root, ".git"));
    const ct::String scene = EditorFileSystem::join(root, "scenes/level.k2dscene");
    const ct::String other = EditorFileSystem::join(root, "scenes/menu.k2dscene");
    const ct::String exported = EditorFileSystem::join(root, "export/web/level.k2dscene");
    const ct::String hidden = EditorFileSystem::join(root, ".git/level.k2dscene");
    const ct::String script = EditorFileSystem::join(root, "assets/scripts/go.py");
    const char* level = R"({"texture": "assets/hero.png"})";
    bool ok = write(scene, level) && write(other, R"({"texture": "assets/tree.png"})") && write(exported, level) &&
              write(hidden, level) && write(script, "load_texture(\"assets/hero.png\")\nprint('hero.png!')\n");
    check(ok, "project written");

    const AssetMove file{"assets/hero.png", "assets/chars/hero.png", false};
    ct::Vector<ct::String> files;
    ct::Vector<ct::String> scripts;
    k2d::editor::findReferences(root, file, files, scripts);
    check(files.size() == 1 && files[0] == "scenes/level.k2dscene", "the scene that uses it, not export/ or .git/");
    check(scripts.size() == 1 && scripts[0] == "assets/scripts/go.py", "the script that quotes its path");

    ct::Vector<ct::String> changed;
    check(k2d::editor::rewriteProjectFiles(root, file, changed) && changed.size() == 1, "one file rewritten");
    check(std::strstr(read(scene).c_str(), "\"assets/chars/hero.png\"") != nullptr, "the scene names the new place");
    check(read(other) == R"({"texture": "assets/tree.png"})" && read(exported) == level && read(hidden) == level,
          "the others untouched, to the byte");
    check(read(script).size() > 0 && std::strstr(read(script).c_str(), "\"assets/hero.png\"") != nullptr,
          "scripts are reported, not edited");
}

void testPlainPaths()
{
    using k2d::editor::isPlainProjectPath;
    check(isPlainProjectPath("assets/a.png") && isPlainProjectPath("a..b.png"), "plain paths, dots inside a name");
    check(!isPlainProjectPath("") && !isPlainProjectPath("../x") && !isPlainProjectPath("assets/../../x") &&
              !isPlainProjectPath("./a") && !isPlainProjectPath("a//b") && !isPlainProjectPath("a/") &&
              !isPlainProjectPath("a\\b"),
          "empty, ., .., empty parts and backslashes refused");
}

void testRemoveTree()
{
#if !defined(_WIN32)
    using k2d::editor::EditorFileSystem;
    const ct::String root = k2d_tests::tempPath("k2d_asset_paths_remove");
    const ct::String outside = k2d_tests::tempPath("k2d_asset_paths_outside");
    EditorFileSystem::makeDirectory(EditorFileSystem::join(root, "doomed/deep"));
    EditorFileSystem::makeDirectory(outside);
    const ct::String keep = EditorFileSystem::join(outside, "keep.txt");
    bool ok = write(keep, "keep") && write(EditorFileSystem::join(root, "doomed/deep/a.txt"), "a") &&
              write(EditorFileSystem::join(root, "doomed/.hidden"), "h");
    const ct::String inner = EditorFileSystem::join(root, "doomed/link");
    const ct::String top = EditorFileSystem::join(root, "toplink");
    ok = ok && symlink(outside.c_str(), inner.c_str()) == 0 && symlink(outside.c_str(), top.c_str()) == 0;
    check(ok, "a folder with a link out of it");
    check(k2d::editor::removeAssetTree(EditorFileSystem::join(root, "doomed")) &&
              !EditorFileSystem::exists(EditorFileSystem::join(root, "doomed")),
          "the folder gone, hidden files and all");
    check(EditorFileSystem::isLink(top) && k2d::editor::removeAssetTree(top) && !EditorFileSystem::isLink(top),
          "a link to a folder removed as a link");
    check(read(keep) == "keep", "what the links pointed at is untouched");
#endif
}

} // namespace

int main()
{
    std::printf("=== Asset paths ===\n");
    testApply();
    testRewriteJson();
    testProjectFiles();
    testPlainPaths();
    testRemoveTree();
    std::printf("%s\n", gFailures == 0 ? "All passed" : "FAILED");
    return gFailures == 0 ? 0 : 1;
}
