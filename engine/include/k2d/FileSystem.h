#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
    class FileBuffer;

    // Central asset lookup & loading. Singleton, initialized by Device::Init().
    //
    // Resolves a relative asset name against an ordered list of search roots,
    // so demos can just pass "wabbit_alpha.png" without worrying about whether
    // the executable was launched from ./bin, .. or ../.. .
    //
    // Default search roots:  assets,  ../assets,  ../../assets
    //
    // All file IO is done through SDL (SDL_RWops / SDL_GetBasePath).
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

        // Returns true and fills `out` with the first existing candidate.
        // Order tried:
        //   1. the path exactly as given (absolute or cwd-relative)
        //   2. each search root, resolved against the current directory
        //   3. each search root, resolved against the executable directory
        // Safe to call before Init() (only the as-is case is tried).
        bool Resolve(const char *rel, ct::String &out) const;

        bool Exists(const char *path) const;

        // Loads a resolved asset into `buffer` (SDL-backed).
        bool LoadFile(const char *rel, FileBuffer &buffer, bool nullTerminate = false) const;

        // Loads a resolved asset into a malloc'd buffer; caller must free()
        // the returned pointer. `outSize` receives the byte count.
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
