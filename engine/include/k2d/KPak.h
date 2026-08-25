#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace k2d
{
// Read-only Kinetix asset package. The index is loaded once while payload
// bytes remain on disk until an asset asks for them.
class KPak
{
  public:
    static constexpr std::uint32_t Version = 1;
    static constexpr std::size_t HeaderSize = 64;

    KPak();
    ~KPak();

    KPak(const KPak&) = delete;
    KPak& operator=(const KPak&) = delete;

    bool Open(const char* path, const char* key = "");
    void Close();
    bool IsOpen() const;
    bool Contains(const char* path) const;
    bool Read(const char* path, std::vector<unsigned char>& out) const;
    std::size_t EntryCount() const;
    const char* EntryName(std::size_t index) const;
    std::uint32_t EntryRawSize(std::size_t index) const;
    std::uint32_t EntryStoredSize(std::size_t index) const;

  private:
    struct Entry
    {
        std::string name;
        std::uint64_t offset = 0;
        std::uint32_t storedSize = 0;
        std::uint32_t rawSize = 0;
        std::uint32_t crc = 0;
        std::uint16_t flags = 0;
    };

    bool ReadDirectory(const char* key);
    bool ReadAt(std::uint64_t offset, void* output, std::size_t size) const;

    std::FILE* mFile;
    std::vector<Entry> mEntries;
    std::unordered_map<std::string, std::size_t> mIndex;
    unsigned char mKey[32];
    bool mEncrypted;
};

// Offline writer used by the packer tool and build tooling.
class KPakWriter
{
  public:
    KPakWriter();

    void SetKey(const char* key);
    void SetCompressionLevel(int level);
    bool AddFile(const char* archivePath, const char* sourcePath);
    bool AddData(const char* archivePath, const void* data, std::size_t size);
    bool Write(const char* path) const;

    std::size_t EntryCount() const;
    std::uint64_t RawSize() const;
    std::uint64_t StoredSize() const;

  private:
    struct Pending
    {
        std::string name;
        std::vector<unsigned char> stored;
        std::uint32_t rawSize = 0;
        std::uint32_t crc = 0;
        std::uint16_t flags = 0;
    };

    std::vector<Pending> mEntries;
    std::unordered_map<std::string, std::size_t> mIndex;
    std::string mKey;
    int mCompressionLevel;
    std::uint64_t mRawSize;
    std::uint64_t mStoredSize;
};
} // namespace k2d
