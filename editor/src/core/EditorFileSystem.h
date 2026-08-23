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

private:
    static bool makeDirectorySingle(const ct::String &path);
};

}
