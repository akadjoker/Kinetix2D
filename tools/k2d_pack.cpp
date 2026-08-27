#include <k2d/KPak.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace
{
struct Root
{
    ct::String path;
    ct::String prefix;
};

struct Options
{
    ct::String output;
    ct::String key;
    ct::Vector<Root> roots;
    int compressionLevel = 9;
    bool list = false;
    bool verbose = false;
};

void PrintUsage()
{
    std::printf("k2d_pack - builds and inspects Kinetix .kpak archives\n"
                "\n"
                "  k2d_pack -o <out.kpak> [options] <path>...\n"
                "  k2d_pack -t <pack.kpak> [-k <key>]\n"
                "\n"
                "Each path is stored relative to its root. Use -p to put a prefix in\n"
                "front of the next roots, for example: -p textures assets/textures\n"
                "\n"
                "  -o <file>   archive to create\n"
                "  -t <file>   list an archive\n"
                "  -k <key>    encrypt the archive with this key\n"
                "  -p <name>   archive prefix for following roots\n"
                "  -l <0-9>    deflate level (default 9; 0 stores raw)\n"
                "  -v          print each added file\n");
}

bool ParseArguments(int argc, char** argv, Options& options)
{
    ct::String prefix;
    for (int i = 1; i < argc; ++i)
    {
        const ct::String argument = argv[i];
        if (argument == "-o" && i + 1 < argc)
            options.output = argv[++i];
        else if (argument == "-t" && i + 1 < argc)
        {
            options.output = argv[++i];
            options.list = true;
        }
        else if (argument == "-k" && i + 1 < argc)
            options.key = argv[++i];
        else if (argument == "-p" && i + 1 < argc)
            prefix = argv[++i];
        else if (argument == "-l" && i + 1 < argc)
            options.compressionLevel = std::atoi(argv[++i]);
        else if (argument == "-v")
            options.verbose = true;
        else if (argument == "-h" || argument == "--help")
            return false;
        else if (!argument.empty() && argument[0] == '-')
        {
            std::printf("k2d_pack: unknown option %s\n", argument.c_str());
            return false;
        }
        else
            options.roots.push_back({argument, prefix});
    }

    if (options.output.empty() || options.compressionLevel < 0 || options.compressionLevel > 9)
        return false;
    return options.list || !options.roots.empty();
}

ct::String JoinPath(const ct::String& left, const ct::String& right)
{
    if (left.empty())
        return right;
    if (right.empty())
        return left;
    return left + "/" + right;
}

ct::String BaseName(const ct::String& path)
{
    const ct::String::size_type separator = path.find_last_of("/\\");
    return separator == ct::String::npos ? path : path.substr(separator + 1);
}

bool IsDirectory(const ct::String& path)
{
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool AddFile(k2d::KPakWriter& writer, const ct::String& archivePath, const ct::String& sourcePath, bool verbose,
             std::size_t& added)
{
    if (!writer.AddFile(archivePath.c_str(), sourcePath.c_str()))
    {
        std::printf("k2d_pack: could not add %s\n", sourcePath.c_str());
        return false;
    }
    ++added;
    if (verbose)
        std::printf("  %s\n", archivePath.c_str());
    return true;
}

#if defined(_WIN32)
bool AddDirectory(k2d::KPakWriter& writer, const ct::String& root, const ct::String& prefix,
                  const ct::String& relative, bool verbose, std::size_t& added)
{
    const ct::String directory = relative.empty() ? root : JoinPath(root, relative);
    WIN32_FIND_DATAA entry;
    HANDLE search = FindFirstFileA(JoinPath(directory, "*").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        const ct::String name = entry.cFileName;
        if (name == "." || name == "..")
            continue;
        const ct::String child = JoinPath(relative, name);
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            if (!AddDirectory(writer, root, prefix, child, verbose, added))
            {
                FindClose(search);
                return false;
            }
        }
        else if (!AddFile(writer, JoinPath(prefix, child), JoinPath(root, child), verbose, added))
        {
            FindClose(search);
            return false;
        }
    } while (FindNextFileA(search, &entry));

    FindClose(search);
    return true;
}
#else
bool AddDirectory(k2d::KPakWriter& writer, const ct::String& root, const ct::String& prefix,
                  const ct::String& relative, bool verbose, std::size_t& added)
{
    const ct::String directory = relative.empty() ? root : JoinPath(root, relative);
    DIR* const handle = opendir(directory.c_str());
    if (!handle)
        return false;

    while (dirent* const entry = readdir(handle))
    {
        const ct::String name = entry->d_name;
        if (name == "." || name == "..")
            continue;
        const ct::String child = JoinPath(relative, name);
        const ct::String source = JoinPath(root, child);
        if (IsDirectory(source))
        {
            if (!AddDirectory(writer, root, prefix, child, verbose, added))
            {
                closedir(handle);
                return false;
            }
        }
        else if (!AddFile(writer, JoinPath(prefix, child), source, verbose, added))
        {
            closedir(handle);
            return false;
        }
    }

    closedir(handle);
    return true;
}
#endif

int ListPack(const Options& options)
{
    k2d::KPak pack;
    if (!pack.Open(options.output.c_str(), options.key.c_str()))
    {
        std::printf("k2d_pack: could not open %s (wrong key or invalid archive)\n", options.output.c_str());
        return 1;
    }

    std::uint64_t raw = 0;
    std::uint64_t stored = 0;
    for (std::size_t i = 0; i < pack.EntryCount(); ++i)
    {
        raw += pack.EntryRawSize(i);
        stored += pack.EntryStoredSize(i);
        std::printf("%10u  %10u  %s\n", pack.EntryRawSize(i), pack.EntryStoredSize(i), pack.EntryName(i));
    }
    std::printf("%zu entries, %llu bytes stored from %llu\n", pack.EntryCount(),
                static_cast<unsigned long long>(stored), static_cast<unsigned long long>(raw));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!ParseArguments(argc, argv, options))
    {
        PrintUsage();
        return 1;
    }
    if (options.list)
        return ListPack(options);

    k2d::KPakWriter writer;
    writer.SetKey(options.key.c_str());
    writer.SetCompressionLevel(options.compressionLevel);

    std::size_t added = 0;
    for (const Root& root : options.roots)
    {
        if (IsDirectory(root.path))
        {
            if (!AddDirectory(writer, root.path, root.prefix, "", options.verbose, added))
            {
                std::printf("k2d_pack: could not read %s\n", root.path.c_str());
                return 1;
            }
        }
        else if (!AddFile(writer, JoinPath(root.prefix, BaseName(root.path)), root.path, options.verbose, added))
            return 1;
    }

    if (!writer.Write(options.output.c_str()))
    {
        std::printf("k2d_pack: could not write %s\n", options.output.c_str());
        return 1;
    }

    const double ratio = writer.RawSize() == 0
                             ? 0.0
                             : 100.0 * static_cast<double>(writer.StoredSize()) / static_cast<double>(writer.RawSize());
    std::printf("%s: %zu entries, %llu bytes from %llu (%.1f%%)%s\n", options.output.c_str(), added,
                static_cast<unsigned long long>(writer.StoredSize()), static_cast<unsigned long long>(writer.RawSize()),
                ratio, options.key.empty() ? "" : ", encrypted");
    return 0;
}
