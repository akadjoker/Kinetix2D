Editor
======

The editor (``k2d_editor``) edits the same scene the game plays. A scene is a
tree of **objects**. An object has a name and a transform, and does nothing
on its own: what it shows and does comes from its **components** (a Sprite, a
RigidBody2D, a script…). Every tool of the editor makes objects, gives them
components, or edits what a component holds.

.. toctree::
   :maxdepth: 2

   objects
   scene-view
   sprites
   images
   tiles
   animation
   particles
   skeletons
   scripts

Start a project
---------------

**File → New Project...** asks for an empty folder and a name, and makes:

.. code-block:: text

   project.k2dproj
   scenes/main.k2dscene        the startup scene
   assets/scripts/player.py    moves the Player with WASD or the arrows
   assets/prefabs/
   assets/textures/

The scene has a *Player* (a RectShape with that script), so **Play** works at
once. **Open Project...** opens a ``project.k2dproj``, and the editor opens
the last project again when it starts with no arguments:

.. code-block:: text

   k2d_editor [scene.k2dscene | project.k2dproj] [--mcp[=port]]

**File → Set as Startup Scene** makes the open scene the one the game starts
with. **New Scene** (Ctrl+N) starts a scene holding one object, *Camera*,
with the active camera.

The window
----------

.. list-table::
   :widths: 22 78

   * - Left rail
     - **Hierarchy** (the objects of the scene) and **Assets** (the
       project's files).
   * - Right rail
     - **Inspector** (the selected object and its components),
       **Particles** (a dot marks it when the selected object has a
       Particle component) and **Settings**.
   * - Centre
     - **Scene**, **Game**, **Sprite Editor**, **Atlas**, **Script Editor**
       and **Image Editor**.
   * - Bottom
     - **Console**, **Animator**, **Skeleton**, **Profiler** and **Tile
       Painter**.

**View → Reset Layout** puts the panels back where they started. The status
bar shows the scene's name (``*`` when it has unsaved changes) and the frame
rate.

Play
----

**Play** (F5) copies the scene, runs scripts and physics on the copy and shows
the **Game** tab. **Stop** (F5 again) throws the copy away: whatever changed
in Play, by hand or by a script, is gone, and the scene is as it was. While
playing, edits, undo and drops are off. **Pause** freezes the game.

*Play in a window* (F6) runs the saved scene in the runner, in a window of its
own; the scene is saved first if it has changes. The combo on the right of
the run bar sets the size of the Game view: the project's screen, *Free*, or
a common resolution.

The **Console** shows the engine's log and the scripts' ``print``. Type
``clear`` in it to empty it.

Settings
--------

The **Settings** tab has three sections:

**Scene**
   ``own gravity`` gives this scene its own ``gravity`` in place of the
   project's (a level under water, on the moon); ``ambient`` is the light
   where no Light2D reaches. Saved with the scene.
**Project display**
   ``width`` and ``height`` of the game's screen: the runner's window and
   the frame drawn in the scene view.
**Project physics**
   ``gravity``, ``steps per s``, ``iterations``, ``tree broadphase``. Saved
   in ``project.k2dproj`` as soon as a value is changed.
**Collision layers**
   Names for the sixteen layers a collider belongs to and collides with
   (``player``, ``enemy``, ``ground``...). Saved with the project; the
   colliders' ``category`` and ``mask`` then show them by name.

Export
------

**File → Export Web** writes the game into ``<project>/export/web``
(``index.html``, ``data.kpak`` and the web runner); **Run Web** (F7) also
serves it on ``http://127.0.0.1:8090/`` and opens the browser. **Export
Desktop** writes ``<project>/export/desktop``, the runner named after the
project with ``data.kpak`` beside it; **Run Desktop** (F8) also starts it.

Shortcuts
---------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Keys
     - Action
   * - Ctrl+N / Ctrl+O
     - New scene / open a scene
   * - Ctrl+S / Ctrl+Shift+S
     - Save / save as. In the Image Editor or the Script Editor, saves what
       it has open.
   * - Ctrl+Z / Ctrl+Y, Ctrl+Shift+Z
     - Undo / redo (of the Image Editor when it has the focus)
   * - Ctrl+X / Ctrl+C / Ctrl+V
     - Cut, copy, paste objects, through the system clipboard
   * - Ctrl+D
     - Duplicate the selected objects
   * - Delete
     - Delete the selected objects
   * - Q / W / E / R
     - Select, Move, Rotate, Scale tool
   * - F
     - Centre the view on the selected object
   * - F5 / F6
     - Play or stop / play in a window
   * - F7 / F8
     - Run Web / Run Desktop
   * - F9
     - Debug in a window, or continue from a breakpoint. F10, F11 and
       Shift+F11 step (see :doc:`scripts`)
   * - F1
     - The editor manual, in the browser (**Help** menu)
   * - Ctrl+Q
     - Quit

The single keys (Delete, Q, W, E, R, F) are ignored while typing in a field,
and while the game is playing in the Game view.

Help from an LLM
----------------

**Tools → MCP Server (127.0.0.1:7777)**, or ``--mcp`` on the command line,
serves the open scene to an MCP client on ``http://127.0.0.1:7777/mcp``:

.. code-block:: text

   claude mcp add --transport http kinetix2d http://127.0.0.1:7777/mcp

Its tools read and change the scene, run it, take screenshots and work on the
project's files. Every change is an undo step in the editor, and nothing is
saved until you save. ``breakpoint`` and ``debug`` drive the script debugger:
set breakpoints, start a debug session, step, and read where the game stopped
with its locals.
