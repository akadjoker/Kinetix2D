#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
    class FileBuffer;

    class FileSystem
    {
    public:
        static FileSystem &Instance();

        FileSystem(const FileSystem &) = delete;
        FileSystem &operator=(const FileSystem &) = delete;

        void Init();
        void Shutdown();

        void AddSearchPath(const char *path);
        void ResetSearchPaths();

        bool Resolve(const char *rel, ct::String &out) const;

        bool Exists(const char *path) const;

        bool LoadFile(const char *rel, FileBuffer &buffer, bool nullTerminate = false) const;

        unsigned char *LoadFileData(const char *rel, size_t *outSize, bool nullTerminate = false) const;

        const ct::Vector<ct::String> &SearchPaths() const { return mSearchPaths; }
        const char *BasePath() const { return mBasePath.c_str(); }

    private:
        FileSystem() = default;
        ~FileSystem() = default;

        ct::Vector<ct::String> mSearchPaths;
        ct::String mBasePath;
    };

}