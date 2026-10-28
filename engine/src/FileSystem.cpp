#include "k2d/FileSystem.h"

#include <ct/detail/utils.hpp>
#include "k2d/FileBuffer.h"
#include "k2d/KPak.h"

#include <platform.h>
#include <cstdlib>
#include <cstring>

namespace k2d
{

namespace
{
bool HasTrailingSeparator(const ct::String& s)
{
    return !s.empty() && (s[s.size() - 1] == '/' || s[s.size() - 1] == '\\');
}
} // namespace

FileSystem& FileSystem::Instance()
{
    static FileSystem fs;
    return fs;
}

FileSystem::~FileSystem() = default;

void FileSystem::Init()
{
    char base[4096];
    if (fs_get_base_path(base, sizeof base))
        mBasePath = base;
    ResetSearchPaths();
}

void FileSystem::Shutdown()
{
    UnmountPacks();
    mSearchPaths.clear();
    mBasePath.clear();
}

void FileSystem::AddSearchPath(const char* path, bool first)
{
    if (!path || !path[0])
        return;

    ct::String p(path);
    while (HasTrailingSeparator(p))
        p.pop_back();
    if (p.empty())
        return;
    if (first)
        mSearchPaths.insert(mSearchPaths.begin(), p);
    else
        mSearchPaths.push_back(p);
}

void FileSystem::ResetSearchPaths()
{
    mSearchPaths.clear();
    AddSearchPath("assets");
    AddSearchPath("../assets");
    AddSearchPath("../../assets");
}

bool FileSystem::MountPack(const char* path, const char* key)
{
    if (!path || !path[0])
        return false;
    ct::Unique<KPak> pack = ct::make_unique<KPak>();
    if (!pack->Open(path, key ? key : ""))
        return false;
    mPacks.push_back(ct::detail::move(pack));
    return true;
}

void FileSystem::UnmountPacks()
{
    mPacks.clear();
}

bool FileSystem::Exists(const char* path) const
{
    if (!path || !path[0])
        return false;

    for (const ct::Unique<KPak>& pack : mPacks)
        if (pack && pack->Contains(path))
            return true;

    return file_exists(path);
}

bool FileSystem::Resolve(const char* rel, ct::String& out) const
{
    if (!rel || !rel[0])
        return false;

    // 1) An absolute path is only itself.
    if (path_is_absolute(rel))
    {
        if (!Exists(rel))
            return false;
        out = rel;
        return true;
    }

    // 2) Each search root first: a project's "assets/x.png" is its own, not
    //    the one that happens to sit under the current directory.
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

    // 3) The path as given, from the current directory.
    if (Exists(rel))
    {
        out = rel;
        return true;
    }

    // 4) Each search root, resolved against the executable directory.
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

bool FileSystem::LoadFile(const char* rel, FileBuffer& buffer, bool nullTerminate) const
{
    if (rel && rel[0])
    {
        for (const ct::Unique<KPak>& pack : mPacks)
        {
            ct::Vector<unsigned char> data;
            if (pack && pack->Contains(rel))
                return pack->Read(rel, data) && buffer.AssignCopy(data.data(), data.size(), nullTerminate);
        }
    }
    ct::String resolved;
    if (!Resolve(rel, resolved))
        return false;
    return buffer.Load(resolved.c_str(), nullTerminate);
}

unsigned char* FileSystem::LoadFileData(const char* rel, size_t* outSize, bool nullTerminate) const
{
    if (outSize)
        *outSize = 0;

    FileBuffer buffer;
    if (!LoadFile(rel, buffer, nullTerminate))
        return nullptr;

    const size_t size = buffer.Size();
    const size_t allocationSize = size + (nullTerminate ? 1u : 0u);
    unsigned char* data = static_cast<unsigned char*>(std::malloc(allocationSize > 0 ? allocationSize : 1));
    if (!data)
        return nullptr;
    if (size > 0)
        std::memcpy(data, buffer.Data(), size);
    if (nullTerminate)
        data[size] = 0;
    if (outSize)
        *outSize = size;
    return data;
}

bool FileSystem::SaveFile(const char* path, const void* data, size_t size) const
{
    if (!path || !path[0])
        return false;

    return file_write(path, data, size);
}

bool FileSystem::SaveTextFile(const char* path, const ct::String& text) const
{
    return SaveFile(path, text.c_str(), text.size());
}

} // namespace k2d
