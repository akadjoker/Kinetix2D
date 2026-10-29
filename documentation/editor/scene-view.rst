The scene view
==============

The **Scene** tab is where objects are placed and shaped. It shows the scene
as it will play, with the outlines and handles of the tools on top.

Moving around
-------------

* Wheel: zoom about the pointer.
* Any button but the left one, dragged: pan.
* **F**: centre the view on the selected object.

Selecting
---------

Click an object to select it. Under the pointer, an object's origin marker
wins, then a bone, then the smallest outline (of a sprite, a shape, a tile
map or a collider). Hidden and locked objects are skipped.

* Ctrl+click adds an object to the selection, or takes it out.
* Drag over empty space to select every object whose origin falls in the
  rectangle (with Ctrl, they are added to the selection).

Tools
-----

.. list-table::
   :header-rows: 1
   :widths: 18 10 72

   * - Tool
     - Key
     - Handles
   * - **Select**
     - Q
     - Selects; edits the points of what the object holds (below).
   * - **Move**
     - W
     - The red arrow moves along x, the green along y, the square in the
       middle moves freely.
   * - **Rotate**
     - E
     - Drag the ring.
   * - **Scale**
     - R
     - The axes scale along x or y, the middle square scales evenly.

Move, Rotate and Scale act on every selected object (but locked ones).

The toolbar's toggles:

**Grid**
   A grid of 64 units; the axes through the origin are red (x) and green
   (y).
**Snap**
   Moves and points in steps of 16 units, turns in steps of 15°. Scaling is
   not snapped.
**Colliders**
   Draws every object's colliders, joints and navigation regions (the
   selected object's always show): green for a solid collider, blue for a
   sensor, orange for a collider on an object without a body, which does
   nothing.
**Live preview**
   Particles and animations move while editing. Scripts and physics never
   run outside Play.

Editing points with Select
--------------------------

With the **Select** tool, the selected object's parts can be edited right in
the view. Each edit is one undo step.

Colliders
   Drag a point to move it. Double-click an edge of a polygon or a chain to
   add a point there; Ctrl+click a point to remove it (a polygon keeps at
   least 3, a chain 2). A box has a corner that resizes it, a circle a point
   on its rim for the radius, an edge collider its two ends.
Joints
   Drawn from anchor to anchor. Drag the anchor on the selected object; the
   other anchor shows when its object is selected.
Navigation regions
   Drag a point; double-click an edge to add one; Ctrl+click to remove one
   (while the loop has more than 3).
SpriteBatch
   Every sprite of the batch is outlined; drag one to move it.
Bones
   Drag a bone to turn it; drag its tip to pull the chain behind it (see
   :doc:`skeletons`).

What else is drawn
------------------

* The selected object's name beside its marker.
* The radius of a selected Light2D.
* The frame of every camera, labelled with its name, resolution and scale
  mode; the active one brighter, with the screen around it and its limits.
  Without a camera, the screen the game shows around the origin.

While the **Tile Painter** is open on an object with a TileMap, the left
button paints tiles instead of selecting: see :doc:`tiles`.
