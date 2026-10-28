# Editor quick workflow

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
(`set_property`, `create_object`, `add_component`, `delete_object`, `undo`),
run it (`run`: play, stop, pause, resume, step frames), look at it
(`screenshot` of the scene or game view, `console`), and work on the project's
files (`list_assets`, `read_file`, `write_file`, `save_scene`). Every change is
an undo step in the editor you are looking at, and nothing is saved until
`save_scene` or your own Save. Files outside the project are refused.
