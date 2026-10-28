# Editor quick workflow

## Projects and the web

**File → New Project...** asks for a folder and a name and makes the project
there: `scenes/`, `assets/`, `project.k2dproj` and a startup scene with a
player that moves with the arrows or WASD, so **Play** works at once.
**Open Project...** opens a `project.k2dproj` at its startup scene, and the
editor opens the last project again when started with no arguments.
**Set as Startup Scene** makes the open scene the one the game starts with.

**Export Web** writes the game into `<project>/export/web`: `index.html`,
`data.kpak` (scenes, assets, scripts compiled to bytecode) and the web
runner. **Run Web** (F7) also serves it on `http://127.0.0.1:8090/` and
opens the browser. Upload the folder to any static host to publish it. The
web runner is the Emscripten build of `k2d_runner` in `bin/web/`, made once:

    emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DK2D_RUNTIME_OUTPUT_DIRECTORY=$PWD/bin/web
    cmake --build build-web --target k2d_runner

`k2d_export <project>` does the same export from the command line.

## Scripts

In **Assets**, use the file-plus button to create a `.py` script. Pick a
template, then drag the file to a **Zen Script** component in the Inspector.
Scripts use `class Name(ScriptComponent):`; `self.node` is the hosting object.
Run them with **Play**, read `print()` output in **Console**, and enable
**Script Hot Reload** to recompile changed files while running.

## Components

Use **Add Component...** in the Inspector. Hover an item for its short
description; opening a component shows the same reminder above its properties.
`*Shape` components are visual, while `*Collider2D` components belong to
physics.

## Scene view

Click an object's sprite, shape or collider to select it (an origin marker
under the pointer wins, then the smallest outline). With the **Select** tool
(Q) the selected object's colliders show their points: drag one to move it,
double-click a polygon or chain edge to add a point there, Ctrl+click a point
to remove it. A box has a corner to resize it, a circle a point on its rim.
The collider button in the toolbar shows every object's colliders: green
solid, blue sensor, orange when the object has no RigidBody2D and the collider
does nothing.

## Atlas

The **Atlas** tab packs images into one texture. Drop images or folders from
Assets on its list (or add a folder with its button), pick the largest page
size, the padding and whether to trim transparent borders, and build: it asks
where the image goes and writes `name.png`, `name.sprite.json` with a region
per image named after it, and `name.atlas.json` with the list, which opens the
Atlas tab again from Assets. The result opens in the Sprite Editor, from where
its regions are dragged onto the scene or into the Animator. Images that need
more than one page give `name_1.png`, `name_2.png`... Trimming moves an image's
pixels, so leave it off for animation frames that must line up.

## Skeletons

A skeleton is an object with a **Skeleton2D** and, below it, objects with a
**Bone2D**: each bone points along its local x for its length. The scene view
draws the bones; click one to select it. With the **Select** tool, dragging a
bone turns it, and dragging its tip pulls the chain above it by IK (the **IK**
field of the Skeleton panel says how many bones move).

The **Skeleton** panel, under the views, lists the clips (new, duplicate,
delete, name, length, loop) and shows each animated bone and property as a
track with its keys. Turn on **Animate** (the record button) to work on the
clip: the bones show the clip at the playhead, and every pose made in the scene
view is keyed there. Without Animate, moving bones edits the rest pose the keys
are measured from, and that rest pose is what the scene file keeps. Drag keys
along their track, delete the selected ones, give them an easing, and play the
clip in the editor. **autoplay** in the Inspector names the clip the game
starts with.

## Example scenes

Choose **File → New Example Scene → Shapes** or **Physics**. These examples
are editable scenes; save one with **Save Scene As...** when it becomes the
starting point for a project.

## LLM help through MCP

The editor can serve its open scene to an LLM client over MCP. Turn it on with
**Tools → MCP Server**, or start the editor with `--mcp` (`--mcp=PORT` for
another port than 7777). It listens on `http://127.0.0.1:7777/mcp` only, and
the port shows in the status bar. With Claude Code:

    claude mcp add --transport http kinetix2d http://127.0.0.1:7777/mcp

The tools read the scene (`scene_info`, `scene_tree`, `get_object`), change it
(`set_property`, `create_object`, `add_component`, `delete_object`,
`reparent`, `select`, `undo`), use prefabs (`instantiate_prefab`,
`save_prefab`), run it (`run`: play, stop, pause, resume, step frames, or window to run the saved scene in the runner) and play
it (`input`: keys, mouse in game coordinates, buttons), look at it
(`screenshot` of the scene or game view, `console`, `profile`), and work on the project's
files (`list_assets`, `read_file`, `write_file`, `save_scene`, `build_atlas`) and the project
itself (`new_project`, `open_project`, `set_startup_scene`, `export_web`). Every change is
an undo step in the editor you are looking at, and nothing is saved until
`save_scene` or your own Save. Files outside the project are refused.

## Prefabs

Save an object and its children as a prefab with the prefab button of the
Hierarchy, or **Save as Prefab...** in its menu. Drag a `.k2dprefab` from Assets
onto the scene view to place it there, onto a row of the Hierarchy to put it
under that object, or double-click it to place it in the middle of the view.
