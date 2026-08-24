# libzen — vendored copy

Zen is the scripting language Kinetix2D embeds for game behaviour (`scripting/`,
see `scripting/README.md`). This is a copy, not a submodule — unlike
`external/containers` and `external/math`, which are — and
`external/zen/CMakeLists.txt` is Kinetix2D's own.

| | |
|---|---|
| Upstream repo | `https://github.com/akadjoker/zenpy` — directory `libzen/` |
| Commit | `cfea55225009e891ec82474ef1646b0375865d45` (`cfea552`), branch `physics-box2d` |
| Date | 2026-08-24 |
| Mirror | `https://github.com/akadjoker/zenpy_lib` — same files, standalone repo |
| License | zlib, see `LICENSE` |

`include/` and `src/` are byte for byte identical to upstream, except for the
files listed below. Radion vendors the same commit, so the three copies can be
diffed against each other directly — mind that Radion omits three modules where
Kinetix2D omits one.

## Deliberately absent

- `src/builtin_numpy.cpp`

Leaving a module out needs no change to the library: imports resolve through a
runtime registry, so a module that was never registered simply is not there.
`CMakeLists.txt` globs `src/*.cpp`, so removing the file removes it from the
build. Radion drops `builtin_http.cpp` and `builtin_net.cpp` as well; Kinetix2D
keeps and registers both.

Note that shipping a `builtin_*.cpp` is not the same as exposing it. The VM only
answers `import <name>` for modules passed to `register_lib`, and
`scripting/src/ZenScriptComponent.cpp` currently registers `base` (as globals),
`math`, `time`, `net` and `http`. `io`, `json`, `os`, `path` and `struct` are
compiled in but unregistered, so scripts cannot import them yet.

## Changes carried here

`zen_host_output.h` / `zen_host_output.cpp` — not upstream. A writer hook that
`print()` and error output go through, so the editor can route them into its
Console instead of stdout. `CMakeLists.txt` force-includes the header
(`-include zen/zen_host_output.h`) so the `zen_write*` macros reach every
translation unit.

## Keeping the copy in sync

Upstream is the source of truth. A bug found here is fixed in `zenpy/libzen`
first, then copied outwards:

```
zenpy/libzen  ->  zenpy_lib  ->  Kinetix2D external/zen
```

To check what has drifted, and to update the table above afterwards:

```sh
diff -rq external/zen/include <zenpy>/libzen/include   # expect only zen_host_output.h
diff -rq external/zen/src     <zenpy>/libzen/src       # expect only numpy and zen_host_output
```

One VM-level bug is known and belongs upstream, not here: `VM::run` resets
`main_fiber_` (`frame_count = 1`, `frame->base = stack`), so compiling and
running a module from inside a native called by a running script corrupts the
caller's frames. Kinetix2D works around it by deferring the compile out of the
VM instead of patching the copy — see `ZenScriptComponent::loadFile`.
