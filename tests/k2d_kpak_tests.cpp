#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/KPak.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
bool same(const std::vector<unsigned char>& data, const char* text)
{
    const std::size_t size = std::strlen(text);
    return data.size() == size && (size == 0 || std::memcmp(data.data(), text, size) == 0);
}

bool testRoundTrip(const char* path, const char* key)
{
    k2d::KPakWriter writer;
    writer.SetKey(key);
    const char* hello = "hello from kpak";
    std::vector<unsigned char> repeated(32768, 'a');
    if (!writer.AddData("texts/hello.txt", hello, std::strlen(hello)) ||
        !writer.AddData("binary/repeated.bin", repeated.data(), repeated.size()) ||
        !writer.AddData("empty", nullptr, 0) || writer.AddData("texts\\hello.txt", hello, std::strlen(hello)) ||
        !writer.Write(path))
        return false;

    k2d::KPak pack;
    if (!pack.Open(path, key) || pack.EntryCount() != 3 || !pack.Contains("texts\\hello.txt") ||
        pack.Contains("missing"))
        return false;
    std::vector<unsigned char> data;
    return pack.Read("texts/hello.txt", data) && same(data, hello) && pack.Read("binary/repeated.bin", data) &&
           data == repeated && pack.Read("empty", data) && data.empty();
}

bool testMounted(const char* path, const char* key)
{
    k2d::FileSystem& files = k2d::FileSystem::Instance();
    files.UnmountPacks();
    if ((key[0] && files.MountPack(path, "wrong")) || !files.MountPack(path, key) || !files.Exists("texts/hello.txt"))
        return false;
    k2d::FileBuffer buffer;
    const bool ok =
        files.LoadFile("texts/hello.txt", buffer, true) && std::strcmp(buffer.Text(), "hello from kpak") == 0;
    files.UnmountPacks();
    return ok && !files.Exists("texts/hello.txt");
}
} // namespace

int main()
{
    const char* path = "k2d_kpak_test.kpak";
    const char* key = "kinetix test key";
    const bool plainRoundTrip = testRoundTrip(path, "");
    const bool plainMounted = plainRoundTrip && testMounted(path, "");
    const bool encryptedRoundTrip = testRoundTrip(path, key);
    const bool encryptedMounted = encryptedRoundTrip && testMounted(path, key);
    const bool plain = plainRoundTrip && plainMounted;
    const bool encrypted = encryptedRoundTrip && encryptedMounted;
    std::remove(path);
    std::printf("kpak: plain=%s/%s encrypted=%s/%s\n", plainRoundTrip ? "round" : "fail",
                plainMounted ? "mount" : "fail", encryptedRoundTrip ? "round" : "fail",
                encryptedMounted ? "mount" : "fail");
    return plain && encrypted ? 0 : 1;
}
