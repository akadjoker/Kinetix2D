#include "EditorWindow.h"

#include "core/AssetPaths.h"
#include "core/EditorFileSystem.h"

#include <k2d/FileSystem.h>

#include <platform.h>

#include <cstdio>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

const char* const kProjectFile = "project.k2dproj";
const char* const kSidecars[] = {".sprite.json", ".atlas.json", nullptr};

ct::String listed(const ct::Vector<ct::String>& names, std::size_t most)
{
    ct::String text;
    for (std::size_t i = 0; i < names.size() && i < most; ++i)
    {
        if (i)
            text += ", ";
        text += names[i];
    }
    if (names.size() > most)
    {
        char more[32];
        std::snprintf(more, sizeof more, " and %d more", static_cast<int>(names.size() - most));
        text += more;
    }
    return text;
}

// A link is removed, never followed: what it points at may be outside the
// project.
} // namespace

bool EditorWindow::moveAsset(const ct::String& from, const ct::String& to)
{
    if (mPlaying)
    {
        log_warn("Stop the game before moving files");
        return false;
    }
    const ct::String source = EditorFileSystem::resolve(mProjectRoot, from);
    const ct::String target = EditorFileSystem::resolve(mProjectRoot, to);
    if (from == kProjectFile)
    {
        log_error("%s stays at the project's root", kProjectFile);
        return false;
    }
    if (!isPlainProjectPath(from) || !isPlainProjectPath(to) || from == to ||
        EditorFileSystem::relativeTo(mProjectRoot, source) == source || !EditorFileSystem::exists(source))
    {
        log_error("Could not move %s to %s: not two paths in the project", from.c_str(), to.c_str());
        return false;
    }
    if (EditorFileSystem::exists(target))
    {
        log_error("Could not move %s: %s is there already", from.c_str(), to.c_str());
        return false;
    }
    const AssetMove move{from, to, EditorFileSystem::isDirectory(source)};
    if (move.directory && to.size() > from.size() && to.substr(0, from.size() + 1) == from + "/")
    {
        log_error("A folder cannot go inside itself");
        return false;
    }
    ct::Vector<ct::String> files;
    ct::Vector<ct::String> scripts;
    findReferences(mProjectRoot, move, files, scripts);

    EditorFileSystem::makeDirectory(EditorFileSystem::parentPath(target));
    if (!fs_rename_path(source.c_str(), target.c_str()))
    {
        log_error("Could not move %s to %s: %s", from.c_str(), to.c_str(), platform_get_error());
        return false;
    }
    ct::Vector<AssetMove> moves;
    moves.push_back(move);
    if (!move.directory)
        for (const char* const* sidecar = kSidecars; *sidecar; ++sidecar)
        {
            const AssetMove beside{EditorFileSystem::withoutExtension(from) + *sidecar,
                                   EditorFileSystem::withoutExtension(to) + *sidecar, false};
            const ct::String besideSource = EditorFileSystem::resolve(mProjectRoot, beside.from);
            const ct::String besideTarget = EditorFileSystem::resolve(mProjectRoot, beside.to);
            if (beside.from != from && EditorFileSystem::exists(besideSource) &&
                !EditorFileSystem::exists(besideTarget) && fs_rename_path(besideSource.c_str(), besideTarget.c_str()))
                moves.push_back(beside);
        }
    for (const AssetMove& each : moves)
        followAssetMove(each);

    log_info("Moved %s to %s", from.c_str(), to.c_str());
    ct::String moved;
    for (ct::String& file : files)
        if (move.apply(mProjectRoot, file, moved))
            file = moved;
    for (ct::String& script : scripts)
        if (move.apply(mProjectRoot, script, moved))
            script = moved;
    if (!files.empty())
        log_info("Now naming %s: %s", to.c_str(), listed(files, 8).c_str());
    if (!scripts.empty())
        log_warn("%s still quote%s %s: change it there by hand", listed(scripts, 8).c_str(),
                 scripts.size() == 1 ? "s" : "", from.c_str());
    mAssetsPanel->refresh();
    return true;
}

void EditorWindow::followAssetMove(const AssetMove& move)
{
    const ct::String root = mProjectRoot;
    mAssets.RenameKeys([&](const ct::String& key, ct::String& renamed) { return move.apply(root, key, renamed); });
    ct::Vector<ct::String> changed;
    if (!rewriteProjectFiles(root, move, changed))
        log_error("Some files naming %s could not be written", move.from.c_str());
    if (rewritePaths(mProjectDocument, root, move) > 0)
        FileSystem::Instance().SaveTextFile(EditorFileSystem::join(root, kProjectFile).c_str(),
                                            mProjectDocument.dump(2));

    auto follow = [&](ct::Json& state) { return rewritePaths(state, root, move) > 0; };
    mCommands.rewrite(follow);
    rewritePaths(mSceneDocument, root, move);
    ct::String path;
    if (move.apply(root, mScenePath, path))
        mScenePath = path;
    PrefabMode& prefab = mPrefabMode;
    if (prefab.active)
    {
        prefab.commands.rewrite(follow);
        rewritePaths(prefab.scene, root, move);
        rewritePaths(prefab.sceneDocument, root, move);
        if (move.apply(root, prefab.scenePath, path))
            prefab.scenePath = path;
        if (move.apply(root, prefab.path, path))
            prefab.path = path;
    }
    ct::Json state = snapshot();
    if (rewritePaths(state, root, move) > 0)
        restore(state);
    markDirty(mDirty);

    mScriptEditor->assetMoved(move);
    mSpriteEditor->assetMoved(move);
    mImageEditor->assetMoved(move);
}

void EditorWindow::requestDeleteAsset(const ct::String& path)
{
    if (mPlaying)
    {
        log_warn("Stop the game before deleting files");
        return;
    }
    const ct::String absolute = EditorFileSystem::resolve(mProjectRoot, path);
    if (!isPlainProjectPath(path) || path == kProjectFile || EditorFileSystem::relativeTo(mProjectRoot, absolute) == absolute ||
        !EditorFileSystem::exists(absolute))
        return;
    const AssetMove asset{path, ct::String(), EditorFileSystem::isDirectory(absolute)};
    ct::String unused;
    if (asset.apply(mProjectRoot, mScenePath, unused) ||
        (mPrefabMode.active && (asset.apply(mProjectRoot, mPrefabMode.scenePath, unused) ||
                                asset.apply(mProjectRoot, mPrefabMode.path, unused))))
    {
        log_error("%s is open: open another scene before deleting it", path.c_str());
        return;
    }
    ct::Vector<ct::String> doomed;
    doomed.push_back(path);
    if (!asset.directory)
        for (const char* const* sidecar = kSidecars; *sidecar; ++sidecar)
        {
            const ct::String beside = EditorFileSystem::withoutExtension(path) + *sidecar;
            if (beside != path && EditorFileSystem::exists(EditorFileSystem::resolve(mProjectRoot, beside)))
                doomed.push_back(beside);
        }
    ct::Vector<ct::String> files;
    ct::Vector<ct::String> scripts;
    findReferences(mProjectRoot, asset, files, scripts);
    for (const ct::String& script : scripts)
        files.push_back(script);

    ct::String message = ct::String("Delete ") + listed(doomed, 4) +
                         (asset.directory ? " and everything in it?" : "?") + " This cannot be undone.";
    if (!files.empty())
        message += ct::String("\n\nStill used by ") + listed(files, 6) + ".";
    auto* ask = mApp->addFloat<rt::ChoiceDialog>("Delete", message);
    const int remove = ask->addChoice("Delete", rt::IconId::Trash, rt::ChoiceDialog::Role::Destructive);
    ask->addChoice("Cancel", rt::IconId::Cross, rt::ChoiceDialog::Role::Cancel);
    ask->chosen.connect([this, remove, asset, doomed](int choice)
    {
        if (choice != remove)
            return;
        for (const ct::String& each : doomed)
        {
            if (!removeAssetTree(EditorFileSystem::resolve(mProjectRoot, each)))
                log_error("Could not delete all of %s: %s", each.c_str(), platform_get_error());
            else
                log_info("Deleted %s", each.c_str());
            mScriptEditor->assetDeleted(AssetMove{each, ct::String(), asset.directory && each == asset.from});
        }
        mAssetsPanel->refresh();
    });
}

} // namespace k2d::editor
