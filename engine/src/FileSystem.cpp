#include "k2d/FileSystem.h"
#include "k2d/FileBuffer.h"

#include <SDL.h>
#include <cstdlib>

namespace k2d
{

    namespace
    {
        bool HasTrailingSeparator(const ct::String &s)
        {
            return !s.empty() && (s[s.size() - 1] == '/' || s[s.size() - 1] == '\\');
        }
    }

    FileSystem &FileSystem::Instance()
    {
        static FileSystem fs;
        return fs;
    }

    void FileSystem::Init()
    {
        char *base = SDL_GetBasePath();
        if (base)
        {
            mBasePath = base;
            SDL_free(base);
        }
        ResetSearchPaths();
    }

    void FileSystem::Shutdown()
    {
        mSearchPaths.clear();
        mBasePath.clear();
    }

    void FileSystem::AddSearchPath(const char *path)
    {
        if (!path || !path[0])
            return;

        ct::String p(path);
        while (HasTrailingSeparator(p))
            p.pop_back();
        if (!p.empty())
            mSearchPaths.push_back(p);
    }

    void FileSystem::ResetSearchPaths()
    {
        mSearchPaths.clear();
        AddSearchPath("assets");
        AddSearchPath("../assets");
        AddSearchPath("../../assets");
    }

    bool FileSystem::Exists(const char *path) const
    {
        if (!path || !path[0])
            return false;

        SDL_RWops *rw = SDL_RWFromFile(path, "rb");
        if (!rw)
            return false;
        SDL_RWclose(rw);
        return true;
    }

    bool FileSystem::Resolve(const char *rel, ct::String &out) const
    {
        if (!rel || !rel[0])
            return false;

        // 1) The path exactly as given (absolute or cwd-relative).
        if (Exists(rel))
        {
            out = rel;
            return true;
        }

        // 2) Each search root, resolved against the current directory.
        for (size_t i = 0; i < mSearchPaths.size(); ++i)
        {
            ct::String candidate = mSearchPaths[i];
            candidate += "/";
            candidate += rel;
            if (Exists(candidate.c_str()))
            {
                out = candidate;
                return true;
            }
        }

        // 3) Each search root, resolved against the executable directory.
        //    SDL_GetBasePath() already ends with a separator.
        if (!mBasePath.empty())
        {
            for (size_t i = 0; i < mSearchPaths.size(); ++i)
            {
                ct::String candidate = mBasePath;
                candidate += mSearchPaths[i];
                candidate += "/";
                candidate += rel;
                if (Exists(candidate.c_str()))
                {
                    out = candidate;
                    return true;
                }
            }
        }

        return false;
    }

    bool FileSystem::LoadFile(const char *rel, FileBuffer &buffer, bool nullTerminate) const
    {
        ct::String resolved;
        if (!Resolve(rel, resolved))
            return false;
        return buffer.Load(resolved.c_str(), nullTerminate);
    }

    unsigned char *FileSystem::LoadFileData(const char *rel, size_t *outSize, bool nullTerminate) const
    {
        if (outSize)
            *outSize = 0;

        ct::String resolved;
        if (!Resolve(rel, resolved))
            return nullptr;

        SDL_RWops *rw = SDL_RWFromFile(resolved.c_str(), "rb");
        if (!rw)
            return nullptr;

        Sint64 size64 = SDL_RWsize(rw);
        if (size64 < 0)
        {
            SDL_RWclose(rw);
            return nullptr;
        }

        size_t size = static_cast<size_t>(size64);
        size_t allocSize = nullTerminate ? size + 1 : size;
        unsigned char *data = static_cast<unsigned char *>(std::malloc(allocSize > 0 ? allocSize : 1));
        if (!data)
        {
            SDL_RWclose(rw);
            return nullptr;
        }

        size_t readTotal = 0;
        while (readTotal < size)
        {
            size_t got = SDL_RWread(rw, data + readTotal, 1, size - readTotal);
            if (got == 0)
                break;
            readTotal += got;
        }
        SDL_RWclose(rw);

        if (readTotal != size)
        {
            std::free(data);
            return nullptr;
        }

        if (nullTerminate)
            data[size] = 0;
        if (outSize)
            *outSize = size;
        return data;
    }

}
