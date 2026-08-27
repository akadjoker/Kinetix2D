Tools, files and CI
===================

The build produces three executables in ``bin/`` next to the source tree:
``k2d_editor``, ``k2d_runner`` and ``k2d_pack``.

The editor (``k2d_editor``)
---------------------------

The editor is an ImGui application over the real ``k2d::Scene`` — there is one
scene model and the editor edits it directly. Its source lives in
``editor/src`` (``core/``, ``panels/``, ``widgets/``).

* Dockable panels: Hierarchy, Inspector, Assets, Console, Game, the 2D Scene
  viewport, Scripts, Settings, Prefabs, Animator, Particle, Sprite/Image,
  TileMap and a script editor.
* The toolbar handles scene files, undo/redo and Play/Pause/Step/Stop.
  Undo/redo records scene edits as ``ct::Json`` snapshots.
* Physics and scripts run only in Play. Play serializes the editable scene into
  a runtime copy, so simulation never touches the level being edited.
* Level design (gravity) lives on the scene; engine tuning (timestep,
  iterations) lives on the project.
* ``File → New Example Scene → Shapes / Physics`` creates editable starting
  scenes.
* Themes (Radion Dark, Light, Blender, Nord, Ember) come from
  ``View → Theme``.

The runner (``k2d_runner``)
---------------------------

A standalone player that loads a scene and runs it with a window, audio,
input actions and the zen runtime:

.. code-block:: text

   k2d_runner scene.k2dscene [project.k2dproj]

Physics tuning is read from the ``physics`` section of the project file:
``gravity``, ``fixedTimeStep``, ``velocityIterations`` and ``treeBroadphase``.
The runner maps the default input actions (``move_forward``, ``move_backward``,
``turn_left``, ``turn_right``, ``primary``, ``secondary``) and shows a
profiler overlay on F5. Touch controls are disabled by default, including in a
Web export. Enable them only for scenes that need them with, for example,
``"virtualPad": { "enabled": true }`` in the scene JSON.

The packer (``k2d_pack``)
-------------------------

Builds and inspects ``.kpak`` archives. Entries stay compressed on disk and
only the requested one is decoded; the archive can be encrypted with a project
key.

.. code-block:: text

   k2d_pack -o game.kpak [-k key] [-p prefix] [-l 0-9] <path>...
   k2d_pack -t game.kpak [-k key]

* ``-o`` — archive to create; each path is stored relative to its root.
* ``-p`` — archive prefix for the following roots (e.g. ``-p textures
  assets/textures``).
* ``-k`` — encrypt with a key.
* ``-l`` — deflate level (default 9; 0 stores raw).
* ``-t`` — list an archive.

Mounted packages take precedence over loose files:

.. code-block:: cpp

   k2d::FileSystem::Instance().MountPack("game.kpak", "project-key");

Files
-----

* ``.k2dscene`` — a scene, serialized as ``ct::Json`` by ``k2d::Serializer``.
* ``.k2dprefab`` — a single object graph saved with ``k2d::Prefab``. A prefab
  carries its components, including the Zen Script path and overrides.
* ``project.k2dproj`` — the project file. It holds the engine tuning that is
  not level design: the ``physics`` section (gravity, fixed timestep, solver
  iterations, broadphase) is read by the runner and the editor.
* ``.kpak`` — indexed asset packages built with ``k2d_pack``.

Tests
-----

Headless test executables are written to ``bin/``. Run them from inside
``bin/`` — some tests open assets by relative path. The naming convention is
``kx_*`` for physics, ``k2d_*`` for the engine and ``k2d_zen*`` /
``k2d_physics2d*`` for scripting and the physics component integration.

Examples: ``k2d_scene_tests``, ``k2d_serializer_tests``, ``k2d_kpak_tests``,
``k2d_camera_tests``, ``k2d_material_tests``, ``k2d_animation_tests``,
``k2d_astar_tests``, ``k2d_navigation_tests``, ``k2d_zen_tests``,
``k2d_zen_class_tests``, ``k2d_physics2d_tests``, ``k2d_zen_physics_tests``,
``kx_shapecast_tests``, ``kx_queries_tests``, ``kx_solver_tests``,
``kx_tilemap_tests`` and the fidelity suites that compare ``kx`` against the
reference collision code. Tests exit 0 on success.

Continuous releases
-------------------

GitHub Actions (``.github/workflows/release.yml``) builds ``k2d_editor``,
``k2d_runner`` and ``k2d_pack`` for linux-x64 and windows-x64. Pushes and pull
requests to ``main`` publish CI artifacts; pushing a version tag publishes both
ZIPs as a GitHub Release.
