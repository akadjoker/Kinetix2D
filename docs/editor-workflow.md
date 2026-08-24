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
