# Kinetix2D

Kinetix2D is a C++ 2D engine with an editor, runner, Chipmunk2D-based physics, and ZenScript scripting.

## Build

CMake, a C++ compiler, and the SDL2 development libraries are required.

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
~~~

On Windows, if SDL2 is provided through vcpkg:

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
~~~

## Local Windows cross-build

Linux developers can validate the Windows build before pushing. Keep a portable, Linux-hosted MinGW-w64 SDK outside Git at `.k2d-tools/mingw64/`; its `bin/` directory must contain `x86_64-w64-mingw32-g++`.

~~~sh
# Fast platform check: builds Zen only, with no SDL2 dependency.
./tools/dev/build-windows-mingw.sh zen

# Full engine build. vcpkg is cloned and bootstrapped under .k2d-tools/vcpkg.
./tools/dev/build-windows-mingw.sh
~~~

The toolchain, vcpkg checkout, downloaded ports, and build output stay in ignored local folders. No system-wide package installation is needed.

Executables are written to `bin/`:

~~~sh
./bin/k2d_editor
./bin/k2d_runner assets/senes/scene.k2dscene
~~~

## Layout

- `engine/` — core systems, rendering, audio, input, assets, and serialization.
- `physics/` and `physics2d/` — shapes, queries, and Chipmunk2D integration.
- `scripting/` — the ZenScript VM and engine bindings.
- `editor/` — the scene editor and content tools.
- `runner/` — runs a project or scene.
- `tools/k2d_pack/` — asset package builder.
- `assets/` — sample assets and scenes.

Editor and language documentation live in `editor/README.md` and `scripting/README.md`.

## Asset packages

`k2d_pack` creates a `.kpak` package from a directory. The runtime mounts it through `FileSystem` and reads assets using the same logical paths as loose files.

~~~sh
./bin/k2d_pack assets game.kpak
./bin/k2d_pack assets game.kpak --key my-key
~~~

Compression uses miniz, built statically by CMake. zlib is not required on the host system.

## Tests

~~~sh
./bin/k2d_kpak_tests
./bin/k2d_zen_tests
./bin/k2d_serializer_tests
~~~

## CI and releases

The workflow in `.github/workflows/release.yml` builds the editor, runner, packer, and tests on Linux and Windows. Each run uploads the binaries as artifacts. A `v*` tag also creates a release with the packages.

## Credits

The project integrates or references SDL2, Chipmunk2D, GLAD, miniz, Box2D, and the Zen libraries.

## License

See [LICENSE](LICENSE).
