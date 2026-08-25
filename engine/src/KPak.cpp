#include "k2d/KPak.h"

#include <miniz.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <random>

namespace k2d
{
namespace
{
constexpr unsigned char Magic[] = {'K', 'P', 'A', 'K'};
constexpr std::uint32_t PackEncrypted = 1u << 0;
constexpr std::uint16_t EntryDeflated = 1u << 0;
constexpr std::size_t SaltSize = 16;
constexpr std::size_t TocEntrySize = 32;
constexpr std::size_t KeySize = 32;
constexpr std::size_t NonceSize = 12;
constexpr std::size_t BlockSize = 64;

std::uint32_t readU32(const unsigned char* data)
{
    return std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8u) | (std::uint32_t(data[2]) << 16u) |
           (std::uint32_t(data[3]) << 24u);
}

std::uint64_t readU64(const unsigned char* data)
{
    return std::uint64_t(readU32(data)) | (std::uint64_t(readU32(data + 4)) << 32u);
}

std::uint16_t readU16(const unsigned char* data)
{
    return std::uint16_t(data[0]) | (std::uint16_t(data[1]) << 8u);
}

void appendU16(std::vector<unsigned char>& out, std::uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value));
    out.push_back(static_cast<unsigned char>(value >> 8u));
}

void appendU32(std::vector<unsigned char>& out, std::uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i)
        out.push_back(static_cast<unsigned char>(value >> (i * 8u)));
}

void appendU64(std::vector<unsigned char>& out, std::uint64_t value)
{
    appendU32(out, static_cast<std::uint32_t>(value));
    appendU32(out, static_cast<std::uint32_t>(value >> 32u));
}

bool validName(const std::string& name)
{
    return !name.empty() && name[0] != '/' && name.find("..") == std::string::npos;
}

std::string normalize(const char* path)
{
    if (!path)
        return std::string();
    std::string result(path);
    std::replace(result.begin(), result.end(), '\\', '/');
    while (result.size() >= 2 && result[0] == '.' && result[1] == '/')
        result.erase(0, 2);
    return result;
}

std::uint32_t rotateLeft(std::uint32_t value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

void quarterRound(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d)
{
    a += b;
    d = rotateLeft(d ^ a, 16);
    c += d;
    b = rotateLeft(b ^ c, 12);
    a += b;
    d = rotateLeft(d ^ a, 8);
    c += d;
    b = rotateLeft(b ^ c, 7);
}

void chachaBlock(const std::uint32_t state[16], unsigned char output[BlockSize])
{
    std::uint32_t work[16];
    std::memcpy(work, state, sizeof(work));
    for (int i = 0; i < 10; ++i)
    {
        quarterRound(work[0], work[4], work[8], work[12]);
        quarterRound(work[1], work[5], work[9], work[13]);
        quarterRound(work[2], work[6], work[10], work[14]);
        quarterRound(work[3], work[7], work[11], work[15]);
        quarterRound(work[0], work[5], work[10], work[15]);
        quarterRound(work[1], work[6], work[11], work[12]);
        quarterRound(work[2], work[7], work[8], work[13]);
        quarterRound(work[3], work[4], work[9], work[14]);
    }
    for (int i = 0; i < 16; ++i)
    {
        const std::uint32_t value = work[i] + state[i];
        output[i * 4 + 0] = static_cast<unsigned char>(value);
        output[i * 4 + 1] = static_cast<unsigned char>(value >> 8u);
        output[i * 4 + 2] = static_cast<unsigned char>(value >> 16u);
        output[i * 4 + 3] = static_cast<unsigned char>(value >> 24u);
    }
}

void cipher(const unsigned char key[KeySize], const unsigned char nonce[NonceSize], unsigned char* data,
            std::size_t size)
{
    std::uint32_t state[16] = {0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u};
    for (int i = 0; i < 8; ++i)
        state[4 + i] = readU32(key + i * 4);
    state[12] = 0;
    state[13] = readU32(nonce);
    state[14] = readU32(nonce + 4);
    state[15] = readU32(nonce + 8);

    unsigned char stream[BlockSize];
    for (std::size_t offset = 0; offset < size; offset += BlockSize)
    {
        chachaBlock(state, stream);
        ++state[12];
        const std::size_t count = (std::min)(BlockSize, size - offset);
        for (std::size_t i = 0; i < count; ++i)
            data[offset + i] ^= stream[i];
    }
}

std::uint64_t foldKey(const std::string& passphrase, std::uint64_t seed)
{
    std::uint64_t hash = 14695981039346656037ull ^ seed;
    for (unsigned char value : passphrase)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    hash ^= passphrase.size();
    return hash * 1099511628211ull;
}

void deriveKey(const std::string& passphrase, const unsigned char salt[SaltSize], unsigned char key[KeySize])
{
    for (int lane = 0; lane < 4; ++lane)
    {
        const std::uint64_t value = foldKey(passphrase, std::uint64_t(lane) * 0x9e3779b97f4a7c15ull);
        for (int byte = 0; byte < 8; ++byte)
            key[lane * 8 + byte] = static_cast<unsigned char>(value >> (byte * 8u));
    }
    unsigned char nonce[NonceSize];
    std::memcpy(nonce, salt, NonceSize);
    for (std::uint32_t round = 0; round < 4096; ++round)
    {
        std::uint32_t state[16] = {0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u};
        for (int i = 0; i < 8; ++i)
            state[4 + i] = readU32(key + i * 4);
        state[12] = round;
        state[13] = readU32(nonce);
        state[14] = readU32(nonce + 4);
        state[15] = readU32(nonce + 8);
        unsigned char stream[BlockSize];
        chachaBlock(state, stream);
        std::memcpy(key, stream, KeySize);
        for (std::size_t i = 0; i < SaltSize; ++i)
            key[i] ^= salt[i];
    }
}

void directoryNonce(unsigned char nonce[NonceSize])
{
    std::memset(nonce, 0xff, NonceSize);
}

void entryNonce(unsigned char nonce[NonceSize], std::uint32_t index)
{
    std::memset(nonce, 0, NonceSize);
    nonce[0] = static_cast<unsigned char>(index);
    nonce[1] = static_cast<unsigned char>(index >> 8u);
    nonce[2] = static_cast<unsigned char>(index >> 16u);
    nonce[3] = static_cast<unsigned char>(index >> 24u);
}

std::uint32_t keyChecksum(const unsigned char key[KeySize])
{
    return static_cast<std::uint32_t>(mz_crc32(0, key, KeySize));
}
} // namespace

KPak::KPak() : mFile(nullptr), mEncrypted(false)
{
    std::memset(mKey, 0, sizeof(mKey));
}

KPak::~KPak()
{
    Close();
}

bool KPak::Open(const char* path, const char* key)
{
    Close();
    if (!path || !path[0])
        return false;
    mFile = std::fopen(path, "rb");
    return mFile && ReadDirectory(key ? key : "");
}

void KPak::Close()
{
    if (mFile)
        std::fclose(mFile);
    mFile = nullptr;
    mEntries.clear();
    mIndex.clear();
    mEncrypted = false;
    std::memset(mKey, 0, sizeof(mKey));
}

bool KPak::IsOpen() const
{
    return mFile != nullptr;
}

bool KPak::ReadAt(std::uint64_t offset, void* output, std::size_t size) const
{
    if (!mFile || offset > static_cast<std::uint64_t>((std::numeric_limits<long>::max)()) ||
        std::fseek(mFile, static_cast<long>(offset), SEEK_SET) != 0)
        return false;
    return size == 0 || std::fread(output, 1, size, mFile) == size;
}

bool KPak::ReadDirectory(const char* key)
{
    std::array<unsigned char, HeaderSize> header{};
    if (!ReadAt(0, header.data(), header.size()) || std::memcmp(header.data(), Magic, sizeof(Magic)) != 0 ||
        readU32(header.data() + 4) != Version)
    {
        Close();
        return false;
    }

    const std::uint32_t flags = readU32(header.data() + 8);
    const std::uint32_t count = readU32(header.data() + 12);
    const std::uint64_t tocOffset = readU64(header.data() + 16);
    const std::uint32_t tocSize = readU32(header.data() + 24);
    const std::uint32_t namesSize = readU32(header.data() + 28);
    const unsigned char* salt = header.data() + 32;
    const std::uint32_t checksum = readU32(header.data() + 48);
    if (tocSize != count * TocEntrySize || namesSize > (std::numeric_limits<std::size_t>::max)() - tocSize)
    {
        Close();
        return false;
    }

    mEncrypted = (flags & PackEncrypted) != 0;
    if (mEncrypted)
    {
        if (!key || !key[0])
        {
            Close();
            return false;
        }
        deriveKey(key, salt, mKey);
        if (keyChecksum(mKey) != checksum)
        {
            Close();
            return false;
        }
    }

    std::vector<unsigned char> directory(std::size_t(tocSize) + namesSize);
    if (!ReadAt(tocOffset, directory.data(), directory.size()))
    {
        Close();
        return false;
    }
    if (mEncrypted)
    {
        unsigned char nonce[NonceSize];
        directoryNonce(nonce);
        cipher(mKey, nonce, directory.data(), directory.size());
    }

    mEntries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const unsigned char* source = directory.data() + std::size_t(i) * TocEntrySize;
        const std::uint32_t nameOffset = readU32(source);
        const std::uint32_t nameLength = readU32(source + 4);
        if (nameOffset > namesSize || nameLength > namesSize - nameOffset)
        {
            Close();
            return false;
        }
        Entry entry;
        entry.name.assign(reinterpret_cast<const char*>(directory.data() + tocSize + nameOffset), nameLength);
        entry.offset = readU64(source + 8);
        entry.storedSize = readU32(source + 16);
        entry.rawSize = readU32(source + 20);
        entry.crc = readU32(source + 24);
        entry.flags = readU16(source + 28);
        if (!validName(entry.name) || mIndex.find(entry.name) != mIndex.end())
        {
            Close();
            return false;
        }
        mIndex[entry.name] = mEntries.size();
        mEntries.push_back(std::move(entry));
    }
    return true;
}

bool KPak::Contains(const char* path) const
{
    return IsOpen() && mIndex.find(normalize(path)) != mIndex.end();
}

bool KPak::Read(const char* path, std::vector<unsigned char>& out) const
{
    out.clear();
    const auto found = mIndex.find(normalize(path));
    if (found == mIndex.end())
        return false;
    const Entry& entry = mEntries[found->second];
    std::vector<unsigned char> stored(entry.storedSize);
    if (!ReadAt(entry.offset, stored.data(), stored.size()))
        return false;
    if (mEncrypted)
    {
        unsigned char nonce[NonceSize];
        entryNonce(nonce, static_cast<std::uint32_t>(found->second));
        cipher(mKey, nonce, stored.data(), stored.size());
    }
    if ((entry.flags & EntryDeflated) != 0)
    {
        out.resize(entry.rawSize);
        mz_ulong rawSize = static_cast<mz_ulong>(out.size());
        if (mz_uncompress(out.data(), &rawSize, stored.data(), static_cast<mz_ulong>(stored.size())) != MZ_OK ||
            rawSize != out.size())
        {
            out.clear();
            return false;
        }
    }
    else
    {
        out = std::move(stored);
    }
    if (out.size() != entry.rawSize || static_cast<std::uint32_t>(mz_crc32(0, out.data(), out.size())) != entry.crc)
    {
        out.clear();
        return false;
    }
    return true;
}

std::size_t KPak::EntryCount() const
{
    return mEntries.size();
}

const char* KPak::EntryName(std::size_t index) const
{
    return index < mEntries.size() ? mEntries[index].name.c_str() : "";
}

std::uint32_t KPak::EntryRawSize(std::size_t index) const
{
    return index < mEntries.size() ? mEntries[index].rawSize : 0;
}

std::uint32_t KPak::EntryStoredSize(std::size_t index) const
{
    return index < mEntries.size() ? mEntries[index].storedSize : 0;
}

KPakWriter::KPakWriter() : mCompressionLevel(9), mRawSize(0), mStoredSize(0)
{
}

void KPakWriter::SetKey(const char* key)
{
    mKey = key ? key : "";
}

void KPakWriter::SetCompressionLevel(int level)
{
    mCompressionLevel = (std::max)(0, (std::min)(9, level));
}

bool KPakWriter::AddFile(const char* archivePath, const char* sourcePath)
{
    if (!sourcePath || !sourcePath[0])
        return false;
    std::FILE* file = std::fopen(sourcePath, "rb");
    if (!file)
        return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size < 0)
    {
        std::fclose(file);
        return false;
    }
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    const bool read = data.empty() || std::fread(data.data(), 1, data.size(), file) == data.size();
    std::fclose(file);
    return read && AddData(archivePath, data.data(), data.size());
}

bool KPakWriter::AddData(const char* archivePath, const void* data, std::size_t size)
{
    const std::string name = normalize(archivePath);
    if (!validName(name) || (!data && size != 0) || size > (std::numeric_limits<std::uint32_t>::max)() ||
        mIndex.find(name) != mIndex.end())
        return false;

    Pending pending;
    pending.name = name;
    pending.rawSize = static_cast<std::uint32_t>(size);
    pending.crc = static_cast<std::uint32_t>(mz_crc32(0, static_cast<const unsigned char*>(data), size));
    pending.stored.resize(size);
    if (size > 0)
        std::memcpy(pending.stored.data(), data, size);
    if (mCompressionLevel > 0 && size > 0)
    {
        std::vector<unsigned char> compressed(mz_compressBound(static_cast<mz_ulong>(size)));
        mz_ulong compressedSize = static_cast<mz_ulong>(compressed.size());
        if (mz_compress2(compressed.data(), &compressedSize, pending.stored.data(), static_cast<mz_ulong>(size),
                         mCompressionLevel) == MZ_OK &&
            compressedSize < size)
        {
            compressed.resize(compressedSize);
            pending.stored = std::move(compressed);
            pending.flags |= EntryDeflated;
        }
    }
    mRawSize += size;
    mStoredSize += pending.stored.size();
    mIndex[name] = mEntries.size();
    mEntries.push_back(std::move(pending));
    return true;
}

bool KPakWriter::Write(const char* path) const
{
    if (!path || !path[0] || mEntries.size() > (std::numeric_limits<std::uint32_t>::max)())
        return false;
    unsigned char salt[SaltSize];
    std::random_device random;
    for (unsigned char& value : salt)
        value = static_cast<unsigned char>(random());
    const bool encrypted = !mKey.empty();
    unsigned char key[KeySize] = {};
    if (encrypted)
        deriveKey(mKey, salt, key);

    std::string names;
    std::vector<unsigned char> toc;
    toc.reserve(mEntries.size() * TocEntrySize);
    std::uint64_t offset = KPak::HeaderSize;
    for (const Pending& entry : mEntries)
    {
        if (entry.name.size() > (std::numeric_limits<std::uint32_t>::max)() ||
            names.size() > (std::numeric_limits<std::uint32_t>::max)() - entry.name.size())
            return false;
        appendU32(toc, static_cast<std::uint32_t>(names.size()));
        appendU32(toc, static_cast<std::uint32_t>(entry.name.size()));
        appendU64(toc, offset);
        appendU32(toc, static_cast<std::uint32_t>(entry.stored.size()));
        appendU32(toc, entry.rawSize);
        appendU32(toc, entry.crc);
        appendU16(toc, entry.flags);
        appendU16(toc, 0);
        names += entry.name;
        offset += entry.stored.size();
    }

    std::vector<unsigned char> header;
    header.insert(header.end(), Magic, Magic + sizeof(Magic));
    appendU32(header, KPak::Version);
    appendU32(header, encrypted ? PackEncrypted : 0);
    appendU32(header, static_cast<std::uint32_t>(mEntries.size()));
    appendU64(header, offset);
    appendU32(header, static_cast<std::uint32_t>(toc.size()));
    appendU32(header, static_cast<std::uint32_t>(names.size()));
    header.insert(header.end(), salt, salt + SaltSize);
    appendU32(header, encrypted ? keyChecksum(key) : 0);
    appendU32(header, 0);
    appendU32(header, 0);
    appendU32(header, 0);
    if (header.size() != KPak::HeaderSize)
        return false;

    std::FILE* file = std::fopen(path, "wb");
    if (!file)
        return false;
    bool ok = std::fwrite(header.data(), 1, header.size(), file) == header.size();
    for (std::size_t i = 0; ok && i < mEntries.size(); ++i)
    {
        std::vector<unsigned char> data = mEntries[i].stored;
        if (encrypted)
        {
            unsigned char nonce[NonceSize];
            entryNonce(nonce, static_cast<std::uint32_t>(i));
            cipher(key, nonce, data.data(), data.size());
        }
        ok = data.empty() || std::fwrite(data.data(), 1, data.size(), file) == data.size();
    }
    std::vector<unsigned char> directory = toc;
    directory.insert(directory.end(), names.begin(), names.end());
    if (encrypted)
    {
        unsigned char nonce[NonceSize];
        directoryNonce(nonce);
        cipher(key, nonce, directory.data(), directory.size());
    }
    ok = ok && (directory.empty() || std::fwrite(directory.data(), 1, directory.size(), file) == directory.size());
    std::fclose(file);
    return ok;
}

std::size_t KPakWriter::EntryCount() const
{
    return mEntries.size();
}

std::uint64_t KPakWriter::RawSize() const
{
    return mRawSize;
}

std::uint64_t KPakWriter::StoredSize() const
{
    return mStoredSize;
}
} // namespace k2d
