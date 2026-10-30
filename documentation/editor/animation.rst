Frame animation
===============

An **Animation2D** plays *clips*: lists of frames, each a rectangle of an
image, shown one after another on the **Sprite** of the same object. The
**Animator** panel makes the clips and fills them with frames.

Make an animated object
-----------------------

1. Make a sprite object: drag the first frame from the Sprite Editor onto the
   scene view, or an image from Assets (see :doc:`sprites`). The Animation2D
   draws through this Sprite; without one nothing shows.
2. Open the **Animator** tab (at the bottom, with the Console). With the
   object selected it says *This object has no Animation2D*: press the
   button beside it, or pick **Animation2D** in **Add Component**.
3. Press **New clip**. The clip is named ``clip_N``, at 8 fps, looping, with
   no frames.
4. Add frames (below).
5. Press *Play this clip when the scene starts* on the clip the game should
   open with. The list marks it with ``>``. The first clip made gets the mark
   on its own.

Add frames
----------

The row of cells at the bottom of the Animator is the clip's frame strip.

* Drag a region's preview from the **Sprite Editor** onto the strip: the frame
  goes where it is dropped.
* Drag an image from **Assets** onto the strip: the whole image is one frame.
* *Add every region of the image open in the Sprite Editor* appends all its
  regions at once, in order. Cut a sprite sheet with **Split** in the Sprite
  Editor first, and this builds the whole clip.

On the strip:

* click a frame to select it (the preview jumps to it);
* drag a frame sideways to move it; a blue mark shows where it lands;
* **Delete** or **Backspace**, or *Delete the selected frame*, removes it;
* **←** / **→** move the selection; the wheel scrolls the strip.

Frame numbers are written under the cells; the frame the preview is showing is
green.

Clip settings
-------------

**Clip name**
   Type and press Enter; a name already in use is refused.
**fps**
   Frames per second, 1 to 120.
**Mode**
   *One shot* (stops on the last frame), *Loop*, or *Ping pong* (forwards,
   then backwards).

The status line gives the number of frames and the clip's length in seconds.
**New clip**, *Duplicate the clip* and *Delete the clip* work on the list on
the left.

The box on the right plays the clip inside the panel: *Play the preview*
starts and stops it, *Next frame* steps it. With **Live preview** on in the
main toolbar, the scene plays the animation too, while editing.

Everything is saved in the scene; each change is one undo step. A frame keeps
the image and the rectangle, not the region's name.

From a script
-------------

.. code-block:: python

   class Hero(ScriptComponent):
       def on_start(self):
           self.anim = self.node.get_animation()

       def on_update(self, dt):
           if key_down(KEY_RIGHT):
               if self.anim.current() != "run":
                   self.anim.play("run")

       def on_animation_finished(self, clip):
           self.anim.play("idle")

``play(name)`` (or ``play()`` for the current clip), ``stop()``,
``is_playing()``, ``current()``, ``get_frame()``. A script receives
``on_animation_finished`` when a *One shot* clip ends, and
``on_animation_event`` for events attached to frames.

.. note::

   * A clip only starts when the scene loads if it had frames when it was
     marked to start: add the frames first, then press *Play this clip when
     the scene starts*.
   * The Animator has no controls yet for frame events, frame offsets or
     frame points.
