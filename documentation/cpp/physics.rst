Physics (``kx`` and ``k2d_physics2d``)
======================================

The physics core is ``kx``, a standalone library with no engine coupling. The
``k2d_physics2d`` layer turns it into scene components. Both are deterministic
and the collision, solver and joint code is ported from proven references, not
invented.

The world (``kx::World``)
-------------------------

``kx::World`` (``physics/include/kx/world.h``) owns the bodies, the joints and
the contact list. Its Y axis points down, so the default gravity is positive
for a ground at the bottom.

.. code-block:: cpp

   kx::World world(Math::Vec2(0.0f, 980.0f));   // gravity in px/s^2

   kx::Body *box = world.CreateBox(Math::Vec2(320, 100), 40, 40, 1.0f);
   kx::Body *ground = world.CreateStaticBox(Math::Vec2(320, 700), 320, 20);

   world.Step(1.0f / 60.0f);

Creation helpers

* ``CreateBody(type, pos, angle)`` — a bare body; shapes are added later.
* ``CreateBox``, ``CreateCircle``, ``CreateStaticBox``, ``CreateKinematicBox``,
  ``CreateEdge(a, b)``, ``CreateChain(points, count, loop)``,
  ``CreatePolygon(pos, points, count, density)``,
  ``CreateMesh(pos, outline, count, density)`` — one-shape shorthands. The
  mesh is a concave outline triangulated into convex shapes.
* ``CreateFromImage(pos, pixels, w, h, bpp, threshold, density, scale,
  simplifyDegrees)`` — traces a sprite silhouette into collision shapes.
* ``Destroy(body)`` — removes a body.

Queries and casts

* ``BodyAtPoint(point, dynamicOnly = true)``
* ``QueryAABB(aabb, out)``, ``QueryCircle(center, radius, out)``
* ``RayCastClosest(origin, translation, out, categoryMask, includeSensors,
  ignoreBody)`` and ``RayCastAll(...)``
* ``TestMotion(body, motion, out, safeMargin, includeSensors)`` and
  ``TestPosition(body, position, out, includeSensors)`` — exact linear convex
  casts based on Box2D's GJK shape cast. They never advance the simulation.

Step and tuning

* ``Step(dt)`` — advances the simulation. ``SetVelocityIterations(n)`` tunes
  the solver.
* ``SetTreeBroadphase(bool)`` — toggles between the brute-force O(n²)
  broadphase and the dynamic tree.
* ``SetTimeSource(clock)`` — plug a custom clock (used for sleeping).
* ``Gravity()`` / ``SetGravity()``, ``Bodies()``, ``Contacts()``,
  ``BodyCount()``, ``ContactCount()``, ``Joints()``, ``Profile()``.

Bodies (``kx::Body``)
---------------------

``kx::Body`` (``physics/include/kx/body.h``) holds a fixed array of up to 32
shapes (``kMaxShapes``) in body-local coordinates, added with:

* ``AddCircle(localCenter, radius, density)``
* ``AddBox(halfWidth, halfHeight, localCenter, density)``
* ``AddEdge(localA, localB)``
* ``AddChain(points, count, loop)``
* ``AddPolygon(points, count, density)`` — builds a convex hull.
* ``AddMesh(outline, count, density)`` — triangulates a concave outline into
  convex shapes on the same body.
* ``AddFromImage(pixels, w, h, bpp, threshold, density, scale, simplifyDegrees)``

Mass and inertia are recomputed automatically with the parallel-axis theorem
after each ``Add*``.

Body state

* ``Position()``/``SetPosition()``, ``Angle()``/``SetAngle()``,
  ``Velocity()``/``SetVelocity()``, ``AngularVelocity()``/``SetAngularVelocity()``.
* ``Friction()``/``SetFriction()``, ``Restitution()``/``SetRestitution()``,
  ``LinearDamping()``, ``AngularDamping()``, ``GravityScale()``.
* ``FixedRotation()``, ``IsBullet()`` (high-speed continuous collision),
  ``IsAwake()``/``SetAwake()`` (sleeping).
* Forces and impulses: ``ApplyForce(force, point)``,
  ``ApplyForceToCenter(force)``, ``ApplyImpulse(impulse, point)``,
  ``ApplyLinearImpulseToCenter(impulse)``, ``ApplyAngularImpulse(impulse)``,
  ``ApplyTorque(torque)``.
* ``Type()`` — Static, Kinematic or Dynamic. Kinematic bodies are never pushed
  but move under their own velocity; Dynamic bodies respond to forces and
  gravity.

Collision filtering

Each shape carries a ``Filter { category, mask, group }``. Two shapes collide
when they share a non-zero positive group, or when each one's mask matches the
other's category (``kx::ShouldCollide``). ``SetFilter`` stamps the body default;
``SetShapeFilter`` changes one shape. Sensors (``SetSensor``) report contacts
without generating impulses.

Contacts and events

``Step`` produces a ``ct::Vector<ContactInfo>`` (``world.Contacts()``) and
contact events through ``Body::SetContactCallback`` or the world-level event
list. ``ContactEvent`` carries the phase (begin/persist/end), the two bodies,
the shape indexes and the manifold. Sensors flow through the same events.

Joints
------

The joint classes live in ``physics/include/kx/joints/``:

* ``MouseJoint`` — soft constraint pulling a body toward a target point.
* ``DistanceJoint`` — rigid rod or spring between two points.
* ``WheelJoint`` — spring suspension plus a motor (vehicles).
* ``RevoluteJoint`` — pin rotation with a motor and angle limits.
* ``GearJoint`` — gear ratio coupling between two revolutes.
* ``MotorJoint`` — keeps the relative rotation of two bodies.

Joints are created with ``new``, handed to the world with ``AddJoint`` and
destroyed with ``DestroyJoint``.

Tilemap collider (``kx::TileMapCollider``)
------------------------------------------

Builds chunked static collision bodies by greedily merging adjacent solid
cells into boxes (``physics/include/kx/tilemapcollider.h``). Used by
``k2d::TileMapComponent``.

The scene layer (``k2d_physics2d``)
-----------------------------------

The component layer wraps ``kx`` so physics participates in the scene model.

``k2d::PhysicsWorld2D`` (``physics2d/include/k2d/PhysicsWorld2D.h``)

* Owns a ``kx::World`` and the ``RigidBody2D`` list. ``SetActive`` /
  ``Active()`` selects the world the script queries target.
* ``build(root)``, ``step(dt)``, ``clear()``.
* ``setGravity`` / ``gravity()``, ``setFixedTimeStep(seconds)``.
* ``raycast(origin, direction, distance, outPoint, outNormal, ignore)`` —
  returns the first ``GameObject`` hit.
* ``objectAtPoint(point)``, ``overlapCircle(center, radius, out)``.
* ``setCollisionCallback(cb, user)`` — the collision hook that feeds
  ``on_collision`` in scripts.
* ``debugDraw(canvas, flags)`` — renders the simulation as an overlay.

``k2d::RigidBody2D`` (``RigidBody2D.h``)

A component wrapping one ``kx::Body``. Body type, density, friction,
restitution, damping, gravity scale, fixed rotation and bullet mode are
exposed; ``velocity()``/``setVelocity()``, ``applyForce``, ``applyImpulse``,
``applyTorque`` and ``wake()`` forward to the ``kx`` body. ``body()`` returns
the raw ``kx::Body *``.

Colliders

``k2d::Collider2D`` is the base for the shape components. Each collider carries
an offset, a sensor flag and a collision filter (category/mask):

* ``BoxCollider2D``, ``CircleCollider2D``, ``EdgeCollider2D``,
  ``PolygonCollider2D``, ``ChainCollider2D``.

A RigidBody2D collects the colliders on its GameObject when the world is built;
changing a collider marks the body for rebuild.

``k2d::CharacterBody2D`` (``CharacterBody2D.h``)

A script-driven kinematic body. It uses the ``RigidBody2D`` + colliders on the
same object but never runs through the impulse solver; movement uses the
engine's convex shape cast.

* ``moveAndCollide(motion, testOnly)`` — moves until the first contact and
  returns a ``CollisionInfo``.
* ``moveAndSlide()`` — uses the component velocity, projecting the remaining
  motion along each hit normal up to ``maxSlides``. Tracks
  ``isOnFloor()``/``isOnWall()``/``isOnCeiling()``.
* ``placeFree(x, y)``, ``placeMeeting(x, y)`` — non-mutating placement tests.
* ``safeMargin()``, ``maxSlides()``, ``motionMode()`` (floating/grounded),
  ``upDirection()``, ``floorMaxAngleDegrees()``.
