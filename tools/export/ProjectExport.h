#pragma once

#include <ct/string.hpp>

namespace k2d
{

// The project's scenes, assets and settings in one pack, its scripts compiled
// to a bytecode bundle, and paths inside the project made relative, as the
// runner reads them when it plays a pack.
bool WriteProjectPack(const char* projectRoot, const char* packPath, ct::String& error);

// A web game in outDir: the web runner from runnerDir (k2d_runner.js and
// k2d_runner.wasm), data.kpak and the page that loads it.
bool ExportWeb(const char* projectRoot, const char* runnerDir, const char* outDir, ct::String& error);

} // namespace k2d
