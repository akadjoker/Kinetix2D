C++ API
=======

The C++ side is split into libraries, each with a clean dependency direction:

.. list-table::
   :widths: 25 75
   :header-rows: 1

   * - Library
     - What it is
   * - ``kx`` (``physics/``)
     - The physics core: collision, solver, joints, broadphase, queries,
       shapecast, triangulation. It has no engine coupling and uses only
       ``Math`` (mathc) and the ``ct`` containers.
   * - ``k2d`` (``engine/``)
     - The engine: window and input (``Device``), scene/component system,
       renderers, audio, UI controls, particles, animation, tilemaps, A*,
       navigation, serialization and the ``.kpak`` file system.
   * - ``k2d_physics2d`` (``physics2d/``)
     - The physics-as-components layer: ``RigidBody2D``, the colliders and
       ``PhysicsWorld2D``. It wraps ``kx`` for scene use.
   * - ``k2d_zen`` (``scripting/``)
     - The zen VM integration: ``ZenScriptComponent``, the script class cache,
       the blackboard and the script event dispatch.

The public headers live under ``engine/include/k2d/``,
``physics/include/kx/``, ``physics2d/include/k2d/`` and
``scripting/include/k2d/``. The umbrella header ``engine/include/k2d/k2d.h``
includes the whole engine.

House rules that matter when reading the API: C++14, ``ct`` containers only,
raw pointers with explicit ownership, and one transform representation
(``k2d::Matrix2D``) everywhere in physics and rendering.

.. toctree::
   :maxdepth: 2

   engine
   physics
   tools
