Skeletons
=========

A skeleton animates a character made of pieces: an object with a
**Skeleton2D**, and below it a tree of objects with a **Bone2D**. Each bone
points along its own x axis for its ``length``; put the sprites of the body
under the bones and they follow them. The **Skeleton** panel makes the clips
and keys the poses.

Build the skeleton
------------------

1. Make an object for the character (**New object**) and select it.
2. Open the **Skeleton** tab (at the bottom, with the Console). It says
   *Neither this object nor one above it has a Skeleton2D*: press the button
   beside it, or pick **Skeleton2D** in **Add Component...**.
3. With the character selected, press *Add a bone at the tip of the selected
   one*: the first bone appears at the origin, 50 long. With that bone
   selected, press it again for the next one, at its tip and as long as it.
   Bones are named ``Bone_N``; rename them in the Inspector (``hip``,
   ``thigh``, ``shin``…) **before** animating, since clips find bones by name.
4. Shape the rest pose with the Select tool (**Animate** off): drag a bone to
   turn it, drag its tip to pull the chain, or type values in the Inspector.
   ``length`` is in the Bone2D section.
5. Drag the body's sprites onto the bones in the Hierarchy. A reparented
   object keeps its *local* position, so place each sprite again after
   dropping it on its bone.

The panel works on the Skeleton2D of the selected object or of any object
above it, so selecting a bone keeps the skeleton in the panel.

Animate
-------

1. Press **New clip** (``clip_N``, 1 s, looping). The dropdown picks the clip,
   **Clip name** renames it, **length** (0.05–600 s) and **Loop** set it.
2. Turn on **Animate** (the record button). The bones now show the clip at
   the playhead, and the pose you had is kept as the rest pose.
3. Move the playhead (click or drag the ruler), then pose: drag a bone to turn
   it, drag its tip to pull the chain. Releasing the drag **keys** the pose at
   the playhead: the dragged bone and the bones above it, as many as **IK**
   says (2 by default). *Key the selected bone at the playhead* keys without
   dragging.
4. Press *Play the clip* to watch it (it turns Animate on).

Keys are measured from the rest pose and snap to frames of 1/30 s. Every bone
property animated has its own track (``x``, ``y``, ``rotation``,
``scale x``, ``scale y``).

On the timeline:

* click a key to select it, Ctrl+click to toggle it, Shift+click to add it;
  drag over a track to select a rectangle of keys;
* drag keys to move them in time;
* *Delete the selected keys* removes them (a track left empty goes too);
* the easing combo gives the selected keys *Linear*, *Step*, *Ease in*,
  *Ease out* or *Ease in out*;
* the middle button pans, the wheel zooms.

Turn **Animate** off to go back to the rest pose; that pose is what the scene
saves, with the clips. To play a clip when the game starts, type its name in
the Skeleton2D's ``autoplay`` field in the Inspector; ``speed`` scales the
playback.

From a script
-------------

.. code-block:: python

   class Walker(ScriptComponent):
       def on_start(self):
           self.skel = self.node.get_component<Skeleton>()
           self.skel.play("walk")

       def reach(self, x, y):
           self.skel.solve_ik("hand", x, y, 2)

``play(name, loop=True, speed=1)``, ``stop()``, ``pause()``, ``resume()``,
``seek(time)``, ``is_playing()``, ``current()``, ``get_time()``,
``get_speed()`` / ``set_speed()``, ``clip_count()``, ``find_bone(name)``,
``reset_to_rest()``, and ``solve_ik(bone_name, x, y, chain=0, iterations=8,
tolerance=0.5)`` to pull a bone's tip to a point. A Bone has ``get_length()`` / ``set_length()``,
``get_rest_position()``, ``get_rest_rotation()``, ``save_rest_pose()`` and
``reset_to_rest()``.

.. note::

   * Only poses made by dragging in the scene view are keyed. Values typed in
     the Inspector while Animate is on are not keyed and do not stay.
   * Renaming a bone after keying it leaves its tracks without a bone.
   * A script's ``play`` loops unless told ``loop=False``, whatever the
     clip's **Loop** says.
   * Deleting the clip named in ``autoplay`` leaves the name there: clear
     it.
   * Double-clicking the timeline, its mute and lock buttons, and the Delete
     key do nothing to the clip yet.
