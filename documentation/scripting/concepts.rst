Scripting concepts
==================

How a script is structured
--------------------------

A script file defines **one class** derived from ``ScriptComponent``. The
editor sets the ``self.node`` field to the GameObject hosting the script before
anything else runs, so ``__init__`` can already use it.

.. code-block:: python

   class Bullet(ScriptComponent):
       speed = 400           # exported property, default 400

       def on_start(self):
           pass

       def on_update(self, dt):
           self.node.translate(self.speed * dt, 0)

       def on_draw(self):
           pass

       def on_draw_ui(self):
           pass

       def on_destroy(self):
           pass

       def on_event(self, name, value):
           pass

       def on_collision(self, other, began):
           pass

The class is compiled once per file. Every object holding that file gets its
own instance, so ``__init__`` and instance fields run per object while the
module-level code runs once.

The class method set
--------------------

``__init__`` (optional)
   Called when the instance is created, before the first frame. ``self.node``
   is already set. The constructor form ``def __init__(self, node)`` from older
   scripts is still accepted.

``on_start()`` (optional)
   Called once, on the first frame the script runs.

``on_update(dt)`` (optional)
   Called every frame with the frame delta time in seconds.

``on_draw()`` (optional)
   Called every frame while the owning object is visible, at the object's
   Z index. This is where the global draw calls are valid.

``on_draw_ui()`` (optional)
   A second pass that always runs above every world sprite. Draw calls here
   land in top-left screen coordinates (the world camera is cancelled for this
   item), which makes it the place for HUD counters and debug text.

``on_destroy()`` (optional)
   Called when the component goes away or the script is reloaded.

``on_event(name, value)`` (optional)
   Called for every event broadcast in the running scene (see
   :ref:`scripting-events`), and for ``Event`` actions fired by an Action
   Sequence on the same object. ``value`` is a number and defaults to 0.

``on_collision(other, began)`` (optional)
   Called on the scripts of the object that was touched. ``other`` is the Node
   that touched it, ``began`` is True on enter and False on exit. Sensors
   report through the same hook without blocking, so a trigger volume is a
   collider with *Sensor* ticked plus an ``on_collision``.

Drawing from scripts
--------------------

The draw functions are global and only take effect while ``on_draw()`` or
``on_draw_ui()`` is running, because only then is there an active render
queue. Calling them from ``on_update`` does nothing.

* ``on_draw`` renders in world order at the owner's Z index, in the world
  coordinates defined by the active camera.
* ``on_draw_ui`` is an overlay pass in top-left screen coordinates.

``set_draw_color(r, g, b, a=1)`` takes normalized RGB(A) values from 0 to 1 and
stays active until the next call.

.. code-block:: python

   class DrawDemo(ScriptComponent):
       def on_draw(self):
           set_draw_color(0.15, 0.7, 1.0)
           draw_rect(40, 40, 160, 72, True)

           set_draw_color(1.0, 0.85, 0.2)
           draw_circle(280, 76, 32, False, 32, 3)
           draw_line(20, 150, 340, 180, 2)
           draw_text(48, 64, "Zen draw API", 16)

Properties in the Inspector
---------------------------

A field the class body gives a value to becomes a property the Inspector can
tune per object. The compiler records the name, value and type straight off the
class, so nothing is parsed.

.. code-block:: python

   class Player(ScriptComponent):
       speed = 200         # exported, default 200 (whole number)
       jump = 380.5        # exported, default 380.5
       tag = "hero"        # exported, default "hero"
       armed = True        # exported, default True
       _phase = 0.0        # leading underscore, not exported

       def on_update(self, dt):
           self.node.translate(self.speed * dt, 0)

A constructor writes its fields on the instance instead, so there is nothing on
the class to read and the source has to be scanned. That path still works for
scripts written before the class body accepted fields:

.. code-block:: python

   SPEED = 200

   class Player(ScriptComponent):
       def __init__(self):
           self.speed = SPEED      # exported through the source scan
           self._timer = 0.0       # leading underscore, not exported

Only literals are read there: numbers, strings, ``True``/``False``, or a
module-level constant holding one of those. Anything else is skipped. When a
name appears in both places the class body wins, because that is the
declaration and ``__init__`` is just code.

The Inspector only stores the fields you actually change. The override travels
with the scene; everything else keeps following the ``.py``. Editing the file
changes the defaults for every object that has not overridden them.

The value is written into the instance right after ``__init__``, so
``on_start`` and ``on_update`` already see it. Changing a property while
playing retunes the live instance without restarting it, and reverting one
keeps the rest of the object's state.

Integers stay integers and floats stay floats, which is why ``self.lives = 3``
gets a whole-number widget and ``self.jump = 380.5`` gets a decimal one.

From C++ the same overrides are available:

.. code-block:: cpp

   script->setNumberOverride("speed", 500.0, true);   // true = keep it an integer
   script->setStringOverride("tag", "boss");
   script->setBoolOverride("armed", false);
   script->clearOverride("speed");                    // back to the script default

Hot reload
----------

While playing, the editor can watch ``.py`` files for changes. A changed file
is recompiled and every live object that uses it swaps to the new class on the
next frame, keeping its property overrides. If the new source fails to
compile, the previous class stays in place.

Prefabs
-------

A prefab carries its Zen Script like any other component: the ``.py`` path and
the property overrides travel inside the ``.k2dprefab``, so every instance
comes back wired and tuned. One compile serves the whole flock — 50 balls off
the same prefab still cost a single compile, and each instance can then be
retuned on its own.

From a script, ``Node.spawn()`` does the instantiation:

.. code-block:: python

   bullet = self.node.spawn("assets/prefabs/bullet.k2dprefab", self.node.get_x(), self.node.get_y())

When the spawned prefab uses a ``.py`` that has not been compiled yet, the
compile cannot happen right there — the VM is in the middle of running your
script, so compiling on top of it would corrupt the caller. The component
takes the path and compiles on the next frame instead; between the spawn and
that frame the component is in the "pending load" state and the object starts
running one frame later. Prefabs whose script is already in the cache start on
the same frame.

.. _scripting-events:

Talking to other scripts and to C++
-----------------------------------

Two mechanisms are global to the running scene.

**Blackboard** — shared key/value state:

.. code-block:: python

   set_number("score", 100)      get_number("score", 0)
   set_string("stage", "boss")   get_string("stage", "")
   set_flag("alive", True)       get_flag("alive", False)
   has_key("score")

**Persistent user data** — values saved outside the project, in the writable
per-user folder selected by SDL for the editor/runner. It is loaded
automatically at startup; call ``user_data_save()`` after changing values:

.. code-block:: python

   coins = user_data_get_int("coins", 0)
   user_data_set_int("coins", coins + 1)
   user_data_set_bool("music", True)
   user_data_save()

**Events** — fire-and-forget broadcast, delivered to every script's
``on_event``:

.. code-block:: python

   emit("enemy_killed", 10)

Events are queued and delivered once per frame, after ``update``. From C++:

.. code-block:: cpp

   ZenBlackboard::setNumber("hp", 75);    // scripts read it with get_number
   ZenBlackboard::emit("player_died");    // reaches every on_event
   BroadcastZenScriptEvent(scene.root(), "boss"); // immediate, skips the queue
   ZenBlackboard::setHostHandler(fn, user);       // C++ sees every emit() from scripts
   script->callFunction("reset");         // call a named script function directly

Host setup
----------

The runtime is wired from C++ by the runner and the editor. A minimal host:

.. code-block:: cpp

   RegisterZenScriptSerializer(); // makes the component save/load with the scene
   SetZenScriptInput(&device.GetInput());
   SetZenScriptAssets(&assets);
   SetZenScriptUserData(&userData); // optional: enables user_data_* in scripts
   SetZenScriptOutput(fn, user);    // route print() into your console
   SetZenScriptsEnabled(true);      // scripts idle until this is on
   RouteZenScriptCollisions(world); // makes on_collision fire
   // each frame, after scene.update():
   DispatchZenScriptEvents(scene.root());
