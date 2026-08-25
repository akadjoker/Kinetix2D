#Zen Scripting

Python-syntax scripting for Kinetix2D, powered by the vendored `libzen` VM (`external/zen`).

Add a **Zen Script** component to a GameObject, drag a `.py` file onto it from the Assets
panel, and hit Play. Scripts do not run in edit mode.

## Script contract

A script file defines **one class**. It is compiled **once per file**, no matter how many
objects use it;
each object gets its own cheap instance with its own state
    .

```python class Bullet(ScriptComponent)
    :
#self.node is assigned by the editor before this runs.
      def __init__(self)
    : #called when the instance is created self
    .speed = 400

    def on_start(self)
    : #called once,
on the first frame pass

    def on_update(self, dt)
    : #called every frame, dt in seconds self.node.translate(self.speed * dt, 0)

                               def on_draw(self)
    : #called every frame while the object is visible pass

      def on_draw_ui(self)
    : #optional overlay pass,
      always above world sprites pass

          def on_destroy(self)
    : #called when the component goes away pass

      def on_event(self, name, value)
    : #called when a script or C++ emits an event pass
```

`ScriptComponent` provides `self.node`,
      the GameObject hosting this script. `__init__` is optional.Instance fields(`self.speed`) are per object,
      so 500 bullets sharing one file each keep their own state.Module - level constants are shared
                                                                             .

                                                                         Older scripts are still supported
    : `class Bullet :` with
`def __init__(self, node)
    : self.node = node` keeps working.

                  Cost : one compile per file(~0.2 ms),
      then ~0.0005 ms per spawned object.

          ##Drawing from scripts

          Draw calls are global and can only be made inside `on_draw()` or `on_draw_ui()`.
`on_draw()` renders in world order at the script owner's Z index. `on_draw_ui()` is an overlay pass after all world
                                                                           sprites,
      suited to HUD counters and text.Coordinates remain in world
          / screen coordinates defined by the active camera. `set_draw_color` uses normalized RGB(A) values from `0` to `1` and remains active until the next call to it.

```python
class DrawDemo(ScriptComponent):
    def on_draw(self):
        set_draw_color(0.15, 0.7, 1.0)
        draw_rect(40, 40, 160, 72, True)

        set_draw_color(1.0, 0.85, 0.2)
        draw_circle(280, 76, 32, False, 32, 3)
        draw_line(20, 150, 340, 180, 2)
        draw_text(48, 64, "Zen draw API", 16)
```

| Function | Arguments |
|---|---|
| `set_draw_color` | `r, g, b, a=1` |
| `draw_line` | `x1, y1, x2, y2, thickness=1` |
| `draw_rect` | `x, y, width, height, fill=True, thickness=1` |
| `draw_circle` | `x, y, radius, fill=True, segments=32, thickness=1` |
| `draw_text` | `x, y, text, size=16` |
| `draw_text_width` | `text, size=16` → width in world units |

`draw_rect` and `draw_circle` use their final `thickness` argument only when `fill=False`.
Calling `draw_*` in `on_update` has no effect, because there is no active render queue then.

## Properties in the Inspector

A field the class body gives a value to becomes a property the Inspector can tune per object.
This is the way to write one:

```python
class Player(ScriptComponent):
    speed = 200         # exported, default 200 (whole number)
    jump = 380.5        # exported, default 380.5
    tag = "hero"        # exported, default "hero"
    armed = True        # exported, default True
    _phase = 0.0        # leading underscore, not exported

    def on_update(self, dt):
        self.node.translate(self.speed * dt, 0)
```

Nothing is parsed here: the compiler records those defaults on the class and the editor reads
the name, the value and the type straight off it.

A constructor writes its fields on the instance instead, so there is nothing on the class to
read and the source has to be scanned. That path still works, for scripts written before the
class body accepted fields:

```python
SPEED = 200

class Player(ScriptComponent):
    def __init__(self):
        self.speed = SPEED      # exported through the source scan
        self._timer = 0.0       # leading underscore, not exported
```

Only literals are read there — numbers, strings, `True`/`False`, or a module-level constant
holding one of those. Anything else is skipped rather than guessed at. When a name appears in
both places the class body wins, since that is the declaration and `__init__` is just code.

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
script->clearOverride("speed");  // back to the script default
script->declaredPropertyCount(); // what the .py declares
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
| Components | `get_component<T>()` for `ScriptComponent`, `Sprite`, `Animation`, `Camera`, `Particle`, `RigidBody`, `CharacterBody`, `Collider`, `BoxCollider`, `CircleCollider`, `EdgeCollider`, `PolygonCollider`, `ChainCollider`, `TileMap`, `SpriteBatch`, `Polygon2D`, `Line2D`, `NinePatch`, `Light`, `Light2D`, `DirectionalLight2D`, `LightOccluder`, `AudioPlayer`, `CircleShape`, `RectShape`, `CapsuleShape`, `UiCanvas`, `Panel`, `Label`, `Button`, `CheckBox`, `Slider`, `NavigationRegion`, `NavigationAgent`, `MotionTween`, and `MotionStreak`; plus the legacy component getters |

Component handles return `None` when the component is missing.

Every component handle provides `is_active()` and `set_active(value)`. Components with
component-specific script APIs expose those methods on the handle as well; for example,
`CharacterBody` provides velocity and slide-state accessors, `Collider` provides offset,
sensor and filter accessors, and `Panel`/`Label` provide their UI setters and getters.

`get_component<T>()` is the generic form of the component accessors. `T` must be one of the
component handle classes listed above. A node owns its transform directly, so use the node's
transform methods (`get_position()`, `set_position()`, and so on) instead of
`get_component<Transform>()`.

- **Sprite**: `set_color(r, g, b, a)`, `set_flip(x, y)`, `set_size(w, h)`,
  `set_water_enabled(b)`, `set_water_flow(ax, ay, bx, by)`, `set_water_strength(value)`.
  Water uses the sprite's normal map twice; assign one in the Inspector first.
- **Animation**: `play(clip)`, `stop()`, `is_playing()`, `current()`
- **Camera**: `start_shake(x, y, frequency, cycles)`, `stop_shake()`,
  `set_trauma_profile(x, y, frequency, decay)`, `add_trauma(amount)`,
  `clear_trauma()`, `start_zoom_punch(amount, duration)`, `stop_zoom_punch()`,
  `is_shaking()`. Shake amplitudes are screen pixels and never alter the camera's saved offset.
- **Particle**: `start()`, `stop()`, `reset()`, `burst(count)`, `is_playing()`
- **RigidBody**: `get_velocity()`, `set_velocity(x, y)`, `get_angular_velocity()`,
  `set_angular_velocity(deg)`, `apply_force(x, y)`, `apply_impulse(x, y)`, `apply_torque(t)`,
  `get_gravity_scale()`, `set_gravity_scale(s)`, `set_type("static"/"kinematic"/"dynamic")`,
  `is_awake()`, `wake()`
- **Button**: `clicked()` (returns `True` once per click), `set_text(text)`
- **CheckBox**: `is_checked()`, `set_checked(value)`, `changed()` (returns `True` once per toggle)
- **Slider**: `get_value()`, `set_value(value)`, `changed()` (returns `True` once per drag change)

## Retained UI

Create a `UiCanvas` in the editor and add child nodes with `Panel`, `Label`, `Button`,
`CheckBox`, or `Slider`. Layout uses normalized anchors plus pixel offsets, so the UI is
screen-space and ignores the world camera. The default button skin comes from the engine-owned
embedded `menu.png`; a project cannot accidentally lose it by omitting an asset.

```python
class Menu(ScriptComponent):
    def on_start(self):
        self.play = self.node.find("Play Button").get_button()

    def on_update(self, dt):
        if self.play.clicked():
            emit("start_game")
```

`on_draw_ui()` remains the right tool for one-off HUD/debug drawing. Use retained UI controls
when the editor needs to save layout and interaction state in the scene.

## Audio

Audio is initialized by the runner and editor. Load SFX and music through the same asset search
paths used by textures;
SFX can overlap, while starting music replaces the previous music voice.

```python
class AudioDemo(ScriptComponent):
    def on_start(self):
        self.click = audio_load("audio/click.ogg")
        self.music = audio_load_music("audio/theme.ogg")
        audio_play_music(self.music, True, 0.6)

    def on_update(self, dt):
        if key_pressed(KEY_SPACE):
            audio_play(self.click, 0.8, 1.0, 0.0)
```

`audio_play(sound, volume=1, pitch=1, pan=0)` returns a voice ID. Use `audio_stop`,
`audio_pause`, `audio_resume`, and `audio_playing` with that ID. `audio_stop_all`,
`audio_stop_music`, `audio_set_master_volume`, `audio_set_sfx_volume`, and
`audio_set_music_volume` control the engine-wide mixer. Volume and mute preferences are persisted
by the runner/editor. `audio_set_master_muted`, `audio_set_sfx_muted`, and
`audio_set_music_muted` retain their respective volume values.

Use `audio_fade_in(voice, seconds)`, `audio_fade_out(voice, seconds, stop=True)`, or
`audio_crossfade_music(music, loop=True, volume=1, seconds=1)` for transitions. For positional
SFX, `audio_play_at(sound, x, y, volume=1, pitch=1, min_distance=64, max_distance=1024)` uses the
active camera as its listener; `audio_set_listener_position(x, y)` overrides it when required.

## Physics

`get_body()` returns `None` unless the object has a Rigid Body component, and the body only
exists while playing. Impulses divide by mass, so a 40x40 box at density 1 has mass 1600 and
`apply_impulse(0, -8000)` changes its velocity by 5 units per second, not 8000.

```python
def on_update(self, dt):
    body = self.node.get_body()
    if key_pressed(KEY_SPACE):
        body.apply_impulse(0, -160000)
```

**Contacts** reach `on_collision`, on the scripts of the object that was touched:

```python
def on_collision(self, other, began):   # began is True on enter, False on exit
    if began and other.get_name() == "spike":
        emit("player_hurt", 1)
```

Sensors report through the same hook without blocking, so a trigger volume is a collider with
Sensor ticked plus an `on_collision`.

**Queries** work on the running world:

```python
hit, hx, hy = raycast(x, y, dx, dy, distance)   # hit is a Node or None
node = body_at(x, y)                            # what is under this point
set_gravity(0, 0)                               # retune the live world
gx, gy = get_gravity()
```

### Character movement

Add a `RigidBody2D`, one or more `Collider2D` components, and a `CharacterBody2D` to the
same Node. `CharacterBody2D` sets that rigid body to **Kinematic** and uses the engine's
Box2D-derived convex shape cast;
it never moves the object one pixel at a time.

    Every placement or
    movement query reads the node's own colliders and their collision layers / masks.No shape or
    mask is passed from ZenPy
        .

```python
#Non - mutating GameMaker - style queries.Coordinates use this Node's position space.
    if self.node.place_free(next_x, next_y)
    : pass wall = self.node.place_meeting(next_x, next_y) #Node or None

#Moves until the first contact.A miss returns None, 0, 0, 0, 0.
                  other,
           hit_x, hit_y, normal_x, normal_y = self.node
                                                  .move_and_collide(dx, dy)

#Either set velocity separately(Godot - style)...
                                                      self.node.set_character_velocity(vx, vy) hit,
           vx, vy, on_floor, on_wall, on_ceiling = self.node.move_and_slide()

#... or set it as part of the call.
                                                       hit,
           vx, vy, on_floor, on_wall, on_ceiling = self.node.move_and_slide(vx, vy)

#A slide can contain more than one impact.
                                                       count = self.node.slide_collision_count() other,
           hit_x, hit_y, normal_x,
           normal_y = self.node.slide_collision(0)
```

`move_and_collide(dx, dy)` is the general primitive : it stops at the first obstacle
                      and lets the script choose the response. `move_and_slide()` uses the component velocity
                      and projects the remaining motion and velocity along every hit normal,
           up to `maxSlides`.It returns the updated velocity as multiple ZenPy values
               .

           ##Prefabs

           A prefab carries its Zen Script like any other component
    : the `.py` path and the property overrides travel inside the `.k2dprefab`,
           so every instance comes back wired and tuned
               .One compile serves the whole flock — 50 balls off the same prefab still cost a single compile,
           and each instance can then be retuned on its own.

```cpp Prefab prefab;
prefab.Load("assets/prefabs/ball.k2dprefab");
GameObject* ball = prefab.Instantiate(scene); // script attached, overrides applied
```

From a script, `spawn()` does the same thing:

```python
bullet = self.node.spawn("assets/prefabs/bullet.k2dprefab", self.node.get_x(), self.node.get_y())
```

When the spawned prefab uses a `.py` that has not been compiled yet, the compile cannot happen
right there — the VM is in the middle of running your script, and `VM::run` reuses the main
fiber, so compiling on top of it would corrupt the caller. The component takes the path and
compiles on the next frame instead;
`loaded()` is false and `pendingLoad()` is true in between,
    and the object starts running one frame later.Prefabs whose script is already in the cache — the normal case,
    after the first spawn — start on the same frame.

    ##Talking to other scripts and to C++

    Two mechanisms,
    both global to the running scene.

            ** Blackboard** — shared key
        / value state:

```python
set_number("score", 100)      get_number("score", 0)
set_string("stage", "boss")   get_string("stage", "")
set_flag("alive", True)       get_flag("alive", False)
has_key("score")
```

**Persistent user data** — values saved outside the project, in the writable
per-user folder selected by SDL for the editor/runner. It is loaded automatically
at startup;
call `user_data_save()` after changing values:

```python
coins = user_data_get_int("coins", 0)
user_data_set_int("coins", coins + 1)
user_data_set_bool("music", True)
user_data_save()
```

Available typed functions are `user_data_get/set_int`, `user_data_get/set_float`,
`user_data_get/set_string`, and `user_data_get/set_bool`;
`user_data_has`,
`user_data_delete`, and `user_data_clear` manage keys. `user_data_load()` and
`user_data_save()` use `settings.json` by default, or accept a safe file name such as `"profile_2.json"`
                                                                .For separate text files use
`user_data_read_text("notes.txt", "")` and `user_data_write_text("notes.txt", text)`.

                                                                    **Events ** — fire
                                                            - and-forget broadcast,
    delivered to every script's `on_event`:

```python emit("enemy_killed", 10)
```

    Events are queued and delivered once per frame,
    after `update`.From C++ :

```cpp ZenBlackboard::setNumber("hp", 75);    // scripts read it with get_number
ZenBlackboard::emit("player_died");            // reaches every on_event
BroadcastZenScriptEvent(scene.root(), "boss"); // immediate, skips the queue
ZenBlackboard::setHostHandler(fn, user);       // C++ sees every emit() from scripts
script->callFunction("reset");                 // call a named script function directly
```

    ##Host
    setup(already wired in the editor)

```cpp RegisterZenScriptSerializer(); // makes the component save/load with the scene
SetZenScriptInput(&device.GetInput());
SetZenScriptAssets(&assets);
SetZenScriptUserData(&userData); // optional: enables user_data_* in scripts
SetZenScriptOutput(fn, user);    // route print() into your console
SetZenScriptsEnabled(true);      // scripts idle until this is on
RouteZenScriptCollisions(world); // makes on_collision fire
// each frame, after scene.update():
DispatchZenScriptEvents(scene.root());
```

## Input

`key_down(key)`, `key_pressed(key)`, and `key_released(key)` take a numeric `KEY_*` constant, such
as `KEY_A`, `KEY_SPACE`, `KEY_LEFT`, `KEY_F5`, or `KEY_LEFT_CTRL`. They do no string conversion in
the VM. Direct keys are physical controls; use `action_down(name)`, `action_pressed(name)`, and
`action_released(name)` for remappable gameplay actions.
The runner maps `move_forward`, `move_backward`, `turn_left`,
`turn_right`, `primary`, and `secondary` to keyboard keys and their corresponding virtual-pad
buttons by default. C++ games can add or replace bindings through `GetInputActions().Bind()`.

Mouse: `mouse_down(button)`, `mouse_pressed(button)`, `mouse_x()`, `mouse_y()`, `wheel_y()`.
`get_fps()` returns the engine's rolling half-second FPS measurement; it is stable enough for HUDs,
unlike calculating `1 / dt` every frame.
`profiler_visible()` is true while the runner's F5 profiler overlay is open, so a game HUD can avoid
drawing duplicate diagnostics.
Inside the editor, the mouse position is relative to the Game view and clicks in the other editor
panels are ignored. `viewport_width()` and `viewport_height()` return the current Game view size.
For world-space gameplay, use `wx, wy = mouse_world_position()` or
`wx, wy = screen_to_world(x, y)`; both account for the active Camera2D. `world_view_rect()` returns
`left, top, right, bottom` for the camera's current visible world area.

## Scene transitions

`load_scene(path)` schedules a scene replacement after the current frame; it is safe to call from
an input callback or `on_update`. The scene is loaded using the normal asset paths and its physics
world is rebuilt before the next update.

## Bytecode for exports

Development keeps scripts as `.py`, so source reload remains available. Release exporters can compile
a script to Zen bytecode with:

```sh
k2d_scriptc assets/scripts/player.py exported/scripts/player.zbc
```

`ZenScriptComponent::loadFile()` recognises the `ZENBC` header and runs `.zbc` directly; it never
falls back to source. Bytecode requires the same Zen VM version and Kinetix native bindings that
generated it, so a Web export must be rebuilt after either changes. The Web exporter will compile
and preload the project scripts in a deterministic order before loading its first scene.

## Notes

- All scripts share one VM for the whole run. Module-level globals are therefore shared
  across scripts; keep per-object state in `self`, and use the blackboard for shared state.
- `ZenRuntime::instance().reset()` drops every cached class and instance (call it when
  tearing a game down);
components re - instantiate themselves on the next frame.- Reloading is per script,
    not per component : `ReloadChangedZenScripts()` walks the compiled classes,
    recompiles the files whose timestamp moved, and returns how many scripts it rebuilt.It is an explicit operation,
    never a per - frame file watcher : press `F6` while playing in the editor or
        in the standalone runner to request it.Only the objects running a rebuilt script re - instantiate,
    so editing one `.py` mid -
        play does not reset the state of every other script in the scene
            .A file that fails to compile keeps the last good class running rather than taking the object down with
                it.- `ZenRuntime::instance().recompile(path)` forces one script to rebuild,
    which is what the Inspector's Reload button does. -
        The script's file path and the property overrides are serialized, so the `.py` must be reachable at load time.-
        An override whose field disappears from the `.py` is kept but flagged in the Inspector,
    so a rename does not silently drop tuning.- `import math`, `import time`, `import json`, `import net`
        and `import http` are available;
  `print` goes to the editor Console.
- `import json` currently runs libzen's own parser (`builtin_json.cpp`), which is a second JSON
  implementation next to `ct::Json` — the one the serializer, the scenes and the project file
  already use. Two parsers in one binary means two behaviours on the edges: number formatting,
  escapes, duplicate keys. The plan is to back the module with `ct::Json` so there is one. The
  `import json` surface does not change when that happens.
- `io`, `os`, `path` and `struct` ship compiled in libzen but are **deliberately not
  registered**, so `import os` fails. Registering one is a single `vm.register_lib` call in
  `ZenScriptComponent.cpp`, but it is a decision, not an oversight: `io`, `os` and `path` hand
  game scripts the filesystem and the process, which is a wide door to open for content that a
  scene file can drag in. Register them when a game actually needs them.
