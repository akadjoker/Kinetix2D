# Zen Scripting

Python-syntax scripting for Kinetix2D, powered by the vendored `libzen` VM (`external/zen`).

Add a **Zen Script** component to a GameObject, drag a `.py` file onto it from the Assets
panel, and hit Play. Scripts do not run in edit mode.

## Script contract

A script file defines **one class**. It is compiled **once per file**, no matter how many
objects use it; each object gets its own cheap instance with its own state.

```python
class Bullet:
    def __init__(self, node):    # called when the instance is created
        self.node = node
        self.speed = 400

    def on_start(self):          # called once, on the first frame
        pass

    def on_update(self, dt):     # called every frame, dt in seconds
        self.node.translate(self.speed * dt, 0)

    def on_destroy(self):        # called when the component goes away
        pass

    def on_event(self, name, value):   # called when a script or C++ emits an event
        pass
```

Only `__init__` is required. Instance fields (`self.speed`) are per object, so 500 bullets
sharing one file each keep their own state. Module-level constants are shared.

Cost: one compile per file (~0.2 ms), then ~0.0005 ms per spawned object.

## Properties in the Inspector

Every `self.<name> = <literal>` in `__init__` becomes a property the Inspector can tune per
object. Literals are numbers, strings, `True`/`False`, or a module-level constant holding one
of those.

```python
SPEED = 200

class Player:
    def __init__(self, node):
        self.node = node        # not a literal, not exported
        self.speed = SPEED      # exported, default 200
        self.jump = 380.5       # exported, default 380.5
        self.tag = "hero"       # exported, default "hero"
        self.armed = True       # exported, default True
        self._timer = 0.0       # leading underscore, not exported
```

Two objects can share `player.py` and still run at different speeds. What the Inspector stores
is only the fields you actually change: the override travels with the scene, everything else
keeps following the `.py`. Editing the file changes the defaults for every object that has not
overridden them.

The value is written into the instance right after `__init__`, so `on_start` and `on_update`
already see it. Changing a property while playing retunes the live instance without restarting
it, and so does reverting one — the object keeps the rest of its state.

Integers stay integers and floats stay floats, which is why `self.lives = 3` gets a whole-number
widget and `self.jump = 380.5` gets a decimal one.

From C++:

```cpp
script->setNumberOverride("speed", 500.0, true);   // true = keep it an integer
script->setStringOverride("tag", "boss");
script->setBoolOverride("armed", false);
script->clearOverride("speed");                    // back to the script default
script->declaredPropertyCount();                   // what the .py declares
```

## Node methods

| Group | Methods |
|-------|---------|
| Identity | `get_name()` |
| Transform | `get_x()`, `get_y()`, `get_position()`, `set_position(x, y)`, `translate(dx, dy)`, `get_rotation()`, `set_rotation(deg)`, `rotate(deg)`, `get_scale_x()`, `get_scale_y()`, `set_scale(sx, sy)` |
| State | `set_visible(b)`, `is_visible()`, `set_active(b)`, `is_active()`, `set_z_index(z)`, `get_z_index()` |
| Tree | `get_parent()`, `child_count()`, `get_child(i)`, `find(name)`, `create_child(name)`, `queue_destroy()` |
| Spawning | `spawn(prefab_path)`, `spawn(prefab_path, x, y)` |
| Math | `distance_to(x, y)`, `angle_to(x, y)`, `look_at(x, y)`, `move_toward(x, y, max_step)` |
| Components | `get_sprite()`, `get_animation()`, `get_particle()` |

Component handles return `None` when the component is missing.

- **Sprite**: `set_color(r, g, b, a)`, `set_flip(x, y)`, `set_size(w, h)`
- **Animation**: `play(clip)`, `stop()`, `is_playing()`, `current()`
- **Particle**: `start()`, `stop()`, `reset()`, `burst(count)`, `is_playing()`

## Talking to other scripts and to C++

Two mechanisms, both global to the running scene.

**Blackboard** — shared key/value state:

```python
set_number("score", 100)      get_number("score", 0)
set_string("stage", "boss")   get_string("stage", "")
set_flag("alive", True)       get_flag("alive", False)
has_key("score")
```

**Events** — fire-and-forget broadcast, delivered to every script's `on_event`:

```python
emit("enemy_killed", 10)
```

Events are queued and delivered once per frame, after `update`. From C++:

```cpp
ZenBlackboard::setNumber("hp", 75);              // scripts read it with get_number
ZenBlackboard::emit("player_died");              // reaches every on_event
BroadcastZenScriptEvent(scene.root(), "boss");   // immediate, skips the queue
ZenBlackboard::setHostHandler(fn, user);         // C++ sees every emit() from scripts
script->callFunction("reset");                   // call a named script function directly
```

## Host setup (already wired in the editor)

```cpp
RegisterZenScriptSerializer();       // makes the component save/load with the scene
SetZenScriptInput(&device.GetInput());
SetZenScriptAssets(&assets);
SetZenScriptOutput(fn, user);        // route print() into your console
SetZenScriptsEnabled(true);          // scripts idle until this is on
// each frame, after scene.update():
DispatchZenScriptEvents(scene.root());
```

## Input

`key_down(name)`, `key_pressed(name)`, `key_released(name)` take `"a".."z"`, `"0".."9"`,
`"space"`, `"escape"`, `"enter"`, `"tab"`, `"backspace"`, arrows (`"left"`, `"right"`,
`"up"`, `"down"`), and modifiers (`"lshift"`, `"rshift"`, `"lctrl"`, `"rctrl"`, `"lalt"`).

Mouse: `mouse_down(button)`, `mouse_pressed(button)`, `mouse_x()`, `mouse_y()`, `wheel_y()`.

## Notes

- All scripts share one VM for the whole run. Module-level globals are therefore shared
  across scripts; keep per-object state in `self`, and use the blackboard for shared state.
- `ZenRuntime::instance().reset()` drops every cached class and instance (call it when
  tearing a game down); components re-instantiate themselves on the next frame.
- The script's file path and the property overrides are serialized, so the `.py` must be
  reachable at load time.
- An override whose field disappears from the `.py` is kept but flagged in the Inspector, so a
  rename does not silently drop tuning.
- `import math` and `import time` are available; `print` goes to the editor Console.
