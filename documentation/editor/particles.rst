Particles
=========

A **Particle** component (``ParticleComponent``) is an emitter: fire, smoke,
sparks, rain. The **Particles** tab of the right rail edits it; a dot on the
tab tells that the selected object has one.

Make an emitter
---------------

1. Make an object (**New object** in the Hierarchy) and place it where the
   effect comes from.
2. Open the **Particles** tab. It says *This object has no particle emitter*:
   press the button beside it, or pick **Particle** in **Add Component...**.
3. Pick a **Preset...**: *Fire*, *Smoke*, *Sparks*, *Magic*, *Rain* or
   *Explosion*. A preset sets every value, so start with one and adjust.
4. Give it a ``texture`` (drag an image from Assets): **without a texture
   nothing is drawn**. A small soft dot works for most effects.

With **Live preview** on in the main toolbar the emitter runs while editing.
*Restart the emitter* starts it again, *Burst* fires its ``oneShotCount`` at
once; neither is an undo step. The tab shows how many particles are alive.

The values
----------

**Emission**

``mode``
   *Loop* emits ``emissionRate`` particles a second for as long as it runs;
   *OneShot* fires ``oneShotCount`` at once when it starts (an explosion);
   *Persistent*, the default of a new component, emits nothing on its own.
``emissionRate``, ``oneShotCount``, ``capacity``
   Particles a second, particles per burst, and the most alive at once.
``emitterShape``, ``emitterSize``
   Where they are born: a *Point*, a *Circle* or a *Rectangle* of that size.
``gravity``
   Pulls every particle (rain falls, smoke rises with a negative y).
``followOwner``
   The emitter moves with the object (on by default); particles already
   born stay where they are. Off, it keeps emitting where it was.

**Motion**

``direction`` and ``spreadDegrees`` (0–360) aim them; ``speedMin`` /
``speedMax`` and ``lifeMin`` / ``lifeMax`` pick each particle's speed and life
between the two; ``drag`` slows them.

**Look**

``sizeMin`` / ``sizeMax`` and ``endSize``; ``rotationMin`` / ``rotationMax``
and ``angularVelocityMin`` / ``angularVelocityMax``; ``fadeIn`` and
``fadeOut``; ``colorStart`` to ``colorEnd`` over the life;
``faceDirection`` (with ``faceDirectionOffsetDegrees``) turns each particle
along its motion, for sparks and rain; ``blendMode`` (*Additive* for fire and
light); ``ySort``.

Each edit is one undo step, and the emitter is saved in the scene. The tab
edits the first emitter of the object.

From a script
-------------

.. code-block:: python

   class Barrel(ScriptComponent):
       def explode(self):
           self.node.get_particle().burst(80)

``start()``, ``stop()``, ``reset()``, ``burst(count)``, ``is_playing()``.
