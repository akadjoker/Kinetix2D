#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d::editor
{

struct EditorFileEntry
{
    ct::String name;
    ct::String path;
    bool directory = false;
};

class EditorFileSystem
{
public:
    static ct::String currentDirectory();
    static ct::String parentPath(const ct::String &path);
    static bool listDirectory(const ct::String &path, ct::Vector<EditorFileEntry> &entries);

    static ct::String fileName(const ct::String &path);
    static ct::String extension(const ct::String &path);
    static ct::String withoutExtension(const ct::String &path);

    static bool exists(const ct::String &path);
    static bool isDirectory(const ct::String &path);
    static bool makeDirectory(const ct::String &path);

    static ct::String join(const ct::String &left, const char *right);

    static bool isAbsolute(const ct::String &path);
    // Asset references are stored relative to the project root so a project
    // opens on a machine whose checkout lives somewhere else. Returns path
    // unchanged when it is not under root.
    static ct::String relativeTo(const ct::String &root, const ct::String &path);
    static ct::String resolve(const ct::String &root, const ct::String &path);

private:
    static bool makeDirectorySingle(const ct::String &path);
};

}
