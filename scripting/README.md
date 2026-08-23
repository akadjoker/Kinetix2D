# Zen Scripting

Python-syntax scripting for Kinetix2D, powered by the vendored `libzen` VM (`external/zen`).

Add a **Zen Script** component to a GameObject, drag a `.py` file onto it from the Assets
panel, and hit Play. Scripts do not run in edit mode.

## Script contract

```python
def ready(node):          # called once, on the first frame
    ...

def update(node, dt):     # called every frame, dt in seconds
    ...

def on_event(node, name, value):   # called when any script or C++ emits an event
    ...
```

All three are optional. `node` is the GameObject the component is attached to.

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

- Each component owns its own VM, so globals are private per script instance. Use the
  blackboard to share state.
- Only the script's file path is serialized, so the `.py` must be reachable at load time.
- `import math` and `import time` are available; `print` goes to the editor Console.
