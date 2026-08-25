#include <k2d/ZenRuntime.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>

#include <ct/json.hpp>

#include <cstdio>
#include <cstring>

namespace
{
void printUsage()
{
    std::printf("k2d_scriptc - compile a Zen script to Kinetix bytecode\n\n"
                "Usage:\n"
                "  k2d_scriptc [-g] <source.py> <output.zbc>\n"
                "  k2d_scriptc [-g] --bundle <bundle.zbc> <manifest.json> <script.py>...\n"
                "\n"
                "  -g  keep debug data (release exports strip it by default)\n");
}

bool isIdentifierStart(char value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}

bool isIdentifierChar(char value)
{
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

ct::String firstClassName(const char* source)
{
    if (!source)
        return ct::String();
    const char* cursor = source;
    while (*cursor)
    {
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        if (std::strncmp(cursor, "class", 5) == 0 && (cursor[5] == ' ' || cursor[5] == '\t'))
        {
            cursor += 5;
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            if (!isIdentifierStart(*cursor))
                return ct::String();
            const char* begin = cursor++;
            while (isIdentifierChar(*cursor))
                ++cursor;
            return ct::String(begin, static_cast<size_t>(cursor - begin));
        }
        while (*cursor && *cursor != '\n')
            ++cursor;
        if (*cursor == '\n')
            ++cursor;
    }
    return ct::String();
}
}

int main(int argc, char** argv)
{
    bool stripDebug = true;
    int argument = 1;
    if (argc > 1 && std::strcmp(argv[1], "-g") == 0)
    {
        stripDebug = false;
        ++argument;
    }

    if (argument < argc && std::strcmp(argv[argument], "--bundle") == 0)
    {
        ++argument;
        if (argc - argument < 3)
        {
            printUsage();
            return 1;
        }

        const char* bytecodePath = argv[argument++];
        const char* manifestPath = argv[argument++];
        ct::String source;
        ct::Json scripts = ct::Json::array();
        int scriptCount = 0;
        for (; argument < argc; ++argument)
        {
            k2d::FileBuffer file;
            if (!k2d::FileSystem::Instance().LoadFile(argv[argument], file, true))
            {
                std::fprintf(stderr, "k2d_scriptc: could not read %s\n", argv[argument]);
                return 1;
            }
            const ct::String className = firstClassName(file.Text());
            if (className.empty())
            {
                std::fprintf(stderr, "k2d_scriptc: %s does not define a script class\n", argv[argument]);
                return 1;
            }

            source += "\n";
            source += file.Text();
            source += "\n";

            ct::Json script = ct::Json::object();
            script.set("path", ct::Json(argv[argument]));
            script.set("class", ct::Json(className));
            scripts.push_back(script);
            ++scriptCount;
        }

        ct::String error;
        if (!k2d::ZenRuntime::instance().compileSourceToBytecode(source.c_str(), "<k2d-web-bundle>",
                                                                  bytecodePath, stripDebug, &error))
        {
            std::fprintf(stderr, "k2d_scriptc: %s\n", error.empty() ? "bundle compilation failed" : error.c_str());
            return 1;
        }

        ct::Json manifest = ct::Json::object();
        manifest.set("format", ct::Json("k2d-zen-bytecode-bundle"));
        manifest.set("version", ct::Json((int64_t)1));
        manifest.set("scripts", scripts);
        if (!k2d::FileSystem::Instance().SaveTextFile(manifestPath, manifest.dump(2)))
        {
            std::fprintf(stderr, "k2d_scriptc: could not write %s\n", manifestPath);
            return 1;
        }

        std::printf("Compiled %d scripts -> %s\n", scriptCount, bytecodePath);
        return 0;
    }

    if (argc - argument != 2)
    {
        printUsage();
        return 1;
    }

    ct::String error;
    if (!k2d::ZenRuntime::instance().compileFileToBytecode(argv[argument], argv[argument + 1],
                                                            stripDebug, &error))
    {
        std::fprintf(stderr, "k2d_scriptc: %s\n", error.empty() ? "compilation failed" : error.c_str());
        return 1;
    }

    std::printf("Compiled %s -> %s\n", argv[argument], argv[argument + 1]);
    return 0;
}
