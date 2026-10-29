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
do their errors, logged as errors.

.. warning::

   Quitting the editor does not ask about unsaved scripts: save them first.
