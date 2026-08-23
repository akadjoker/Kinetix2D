# Kinetix2D Editor

Initial 2D editor shell built directly on `k2d::Scene`.

## Layout

- `src/core`: application lifecycle, selection and shared panel base.
- `src/panels`: dockable editor panels.
- `src/widgets`: Kinetix2D-specific widgets (reserved for the component registry,
  gizmos and asset fields).
- `external/imgui_widgets`: editor-only ImGui add-ons. Dear ImGui itself remains
  shared with `../external/imgui`.

## Build and run

```sh
cmake -S . -B build
cmake --build build --target k2d_editor
./bin/k2d_editor
```

The current shell provides docking, Hierarchy, base Inspector, Assets, Console,
Game and a 2D Scene view with Material Design toolbars for selection, move, rotate,
scale, pan, snap and grid. The application toolbar also provides icon actions for
scene files, undo/redo and Play/Pause/Step/Stop. Undo/redo records scene edits as
`ct::Json` snapshots and supports `Ctrl+Z`, `Ctrl+Y` and `Ctrl+Shift+Z`. Scene rendering,
serializer-backed Open/Save, component registration and Play cloning are
the next integration milestones; placeholder UI is labelled accordingly.

The `View > Theme` menu exposes the themes imported from the new Radion editor:
Radion Dark, Light, Blender, Nord and Ember.

Code under `editor/src` uses the project containers (`ct::String`, `ct::Vector`,
`ct::Unique` and `ct::Json`) instead of direct `std` containers. Platform directory
enumeration is isolated in `core/EditorFileSystem`.
