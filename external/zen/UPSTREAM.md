# libzen — submodule

Zen is the scripting language Kinetix2D embeds for game behaviour
(`scripting/`, see `scripting/README.md`). It used to be a vendored copy kept in
step by hand; it is now a submodule, like `external/containers` and
`external/math`.

| | |
|---|---|
| Upstream | `https://github.com/akadjoker/zenpy` — submodule at `upstream/` |
| License | zlib, see `upstream/LICENSE` |

A fresh clone needs `git submodule update --init --recursive`; the build stops
with a readable error otherwise.

## What this directory owns

Only `CMakeLists.txt`, and only two lines of it: the guard above, and

    set(ZEN_HOST_OUTPUT ON CACHE BOOL "" FORCE)

before `add_subdirectory(upstream/libzen)`. Upstream's own CMake builds the
library — every platform decision (`ws2_32` on Windows, `log` on Android, the
MSVC flag split) lives there, so there is nothing here to drift.

`ZEN_HOST_OUTPUT` compiles in `zen_host_output.cpp`, a writer hook that
`print()` and runtime errors go through, so the editor can route them into its
Console. `zenconf.h` includes its header under the option and its `#if
!defined(zen_write)` guards then leave the platform defaults alone. The option
is off upstream by default and the implementation is compiled only under it, so
forgetting it here would be a link error in `ZenScriptComponent`, not a writer
that is silently never consulted.

`builtin_numpy.cpp` is built and never registered. That costs nothing: the VM
only answers `import <name>` for modules passed to `register_lib`, and nothing
references `zen_lib_numpy`, so the linker drops the object. Same for `io`, `os`,
`path` and `struct`, which are compiled in and left unregistered on purpose —
see the note in `scripting/README.md`. `ZenScriptComponent.cpp` registers `base`
(as globals), `math`, `time`, `json`, `net` and `http`.

## Changing the VM

Upstream is the source of truth and the only place to change. A bug found here
is fixed in `zenpy/libzen`, pushed, and then picked up by bumping the submodule:

```sh
git -C external/zen/upstream pull origin main
git add external/zen/upstream
```

Run zenpy's own suite there before bumping, then Kinetix2D's `k2d_zen_*` from
inside `bin/`.
