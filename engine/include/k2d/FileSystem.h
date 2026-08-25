#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <memory>
#include <vector>

namespace k2d
{
class FileBuffer;
class KPak;

class FileSystem
{
  public:
    static FileSystem& Instance();

    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

    void Init();
    void Shutdown();

    void AddSearchPath(const char* path);
    void ResetSearchPaths();

    // Mounted KPAKs override loose files with the same logical name.
    // This keeps exported builds deterministic while the editor can keep
    // using loose assets by simply not mounting a pack.
    bool MountPack(const char* path, const char* key = "");
    void UnmountPacks();

    bool Resolve(const char* rel, ct::String& out) const;

    bool Exists(const char* path) const;

    bool LoadFile(const char* rel, FileBuffer& buffer, bool nullTerminate = false) const;

    unsigned char* LoadFileData(const char* rel, size_t* outSize, bool nullTerminate = false) const;

    bool SaveFile(const char* path, const void* data, size_t size) const;
    bool SaveTextFile(const char* path, const ct::String& text) const;

    const ct::Vector<ct::String>& SearchPaths() const
    {
        return mSearchPaths;
    }
    const char* BasePath() const
    {
        return mBasePath.c_str();
    }

  private:
    FileSystem() = default;
    ~FileSystem();

    ct::Vector<ct::String> mSearchPaths;
    ct::String mBasePath;
    std::vector<std::unique_ptr<KPak>> mPacks;
};

} // namespace k2d
