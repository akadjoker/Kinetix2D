Scripts
=======

Behaviour comes from **ZenScript** components: a Python-syntax file whose
class derives from ``ScriptComponent``. The language and the API are in
:doc:`../scripting/index`; this page is about the editor's side.

Make a script
-------------

In the **Script Editor** tab, press **New script**. It asks for a name
(``new_script.py`` in ``assets/scripts`` by default) and writes:

.. code-block:: python

   class NewScript(ScriptComponent):
       speed = 100

       def on_start(self):
           pass

       def on_update(self, dt):
           pass

Put it on an object
-------------------

* Drag the script from **Assets** onto an object in the scene view, or onto
  its row in the Hierarchy; or
* drag it onto empty space in the scene view (or below the Hierarchy's rows)
  to make a new object holding it.

The Inspector's **Script** section then shows the class's variables, and
those ``__init__`` sets. Values typed there belong to this object; set one
back to the script's value and the object follows the script again. A
variable whose line ends in a range comment gets a slider:

.. code-block:: python

   speed = 200  # range(0, 800)
   jump = 1.5   # range(0, 4, 0.1)

**Open in the Script Editor** opens the file. To use another file, remove the
component and drop the other script.

The Script Editor
-----------------

Double-click a script in Assets, use **Open in the Script Editor**, or drop a
script on the tab. Several files stay open; the combo switches between them
and marks the changed ones with ``*``.

* **Save the script** (Ctrl+S, with the editor focused).
* **Reload the script from disk** drops unsaved changes.
* **Find and replace**.
* Folding, indent guides, bracket matching, a minimap and Python colouring.

Saving while the game plays reloads the changed script in the running game.

Scripts run only in **Play**. Their ``print`` goes to the **Console**, and so
do their errors, logged as errors. Quitting, or opening another project, asks
about the unsaved scripts first.

The debugger
------------

Click in the Script Editor's margin, between the line numbers and the fold
arrows, to put a breakpoint (a red dot) on a line; click again to take it off.
Breakpoints move with their lines as you type, and each open script keeps its
own.

**Debug in a window** (F9, on the run bar beside *Play in a window*) saves the
scene and runs it in ``k2d_runner_debug``, a runner built with the VM's debug
hook. The game waits for the breakpoints before its first frame, so one in
``on_start`` stops too. When a script reaches a breakpoint the game freezes:

* the Script Editor opens the file with the line marked by an arrow;
* the **Debugger** tab shows the call stack, innermost first, and the locals
  of the selected frame with their values. Click a frame to see its line and
  its locals. A local appears once it has a value.

Then, from the Debugger tab's toolbar or the keys:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Keys
     - Action
   * - F9
     - Continue: run until the next breakpoint
   * - F10
     - Step over: the next line in this function, running calls whole
   * - F11
     - Step in: into the function this line calls
   * - Shift+F11
     - Step out: run to the line that called this function
   * - (toolbar)
     - Pause: stop at the next line any script runs, even one stuck in a
       loop; Stop: close the game

Breakpoints set or cleared while the game runs take effect at once. Closing
the game window ends the session.

.. note::

   Only ``k2d_runner_debug`` has the hook. *Play*, *Play in a window* and every
   export run the VM without it, at full speed: a breakpoint costs nothing
   outside a debug session.
