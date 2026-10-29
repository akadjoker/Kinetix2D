Objects and components
======================

An object is a node of the scene tree: a name, a transform (position,
rotation, scale) and children that move with it. Everything else is a
component. There are no object types to choose from: a "sprite", a "player"
or a "light" is an object with the components that make it one.

Make an object
--------------

**Empty object**
   **New object** in the Hierarchy's toolbar, or right-click → **New Object**.
   It is named *GameObject* and goes beside the selected object (as its
   sibling), or at the top of the tree when nothing is selected.
**From an image**
   Drag an image from **Assets** onto the scene view: an object named after
   the file, with a Sprite of the whole image, where it is dropped.
**From a sprite region**
   Drag a region from the Sprite Editor onto the scene view, or press its
   **Add** button. See :doc:`sprites`.
**From a script**
   Drag a script from Assets onto empty space in the scene view (or below the
   Hierarchy's rows): an object named after the script, holding it.
**From a prefab**
   Drag a ``.k2dprefab`` onto the scene view, or onto a Hierarchy row to put
   it under that object.
**An image onto an object**
   Drag an image from Assets onto an object's row in the Hierarchy: it goes to
   the first component that takes an image (a Sprite, a TileMap, a NinePatch,
   a Particle...), and an object with none gets a Sprite. Assets and the
   Hierarchy share the left rail: drag over the *Hierarchy* button and it
   opens, then drop on the row. Dropped below the rows, the image makes a new
   sprite object.
**From another object**
   **Duplicate** (Ctrl+D), or **Copy** and **Paste** (Ctrl+C, Ctrl+V). The
   clipboard is the system's, so objects paste into another scene or another
   editor.

The Skeleton panel adds bones; see :doc:`skeletons`.

Arrange the tree
----------------

* Drag a row onto another to make it a child of it, or just above or below to
  make it a sibling; drop it below the last row to take it to the top. The
  object keeps its local transform.
* **Move up** / **Move down** change the order among siblings.
* Each row has an eye and a lock. A hidden or locked object cannot be clicked
  in the scene view, and a locked one cannot be dragged.
* Ctrl+click adds or removes a row from the selection, Shift+click takes a
  range.
* Rename an object with the **Name** field at the top of the Inspector.

Give it components
------------------

Select the object; the Inspector shows its **Name**, its **Transform** and one
section per component. At the bottom, **Add Component** opens a menu of
the components by what they are for (Rendering, Shapes, Animation, Effects,
Lights, Camera & Audio, Physics, Colliders, Joints, Movement, AI & Navigation,
UI, Script); picking one adds it at once (one undo step). Each section has a
*Remove this component* button. Sections are shown in a fixed order by kind,
not in the order they were added.

An object can hold several components of a kind where that makes sense: a
Light2D and a DirectionalLight2D, or several colliders on one body.

Which component for what
~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 26 74

   * - To…
     - Add
   * - Show an image
     - **Sprite** — an image or a rectangle of it. **NinePatch** — an image
       whose corners keep their size when it is stretched (panels, buttons).
       **SpriteBatch** — many sprites of one image in one object, each
       editable in the scene view.
   * - Draw shapes
     - **RectShape**, **CircleShape**, **CapsuleShape**, **Polygon2D**,
       **Line2D**. Shapes only draw; they do not collide.
   * - Paint a level from tiles
     - **TileMap**, and **TileMapCollider2D** for its solid cells. See
       :doc:`tiles`.
   * - Animate
     - **Animation2D** — frames of a sprite. **Skeleton2D** and **Bone2D** —
       bones with animated clips. See :doc:`animation` and
       :doc:`skeletons`.
   * - Effects
     - **Particle** — an emitter. See :doc:`particles`.
       **ParallaxLayer** — a layer that scrolls at its own speed as the
       camera moves.
   * - Light
     - **Light2D** (a point light), **DirectionalLight2D** (a sun) and
       **LightOccluder2D** (what casts shadows).
   * - See the world
     - **Camera** — what the Game view and the game show.
   * - Physics
     - **RigidBody2D** (``bodyType`` Static, Kinematic or Dynamic) or
       **CharacterBody2D**, with one or more colliders:
       **BoxCollider2D**, **CircleCollider2D**, **PolygonCollider2D**,
       **EdgeCollider2D**, **ChainCollider2D**, **TileMapCollider2D**.
       A collider only does something on an object with a body: the scene
       view draws it orange when it has none.
   * - Join bodies
     - **DistanceJoint2D**, **RevoluteJoint2D**, **WheelJoint2D**,
       **MotorJoint2D**, **MouseJoint2D**, **GearJoint2D**.
   * - Move without code
     - **PathMotion2D** — through a list of keyframed poses.
       **ActionSequence2D** — a sequence of moves, turns and waits.
   * - AI
     - **NavigationRegion2D** — a walkable area (starts as a 256 × 256
       square, edited in the scene view). **NavigationAgent2D** — finds a
       route over it. **Steering2D** — seek, flee, arrive, wander,
       separation, avoidance. **Formation2D** — keeps a group in a
       formation.
   * - Sound
     - **AudioPlayer**.
   * - Behaviour
     - **ZenScript** — a Python-syntax script. See :doc:`scripts`.
   * - Game UI
     - **UiCanvas**, **UiPanel**, **UiLabel**, **UiButton**, **UiCheckBox**,
       **UiSlider**.

In the Inspector some sections carry the C++ class name: the Sprite is
*SpriteComponent*, the Camera *CameraComponent*, the TileMap
*TileMapComponent*, and so on.

.. note::

   The Ui components can be added, but have no section in the Inspector yet:
   set them up from a script.

Editing values
~~~~~~~~~~~~~~

* Numbers with a range are sliders; drag them, or type a value (some, such as
  rotation and scale, accept values past the slider's ends).
* Two-number values (``x``, ``y``) are two fields.
* A file field (*Drop an asset here*) shows the file's name; hover it to see
  the whole path and, for an image, the image. It takes a file dragged from
  Assets, or a typed path; the cross clears it.
* Every change is one undo step, recorded when the drag ends.

Prefabs
-------

A prefab is an object and its children saved to a file, placed again as many
times as needed.

* **Save as prefab** in the Hierarchy (or right-click → **Save as
  Prefab...**) writes the selected object to a ``.k2dprefab``.
* Drag the file onto the scene view to place a copy under the pointer, or onto
  a Hierarchy row to put it under that object.
* Double-click it in Assets to edit the prefab itself: the bar on top says
  *Editing prefab*, **Save the prefab** (Ctrl+S) writes it, and **Back to the
  scene** returns.

The Assets panel
----------------

Assets shows the project's ``assets`` folder (the project folder when there is
none), folders first, images with a thumbnail. **Up one folder** and
**Refresh** are its only buttons: files and folders are made outside the
editor, by the tools that save them, or by the Script Editor's *New script*.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Double-click
     - Opens
   * - a folder
     - the folder
   * - ``.k2dscene``
     - the scene (asking to save the current one)
   * - ``.png`` ``.jpg`` ``.jpeg`` ``.bmp`` ``.tga``
     - the Sprite Editor
   * - ``.py`` ``.zen`` ``.zs``
     - the Script Editor
   * - ``.k2dprefab``
     - the prefab, for editing
   * - ``.atlas.json``
     - the Atlas tab

Files (not folders) can be dragged out of Assets onto the scene view, the
Hierarchy, file fields of the Inspector and the editors that take them.
