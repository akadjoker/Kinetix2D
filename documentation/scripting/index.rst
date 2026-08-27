Scripting
=========

Kinetix2D scripts are Python-syntax files run by the vendored zen VM
(``external/zen``). A script is attached to a GameObject through the
:class:`ZenScriptComponent` component; the editor does this by adding a
**Zen Script** component and dragging a ``.py`` file onto it from the Assets
panel.

Scripts run **only in Play**. They are compiled once per file, no matter how
many objects use the file; each object gets its own cheap instance with its own
state.

.. toctree::
   :maxdepth: 2

   concepts
   reference
   stdlib

Contents
--------

* :doc:`concepts` — how a script file is structured, the lifecycle hooks,
  exported properties, hot reload, prefabs, events and the host setup.

* :doc:`reference` — the full script API: the exposed classes, the global
  functions and the key constants.

* :doc:`stdlib` — the zen standard library modules registered by Kinetix2D
  (``math``, ``time``, ``json``, ``net``, ``http``) and the always-open base
  globals.
