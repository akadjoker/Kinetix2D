# Editor external dependencies

Dear ImGui itself and its SDL2/OpenGL backend are shared with the engine from
`../../external/imgui`. This directory only contains editor-specific add-ons.

`imgui_widgets/` contains the file dialog, console, property grid, splitter,
curve editor, gradient editor and sequencer copied from the Radion workspace.
The ImGuiAl widgets retain their MIT license in
`imgui_widgets/LICENSE-ImGuiAl.txt`.

The Radion-specific logging and icon-font dependencies were removed from the file
dialog so the add-ons remain self-contained.

`icons/` contains the Material Design icon definitions and compressed font used by
the new Radion editor. The font is merged into ImGui's base font before the first
frame, so editor toolbars use icons rather than text placeholders.
