#include "ProjectExport.h"

#include <k2d/FileSystem.h>

#include <platform.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    const char* project = nullptr;
    ct::String out;
    ct::String runner;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (std::strcmp(argv[i], "--runner") == 0 && i + 1 < argc)
            runner = argv[++i];
        else if (!project)
            project = argv[i];
    }
    if (!project)
    {
        std::printf("k2d_export - a project as a web game: k2d_runner.js and .wasm, data.kpak, index.html\n\n"
                    "Usage: k2d_export <project folder> [-o out] [--runner folder]\n\n"
                    "  -o        where the game goes (default: <project>/export/web)\n"
                    "  --runner  the Emscripten build of k2d_runner (default: web/ beside this program)\n");
        return 1;
    }
    char buffer[4096];
    if (out.empty())
        out = path_join(buffer, sizeof buffer, project, "export/web") ? ct::String(buffer) : ct::String("web");
    if (runner.empty())
    {
        char base[4096];
        runner = fs_get_base_path(base, sizeof base) && path_join(buffer, sizeof buffer, base, "web")
                     ? ct::String(buffer)
                     : ct::String("web");
    }
    k2d::FileSystem::Instance().Init();
    ct::String error;
    const bool exported = k2d::ExportWeb(project, runner.c_str(), out.c_str(), error);
    if (!exported)
        std::fprintf(stderr, "k2d_export: %s\n", error.c_str());
    k2d::FileSystem::Instance().Shutdown();
    return exported ? 0 : 1;
}
