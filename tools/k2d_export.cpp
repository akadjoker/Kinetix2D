#include "ProjectExport.h"

#include <k2d/FileSystem.h>

#include <platform.h>

#include <cstdio>
#include <cstring>

namespace
{

ct::String besideTool(const char* name)
{
    char base[4096], buffer[4096];
    return fs_get_base_path(base, sizeof base) && path_join(buffer, sizeof buffer, base, name) ? ct::String(buffer)
                                                                                               : ct::String(name);
}

} // namespace

int main(int argc, char** argv)
{
    const char* project = nullptr;
    ct::String out;
    ct::String runner;
    bool desktop = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--fix-paths") == 0 && i + 1 < argc)
        {
            const int changed = k2d::FixPathsInFiles(argv[i + 1]);
            if (changed < 0)
                std::fprintf(stderr, "k2d_export: cannot read %s\n", argv[i + 1]);
            else
                std::printf("%d file(s) rewritten\n", changed);
            return changed < 0 ? 1 : 0;
        }
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (std::strcmp(argv[i], "--runner") == 0 && i + 1 < argc)
            runner = argv[++i];
        else if (std::strcmp(argv[i], "--desktop") == 0)
            desktop = true;
        else if (!project)
            project = argv[i];
    }
    if (!project)
    {
        std::printf("k2d_export - a project as a game: for the web (k2d_runner.js and .wasm, data.kpak,\n"
                    "index.html) or, with --desktop, for this system (the runner named after the game and\n"
                    "data.kpak beside it)\n\n"
                    "Usage: k2d_export <project folder> [--desktop] [-o out] [--runner path]\n"
                    "       k2d_export --fix-paths <folder>   (absolute paths in its scenes and prefabs made relative to it)\n\n"
                    "  -o        where the game goes (default: <project>/export/web or export/desktop)\n"
                    "  --runner  the runner to use: the Emscripten build's folder for the web (default: web/ beside\n"
                    "            this program), the k2d_runner program for the desktop (default: beside this program)\n");
        return 1;
    }
    char buffer[4096];
    if (out.empty())
        out = path_join(buffer, sizeof buffer, project, desktop ? "export/desktop" : "export/web") ? ct::String(buffer)
                                                                                                  : ct::String("game");
    if (runner.empty())
    {
#if defined(_WIN32)
        runner = besideTool(desktop ? "k2d_runner.exe" : "web");
#else
        runner = besideTool(desktop ? "k2d_runner" : "web");
#endif
    }
    k2d::FileSystem::Instance().Init();
    ct::String error, game;
    const bool exported = desktop ? k2d::ExportDesktop(project, runner.c_str(), out.c_str(), game, error)
                                  : k2d::ExportWeb(project, runner.c_str(), out.c_str(), error);
    if (!exported)
        std::fprintf(stderr, "k2d_export: %s\n", error.c_str());
    k2d::FileSystem::Instance().Shutdown();
    return exported ? 0 : 1;
}
