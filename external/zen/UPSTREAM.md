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
files listed below. Radion vendors the same commit with the same three
omissions, so the three copies can be diffed against each other directly.

## Deliberately absent

- `src/builtin_http.cpp`
- `src/builtin_net.cpp`
- `src/builtin_numpy.cpp`

Kinetix2D registers only `zen_lib_base`, `zen_lib_math` and `zen_lib_time`
(`scripting/src/ZenScriptComponent.cpp`), and the two network modules pull in
sockets. Leaving them out needs no change to the library: module imports resolve
through a runtime registry, so a module that was never registered simply is not
there. `CMakeLists.txt` globs `src/*.cpp`, so removing the files removes them
from the build.

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
diff -rq external/zen/src     <zenpy>/libzen/src       # expect only the four files above
```

One VM-level bug is known and belongs upstream, not here: `VM::run` resets
`main_fiber_` (`frame_count = 1`, `frame->base = stack`), so compiling and
running a module from inside a native called by a running script corrupts the
caller's frames. Kinetix2D works around it by deferring the compile out of the
VM instead of patching the copy — see `ZenScriptComponent::loadFile`.
