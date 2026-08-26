Script API reference
====================

This page lists the script API exactly as the bindings register it. Methods
that return several values return them in order, like the examples show.
Component handles (``get_sprite()`` and friends) return ``None`` when the
component is missing.

.. contents::
   :local:

The ``ScriptComponent`` class
-----------------------------

A script class derives from ``ScriptComponent``. The instance has a ``node``
field set by the engine before ``__init__`` runs.

.. py:method:: __init__()

   Called when the instance is created. Optional. ``self.node`` is already set.
   The older form ``def __init__(self, node)`` is still supported.

.. py:method:: on_start()

   Called once, on the first frame.

.. py:method:: on_update(dt)

   Called every frame with the frame delta time in seconds.

.. py:method:: on_draw()

   Called every frame while the object is visible, at the object's Z index.
   World coordinates defined by the active camera.

.. py:method:: on_draw_ui()

   Overlay pass above every world sprite, in top-left screen coordinates.

.. py:method:: on_destroy()

   Called when the component goes away.

.. py:method:: on_event(name, value)

   Called for every broadcast event. ``value`` is a number, 0 by default.

.. py:method:: on_collision(other, began)

   Called on the scripts of the object that was touched. ``other`` is the Node
   that touched it; ``began`` is True on enter, False on exit.

The ``Node`` class
------------------

``Node`` wraps a GameObject. Every script instance has one as ``self.node``.

.. py:method:: Node.get_name()

   Returns the object name as a string.

.. py:method:: Node.get_x()

.. py:method:: Node.get_y()

   Returns the local position component as a float.

.. py:method:: Node.get_position()

   Returns ``(x, y)``.

.. py:method:: Node.set_position(x, y)

   Sets the local position.

.. py:method:: Node.translate(dx, dy)

   Moves the object by an offset relative to its current position.

.. py:method:: Node.get_rotation()

   Returns the rotation in degrees.

.. py:method:: Node.set_rotation(deg)

.. py:method:: Node.rotate(deg)

   Sets / adds rotation, in degrees.

.. py:method:: Node.get_scale_x()

.. py:method:: Node.get_scale_y()

   Returns the local scale component.

.. py:method:: Node.set_scale(sx, sy)

.. py:method:: Node.set_visible(b)

.. py:method:: Node.is_visible()

   Sets / queries the visibility flag.

.. py:method:: Node.set_active(b)

.. py:method:: Node.is_active()

   Sets / queries the active flag. Inactive objects do not update.

.. py:method:: Node.set_z_index(z)

.. py:method:: Node.get_z_index()

   Sets / queries the render order within the parent.

.. py:method:: Node.queue_destroy()

   Marks the object for deletion at the end of the frame.

.. py:method:: Node.get_parent()

   Returns the parent ``Node``, or ``None``.

.. py:method:: Node.child_count()

   Returns the number of direct children.

.. py:method:: Node.get_child(index)

   Returns the child at ``index`` as a ``Node``, or ``None``.

.. py:method:: Node.find(name)

   Searches the scene for an object by name and returns it as a ``Node``, or
   ``None``.

.. py:method:: Node.create_child(name)

   Creates a child object in the scene with the given name and returns it as a
   ``Node``.

.. py:method:: Node.spawn(prefab_path)
               Node.spawn(prefab_path, x, y)

   Instantiates a prefab into the scene. With three arguments the new object is
   positioned at ``(x, y)``. Returns the new ``Node`` or ``None``. When the
   prefab's script is not yet compiled, the object starts running one frame
   later.

.. py:method:: Node.distance_to(x, y)

   Returns the distance from the object's global position to ``(x, y)``.

.. py:method:: Node.angle_to(x, y)

   Returns the angle from the object's global position to ``(x, y)``, in
   degrees.

.. py:method:: Node.look_at(x, y)

   Rotates the object to face ``(x, y)``.

.. py:method:: Node.move_toward(x, y, max_step)

   Moves toward ``(x, y)`` by at most ``max_step`` pixels. Returns the new
   position as ``(x, y)``.

.. py:method:: Node.get_body()

   Returns the object's ``RigidBody``, or ``None``. The body only exists while
   playing.

.. py:method:: Node.place_free(x, y)

   Returns True if the object's colliders do not overlap anything at ``(x, y)``.
   Requires a CharacterBody2D on the object.

.. py:method:: Node.place_meeting(x, y)

   Returns the first ``Node`` the object's colliders overlap at ``(x, y)``, or
   ``None``.

.. py:method:: Node.move_and_collide(dx, dy)

   Moves the object until the first contact. Returns
   ``(other, hit_x, hit_y, normal_x, normal_y)``; ``other`` is ``None`` and the
   coordinates are 0 when nothing was hit.

.. py:method:: Node.set_character_velocity(vx, vy)

   Sets the CharacterBody2D velocity (used by ``move_and_slide``).

.. py:method:: Node.get_character_velocity()

   Returns the CharacterBody2D velocity as ``(vx, vy)``.

.. py:method:: Node.move_and_slide()
               Node.move_and_slide(vx, vy)

   Uses (or sets) the character velocity and moves it, projecting the remaining
   motion along each hit normal up to ``max_slides``. Returns
   ``(hit, vx, vy, on_floor, on_wall, on_ceiling)``.

.. py:method:: Node.slide_collision_count()

   Returns how many contacts the last slide produced.

.. py:method:: Node.slide_collision(index)

   Returns ``(other, hit_x, hit_y, normal_x, normal_y)`` for the contact at
   ``index``.

.. py:method:: Node.get_sprite()

.. py:method:: Node.get_animation()

.. py:method:: Node.get_camera()

.. py:method:: Node.get_particle()

.. py:method:: Node.get_button()

.. py:method:: Node.get_checkbox()

.. py:method:: Node.get_slider()

   Returns the component of the matching type on the object, or ``None``.

The ``Sprite`` class
--------------------

.. py:method:: Sprite.set_color(r, g, b, a)

   Sets the sprite tint; each channel is 0–255.

.. py:method:: Sprite.set_flip(flip_x, flip_y)

   Flips the sprite horizontally / vertically.

.. py:method:: Sprite.set_size(w, h)

   Overrides the sprite draw size.

.. py:method:: Sprite.set_water_enabled(enabled)

   Enables or disables the water effect (the sprite needs a normal map).

.. py:method:: Sprite.set_water_flow(ax, ay, bx, by)

   Sets the two flow vectors of the water effect.

.. py:method:: Sprite.set_water_strength(value)

   Sets the water effect strength.

The ``Animation`` class
-----------------------

.. py:method:: Animation.play()
               Animation.play(clip)

   Plays the animation; with an argument, switches to the named clip first.
   Returns True when it started.

.. py:method:: Animation.stop()

.. py:method:: Animation.is_playing()

   Returns True while the animation is running.

.. py:method:: Animation.current()

   Returns the name of the current clip as a string.

The ``Camera`` class
--------------------

Shake amplitudes are in screen pixels and never alter the camera's saved
offset.

.. py:method:: Camera.start_shake(amplitude_x, amplitude_y, frequency, duration_cycles)

   Starts a shake with the given amplitudes (pixels), frequency and duration
   in cycles.

.. py:method:: Camera.stop_shake()

.. py:method:: Camera.add_trauma(amount)

   Adds to the camera trauma (0–1), which scales the shake.

.. py:method:: Camera.set_trauma_profile(amplitude_x, amplitude_y, frequency, decay)

   Replaces the trauma-driven shake profile.

.. py:method:: Camera.clear_trauma()

.. py:method:: Camera.start_zoom_punch(amount, duration)

   A short zoom kick of ``amount`` over ``duration`` seconds.

.. py:method:: Camera.stop_zoom_punch()

.. py:method:: Camera.is_shaking()

   Returns True while a shake is active.

The ``RigidBody`` class
----------------------

``RigidBody`` wraps a RigidBody2D. Impulses divide by mass, so the numbers are not
velocities: a 40x40 box at density 1 has mass 1600 and
``apply_impulse(0, -8000)`` changes its velocity by 5 units per second.

.. py:method:: RigidBody.get_velocity()

   Returns the linear velocity as ``(x, y)``.

.. py:method:: RigidBody.set_velocity(x, y)

.. py:method:: RigidBody.get_angular_velocity()

   Returns the angular velocity in degrees per second.

.. py:method:: RigidBody.set_angular_velocity(deg)

   Sets the angular velocity in degrees per second.

.. py:method:: RigidBody.apply_force(x, y)

   Applies a force at the body center for one step.

.. py:method:: RigidBody.apply_impulse(x, y)

   Applies an immediate impulse at the body center.

.. py:method:: RigidBody.apply_torque(torque)

   Applies a torque for one step.

.. py:method:: RigidBody.get_gravity_scale()

.. py:method:: RigidBody.set_gravity_scale(scale)

   Gets / sets how strongly gravity affects this body (1 is normal, 0 is
   floating).

.. py:method:: RigidBody.set_type(type)

   Switches the body type: ``"static"``, ``"kinematic"`` or ``"dynamic"``.

.. py:method:: RigidBody.is_awake()

   Returns True if the body is awake.

.. py:method:: RigidBody.wake()

   Wakes the body up.

The ``Particle`` class
----------------------

.. py:method:: Particle.start()

.. py:method:: Particle.stop()

.. py:method:: Particle.reset()

.. py:method:: Particle.burst(count)

   Emits ``count`` particles immediately.

.. py:method:: Particle.is_playing()

UI control classes
------------------

Retained UI controls are created in the editor on a ``UiCanvas``. The handles
are read from a Node with ``get_button()``, ``get_checkbox()`` and
``get_slider()``.

.. py:method:: Button.clicked()

   Returns True once per click (the click is consumed).

.. py:method:: Button.set_text(text)

.. py:method:: CheckBox.is_checked()

.. py:method:: CheckBox.set_checked(value)

.. py:method:: CheckBox.changed()

   Returns True once per toggle (consumed).

.. py:method:: Slider.get_value()

.. py:method:: Slider.set_value(value)

.. py:method:: Slider.changed()

   Returns True once per drag change (consumed).

Global functions
----------------

Drawing
~~~~~~~

The draw functions only have an effect while ``on_draw`` or ``on_draw_ui`` is
running.

.. py:function:: set_draw_color(r, g, b, a=1)

   Sets the color for subsequent draw calls. Values are normalized 0–1 and are
   clamped. Stays active until the next call.

.. py:function:: draw_line(x1, y1, x2, y2, thickness=1)

.. py:function:: draw_rect(x, y, width, height, fill=True, thickness=1)

   ``thickness`` is used only when ``fill`` is False.

.. py:function:: draw_circle(x, y, radius, fill=True, segments=32, thickness=1)

   ``segments`` is clamped between 3 and 512.

.. py:function:: draw_text(x, y, text, size=16)

.. py:function:: draw_text_width(text, size=16)

   Returns the width of ``text`` in world units (the longest line times the
   size).

.. py:function:: object_count()

   Returns the number of objects in the running scene.

Input
~~~~~

.. py:function:: key_down(code)

.. py:function:: key_pressed(code)

.. py:function:: key_released(code)

   Query a physical key by its ``KEY_*`` constant. No string conversion is
   done in the VM.

.. py:function:: virtual_key_add(code, x, y, width, height)

   Adds an explicit rectangular touch button. Its input is synthesized as the
   supplied ``KEY_*`` code, so direct key and action queries work unchanged.

.. py:function:: virtual_keys_clear()

.. py:function:: virtual_keys_set_visible(visible)

.. py:function:: virtual_keys_visible()

   Manage script-defined virtual touch buttons in the standalone runner.

.. py:function:: action_down(name)

.. py:function:: action_pressed(name)

.. py:function:: action_released(name)

   Query a remappable input action by name.

.. py:function:: mouse_down(button)

.. py:function:: mouse_pressed(button)

   Mouse button state (0 = left). In the editor, clicks in other panels are
   ignored and the position is relative to the Game view.

.. py:function:: mouse_x()

.. py:function:: mouse_y()

   Mouse position relative to the Game view.

.. py:function:: wheel_y()

   Mouse wheel delta for the frame.

.. py:function:: get_fps()

   Returns the engine's rolling half-second FPS measurement. Stable enough for
   HUDs, unlike computing ``1 / dt`` every frame.

.. py:function:: profiler_visible()

   True while the runner's profiler overlay is open.

Viewport and camera
~~~~~~~~~~~~~~~~~~~

.. py:function:: viewport_width()

.. py:function:: viewport_height()

   The current Game view size in pixels.

.. py:function:: screen_to_world(x, y)

   Converts screen to world coordinates through the active camera. Returns
   ``(wx, wy)``.

.. py:function:: mouse_world_position()

   Mouse position in world coordinates. Returns ``(wx, wy)``.

.. py:function:: world_view_rect()

   The camera's visible world area as ``(left, top, right, bottom)``.

Scene and transitions
~~~~~~~~~~~~~~~~~~~~~

.. py:function:: load_scene(path)

   Schedules a scene replacement after the current frame. The scene is loaded
   through the normal asset paths and its physics world is rebuilt before the
   next update.

.. py:function:: fade_in(seconds)

.. py:function:: fade_out(seconds)

   Runs a screen fade over the given number of seconds.

.. py:function:: is_fading()

   Returns True while a fade is running.

.. py:function:: fade_progress()

   Returns the current fade progress as a float.

Audio
~~~~~

Loads return a sound handle; ``audio_play`` and friends return a voice handle.
Sounds can overlap; starting music replaces the previous music voice.

.. py:function:: audio_load(path)

   Loads a sound effect and returns its handle.

.. py:function:: audio_load_music(path)

   Loads music and returns its handle.

.. py:function:: audio_play(sound, volume=1, pitch=1, pan=0)

   Plays a sound effect. Returns the voice handle.

.. py:function:: audio_play_at(sound, x, y, volume=1, pitch=1, min_distance=64, max_distance=1024)

   Plays a sound effect at a world position. The active camera is the listener.
   Returns the voice handle.

.. py:function:: audio_play_music(sound, loop=True, volume=1)

   Starts music. Returns the voice handle.

.. py:function:: audio_crossfade_music(sound, loop=True, volume=1, seconds=1)

   Crossfades to a new music track over ``seconds``. Returns the voice handle.

.. py:function:: audio_stop(voice)

.. py:function:: audio_pause(voice)

.. py:function:: audio_resume(voice)

.. py:function:: audio_playing(voice)

   Control a voice by handle. Each returns a bool.

.. py:function:: audio_fade_in(voice, seconds)

.. py:function:: audio_fade_out(voice, seconds, stop=True)

   Fades a voice. Returns a bool.

.. py:function:: audio_stop_all()

.. py:function:: audio_stop_music()

.. py:function:: audio_set_master_volume(volume)

.. py:function:: audio_set_sfx_volume(volume)

.. py:function:: audio_set_music_volume(volume)

   Engine-wide mixer levels. Volume and mute preferences are persisted by the
   runner/editor.

.. py:function:: audio_set_master_muted(muted)

.. py:function:: audio_set_sfx_muted(muted)

.. py:function:: audio_set_music_muted(muted)

   Mute a bus without losing its volume.

.. py:function:: audio_master_muted()

.. py:function:: audio_sfx_muted()

.. py:function:: audio_music_muted()

   Returns the muted state of a bus.

.. py:function:: audio_set_listener_position(x, y)

   Overrides the audio listener position instead of following the active
   camera. Returns a bool.

Physics
~~~~~~~

The physics queries act on the running world.

.. py:function:: raycast(x, y, dx, dy, distance)

   Casts a ray from ``(x, y)`` in direction ``(dx, dy)`` for ``distance``
   units. Returns ``(hit, hx, hy)``; ``hit`` is a ``Node`` or ``None``.

.. py:function:: body_at(x, y)

   Returns the ``Node`` with a body under the point, or ``None``.

.. py:function:: set_gravity(x, y)

   Retunes the gravity of the live world.

.. py:function:: get_gravity()

   Returns the current gravity as ``(x, y)``.

Blackboard
~~~~~~~~~~

Shared key/value state global to the running scene.

.. py:function:: set_number(key, value)

.. py:function:: get_number(key, fallback=0)

.. py:function:: set_string(key, value)

.. py:function:: get_string(key, fallback="")

.. py:function:: set_flag(key, value)

.. py:function:: get_flag(key, fallback=False)

   Read / write blackboard entries. ``get_number`` on a flag returns 1 or 0.

.. py:function:: has_key(key)

   Returns True if the key exists.

.. py:function:: emit(event, value=0)

   Queues a broadcast event. It is delivered to every script's ``on_event``
   once per frame, after update.

Persistent user data
~~~~~~~~~~~~~~~~~~~~

Values are stored outside the project, in the writable per-user folder chosen
by SDL for the editor/runner. The store is loaded at startup.

.. py:function:: user_data_get_int(key, fallback=0)

.. py:function:: user_data_set_int(key, value)

.. py:function:: user_data_get_float(key, fallback=0)

.. py:function:: user_data_set_float(key, value)

.. py:function:: user_data_get_string(key, fallback="")

.. py:function:: user_data_set_string(key, value)

.. py:function:: user_data_get_bool(key, fallback=False)

.. py:function:: user_data_set_bool(key, value)

   Typed accessors for the persistent store.

.. py:function:: user_data_has(key)

.. py:function:: user_data_delete(key)

.. py:function:: user_data_clear()

   Key management.

.. py:function:: user_data_load(file=None)

.. py:function:: user_data_save(file=None)

   Loads / saves the store. Without an argument they use ``settings.json``;
   with one, a safe file name such as ``"profile_2.json"``. Returns a bool.

.. py:function:: user_data_read_text(file, fallback="")

   Returns the contents of a text file from the user folder, or ``fallback``.

.. py:function:: user_data_write_text(file, text)

   Writes a text file into the user folder. Returns a bool.

Key constants
-------------

The ``key_*`` functions take these integer constants. They are the physical
keys, so they ignore the layout; use ``action_*`` for remappable gameplay
actions.

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - Letters and digits
     - ``KEY_A`` … ``KEY_Z``, ``KEY_0`` … ``KEY_9``
   * - Navigation
     - ``KEY_SPACE``, ``KEY_ENTER``, ``KEY_ESCAPE``, ``KEY_TAB``,
       ``KEY_BACKSPACE``, ``KEY_LEFT``, ``KEY_RIGHT``, ``KEY_UP``,
       ``KEY_DOWN``
   * - Function keys
     - ``KEY_F1`` … ``KEY_F12``
   * - Modifiers
     - ``KEY_LEFT_CTRL``, ``KEY_RIGHT_CTRL``, ``KEY_LEFT_SHIFT``,
       ``KEY_RIGHT_SHIFT``, ``KEY_LEFT_ALT``
