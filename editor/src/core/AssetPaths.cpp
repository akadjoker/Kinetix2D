#include "AssetPaths.h"

#include "EditorFileSystem.h"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>

#include <platform.h>

#include <cstring>

namespace k2d::editor
{

namespace
{

const char kAssetsFolder[] = "assets/";
const std::size_t kAssetsFolderLength = sizeof kAssetsFolder - 1;
const char* const kDataExtensions[] = {"k2dscene", "k2dprefab", "prefab", "k2dtileset", "json", nullptr};
const char* const kScriptExtensions[] = {"py", "zen", "zs", nullptr};

ct::String slashes(const ct::String& path)
{
    ct::String out = path;
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i] == '\\')
            out[i] = '/';
    return out;
}

bool startsWith(const ct::String& text, const ct::String& prefix)
{
    return text.size() >= prefix.size() && std::memcmp(text.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool moveTo(const ct::String& path, const ct::String& from, const ct::String& to, bool directory, ct::String& out)
{
    if (from.empty())
        return false;
    if (!directory)
    {
        if (path != from)
            return false;
        out = to;
        return true;
    }
    if (path.size() <= from.size() || !startsWith(path, from) || path[from.size()] != '/')
        return false;
    out = to + path.substr(from.size());
    return true;
}

ct::String underAssets(const ct::String& path)
{
    return startsWith(path, kAssetsFolder) ? path.substr(kAssetsFolderLength) : ct::String();
}

bool hasExtension(const ct::String& path, const char* const* extensions)
{
    const ct::String ext = EditorFileSystem::extension(path);
    for (const char* const* e = extensions; *e; ++e)
        if (ext == *e)
            return true;
    return false;
}

void collect(const ct::String& directory, int depth, const char* const* extensions, ct::Vector<ct::String>& out)
{
    ct::Vector<EditorFileEntry> entries;
    if (depth > 32 || !EditorFileSystem::listDirectory(directory, entries))
        return;
    for (const EditorFileEntry& entry : entries)
    {
        if (entry.name.empty() || entry.name[0] == '.')
            continue;
        if (entry.directory)
        {
            if (depth > 0 || entry.name != "export")
                collect(entry.path, depth + 1, extensions, out);
        }
        else if (hasExtension(entry.path, extensions))
            out.push_back(entry.path);
    }
}

bool readJson(const ct::String& path, ct::Json& out)
{
    FileBuffer file;
    if (!FileSystem::Instance().LoadFile(path.c_str(), file, true))
        return false;
    ct::Json::Error error;
    out = ct::Json::parse(file.Text(), &error);
    return !error;
}

bool quotedIn(const char* text, const ct::String& path, bool directory)
{
    if (path.empty())
        return false;
    for (const char quote : {'"', '\''})
    {
        ct::String needle(1, quote);
        needle += path;
        needle += directory ? '/' : quote;
        if (std::strstr(text, needle.c_str()))
            return true;
    }
    return false;
}

} // namespace

bool isPlainProjectPath(const ct::String& path)
{
    std::size_t start = 0;
    while (start <= path.size())
    {
        std::size_t end = path.find('/', start);
        if (end == ct::String::npos)
            end = path.size();
        const ct::String part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == ".." || part.find('\\') != ct::String::npos)
            return false;
        start = end + 1;
    }
    return true;
}

bool removeAssetTree(const ct::String& path)
{
    if (EditorFileSystem::isDirectory(path) && !EditorFileSystem::isLink(path))
    {
        ct::Vector<EditorFileEntry> entries;
        if (!EditorFileSystem::listDirectory(path, entries))
            return false;
        for (const EditorFileEntry& entry : entries)
            if (entry.name != "." && entry.name != ".." && !removeAssetTree(entry.path))
                return false;
    }
    return fs_remove_path(path.c_str());
}

bool AssetMove::apply(const ct::String& root, const ct::String& path, ct::String& out) const
{
    if (path.empty())
        return false;
    const ct::String written = slashes(path);
    if (EditorFileSystem::isAbsolute(written))
    {
        const ct::String base = slashes(root);
        const ct::String relative = EditorFileSystem::relativeTo(base, written);
        if (relative == written || !moveTo(relative, from, to, directory, out))
            return false;
        out = EditorFileSystem::join(base, out.c_str());
        return true;
    }
    const ct::String relative = startsWith(written, "./") ? written.substr(2) : written;
    if (moveTo(relative, from, to, directory, out))
        return true;
    const ct::String tail = underAssets(from);
    if (tail.empty())
        return false;
    const ct::String toTail = underAssets(to);
    return moveTo(relative, tail, toTail.empty() ? to : toTail, directory, out);
}

int rewritePaths(ct::Json& data, const ct::String& root, const AssetMove& move)
{
    if (data.is_string())
    {
        ct::String moved;
        if (!move.apply(root, ct::String(data.as_cstr()), moved))
            return 0;
        data = ct::Json(moved.c_str());
        return 1;
    }
    int changed = 0;
    if (data.is_array())
        for (ct::Json& item : data.items())
            changed += rewritePaths(item, root, move);
    else if (data.is_object())
        for (ct::Json::Member& member : data.members())
            changed += rewritePaths(member.value, root, move);
    return changed;
}

bool namesAsset(const ct::Json& data, const ct::String& root, const AssetMove& asset)
{
    if (data.is_string())
    {
        ct::String unused;
        return asset.apply(root, ct::String(data.as_cstr()), unused);
    }
    if (data.is_array())
    {
        for (const ct::Json& item : data.items())
            if (namesAsset(item, root, asset))
                return true;
    }
    else if (data.is_object())
    {
        for (const ct::Json::Member& member : data.members())
            if (namesAsset(member.value, root, asset))
                return true;
    }
    return false;
}

void projectDataFiles(const ct::String& root, ct::Vector<ct::String>& out)
{
    collect(root, 0, kDataExtensions, out);
}

void projectScripts(const ct::String& root, ct::Vector<ct::String>& out)
{
    collect(root, 0, kScriptExtensions, out);
}

bool rewriteProjectFiles(const ct::String& root, const AssetMove& move, ct::Vector<ct::String>& changed)
{
    ct::Vector<ct::String> files;
    projectDataFiles(root, files);
    bool written = true;
    for (const ct::String& file : files)
    {
        ct::Json data;
        if (!readJson(file, data) || rewritePaths(data, root, move) == 0)
            continue;
        if (FileSystem::Instance().SaveTextFile(file.c_str(), data.dump(2)))
            changed.push_back(EditorFileSystem::relativeTo(root, file));
        else
            written = false;
    }
    return written;
}

void findReferences(const ct::String& root, const AssetMove& asset, ct::Vector<ct::String>& files,
                    ct::Vector<ct::String>& scripts)
{
    ct::Vector<ct::String> found;
    projectDataFiles(root, found);
    for (const ct::String& file : found)
    {
        ct::Json data;
        if (readJson(file, data) && namesAsset(data, root, asset))
            files.push_back(EditorFileSystem::relativeTo(root, file));
    }
    found.clear();
    projectScripts(root, found);
    const ct::String tail = underAssets(asset.from);
    for (const ct::String& file : found)
    {
        FileBuffer text;
        if (!FileSystem::Instance().LoadFile(file.c_str(), text, true))
            continue;
        if (quotedIn(text.Text(), asset.from, asset.directory) || quotedIn(text.Text(), tail, asset.directory))
            scripts.push_back(EditorFileSystem::relativeTo(root, file));
    }
}

} // namespace k2d::editor
