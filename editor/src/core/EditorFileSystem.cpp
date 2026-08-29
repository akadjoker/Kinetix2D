#include "EditorFileSystem.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace k2d::editor
{

ct::String EditorFileSystem::join(const ct::String &left, const char *right)
{
    ct::String result = left;
    if (!result.empty() && result[result.size() - 1] != '/' && result[result.size() - 1] != '\\')
        result += "/";
    result += right;
    return result;
}

bool EditorFileSystem::isAbsolute(const ct::String &path)
{
    if (path.empty())
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return true;
    return path.size() >= 2 && path[1] == ':';
}

ct::String EditorFileSystem::relativeTo(const ct::String &root, const ct::String &path)
{
    if (root.empty() || path.empty() || !isAbsolute(path))
        return path;

    ct::String base = root;
    while (!base.empty() && (base[base.size() - 1] == '/' || base[base.size() - 1] == '\\'))
        base.pop_back();
    if (base.empty() || path.size() <= base.size())
        return path;

    for (size_t i = 0; i < base.size(); ++i)
    {
        const char a = path[i] == '\\' ? '/' : path[i];
        const char b = base[i] == '\\' ? '/' : base[i];
        if (a != b)
            return path;
    }
    if (path[base.size()] != '/' && path[base.size()] != '\\')
        return path;

    ct::String result;
    for (size_t i = base.size() + 1; i < path.size(); ++i)
        result.push_back(path[i] == '\\' ? '/' : path[i]);
    return result.empty() ? path : result;
}

ct::String EditorFileSystem::resolve(const ct::String &root, const ct::String &path)
{
    if (path.empty() || isAbsolute(path) || root.empty())
        return path;
    return join(root, path.c_str());
}

ct::String EditorFileSystem::currentDirectory()
{
#if defined(_WIN32)
    char path[MAX_PATH];
    const DWORD length = GetCurrentDirectoryA(MAX_PATH, path);
    return length > 0 && length < MAX_PATH ? ct::String(path, length) : ct::String(".");
#else
    char path[PATH_MAX];
    return getcwd(path, sizeof(path)) ? ct::String(path) : ct::String(".");
#endif
}

ct::String EditorFileSystem::parentPath(const ct::String &path)
{
    if (path.empty())
        return ct::String(".");
    size_t end = path.size();
    while (end > 1 && (path[end - 1] == '/' || path[end - 1] == '\\'))
        --end;
    size_t separator = end;
    while (separator > 0 && path[separator - 1] != '/' && path[separator - 1] != '\\')
        --separator;
    if (separator == 0)
        return ct::String(".");
    if (separator == 1)
        return path.substr(0, 1);
    return path.substr(0, separator - 1);
}

bool EditorFileSystem::listDirectory(const ct::String &path, ct::Vector<EditorFileEntry> &entries)
{
    entries.clear();
#if defined(_WIN32)
    const ct::String pattern = join(path, "*");
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    do
    {
        const char *name = data.cFileName;
        if ((name[0] == '.' && name[1] == '\0') ||
            (name[0] == '.' && name[1] == '.' && name[2] == '\0'))
            continue;
        EditorFileEntry entry;
        entry.name = name;
        entry.path = join(path, name);
        entry.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entries.push_back(ct::detail::move(entry));
    } while (FindNextFileA(handle, &data));
    FindClose(handle);
#else
    DIR *directory = opendir(path.c_str());
    if (!directory)
        return false;
    while (dirent *item = readdir(directory))
    {
        const char *name = item->d_name;
        if ((name[0] == '.' && name[1] == '\0') ||
            (name[0] == '.' && name[1] == '.' && name[2] == '\0'))
            continue;
        EditorFileEntry entry;
        entry.name = name;
        entry.path = join(path, name);
        struct stat info;
        entry.directory = stat(entry.path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
        entries.push_back(ct::detail::move(entry));
    }
    closedir(directory);
#endif

    for (size_t i = 0; i < entries.size(); ++i)
    {
        for (size_t j = i + 1; j < entries.size(); ++j)
        {
            const bool swap = entries[j].directory != entries[i].directory
                ? entries[j].directory
                : entries[j].name < entries[i].name;
            if (swap)
            {
                EditorFileEntry temporary = ct::detail::move(entries[i]);
                entries[i] = ct::detail::move(entries[j]);
                entries[j] = ct::detail::move(temporary);
            }
        }
    }
    return true;
}

ct::String EditorFileSystem::fileName(const ct::String &path)
{
    size_t end = path.size();
    while (end > 0 && (path[end - 1] == '/' || path[end - 1] == '\\'))
        --end;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\')
        --start;
    return path.substr(start, end - start);
}

ct::String EditorFileSystem::extension(const ct::String &path)
{
    const ct::String name = fileName(path);
    size_t dot = name.size();
    while (dot > 0 && name[dot - 1] != '.')
        --dot;
    if (dot == 0 || dot == name.size())
        return ct::String();
    ct::String result = name.substr(dot, name.size() - dot);
    for (size_t i = 0; i < result.size(); ++i)
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = static_cast<char>(result[i] - 'A' + 'a');
    return result;
}

ct::String EditorFileSystem::withoutExtension(const ct::String &path)
{
    size_t dot = path.size();
    while (dot > 0 && path[dot - 1] != '.' && path[dot - 1] != '/' && path[dot - 1] != '\\')
        --dot;
    if (dot == 0 || path[dot - 1] != '.')
        return path;
    return path.substr(0, dot - 1);
}

bool EditorFileSystem::exists(const ct::String &path)
{
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0;
#endif
}

bool EditorFileSystem::isDirectory(const ct::String &path)
{
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool EditorFileSystem::makeDirectorySingle(const ct::String &path)
{
    if (path.empty() || isDirectory(path))
        return true;
#if defined(_WIN32)
    return CreateDirectoryA(path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0775) == 0 || errno == EEXIST;
#endif
}

bool EditorFileSystem::makeDirectory(const ct::String &path)
{
    if (path.empty())
        return false;

    ct::String accumulated;
    size_t start = 0;
#if !defined(_WIN32)
    if (path[0] == '/')
    {
        accumulated = "/";
        start = 1;
    }
#endif
    for (size_t i = start; i <= path.size(); ++i)
    {
        if (i == path.size() || path[i] == '/' || path[i] == '\\')
        {
            if (i > start)
            {
                const ct::String segment = path.substr(start, i - start);
                accumulated = accumulated.empty() ? segment : join(accumulated, segment.c_str());
                if (!makeDirectorySingle(accumulated))
                    return false;
            }
            start = i + 1;
        }
    }
    return isDirectory(path);
}

}
