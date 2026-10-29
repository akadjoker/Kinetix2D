Tile maps
=========

A tile map is a grid of cells drawn from one tileset image. It lives in a
**TileMap** component (``TileMapComponent`` in C++), and the **Tile Painter**
panel paints it straight in the scene view. Collision is one flag per cell,
turned into physics shapes by a **TileMapCollider2D**.

Make a tile map
---------------

1. In the **Hierarchy**, press **New object** (or right-click → **New Object**).
   The new object is an empty ``GameObject``.
2. Give it a TileMap. Either pick **TileMap** in the Inspector's
   **Add Component...**, or open the **Tile Painter** tab with the object
   selected: it says *This object has no TileMap* and offers a **TileMap**
   button.
3. Set the component's properties in the Inspector:

   ``texture``
      The tileset image (png, jpg, jpeg, bmp, tga). Drag it from **Assets**
      onto the field.
   ``cellSize``
      The size of one tile in the image, and of one cell in the world.
      32 × 32 by default.
   ``columns`` / ``rows``
      The size of the map in cells. A new map is **0 × 0**, and nothing can
      be painted on it until both are set. Resizing keeps the cells that
      still fit and clears the new ones.
   ``atlasTilesX``
      How many tiles a row of the tileset holds. Press the grid button of the
      Tile Painter, *Atlas columns from the texture*, to compute it from the
      image. When it does not match the image, the Tile Painter's status line
      says *atlas columns differ from the texture: press the grid button*.
   ``blendMode``
      Alpha, Additive, Multiplied, AddColors or SubtractColors.

The map starts at the object's origin and grows towards +x and +y. Move,
rotate or scale the object and the map follows; painting still lands on the
right cell.

Paint
-----

Open the **Tile Painter** tab. It sits with the Console, Animator, Skeleton
and Profiler tabs and always works on the selected object. While it is
visible and the selected object has a TileMap, the **left button paints in
the scene view** instead of selecting. Any other button pans and the wheel
zooms, as usual.

The palette shows the tileset:

* click a tile to pick it, or drag over several to pick a block that is
  stamped as a whole;
* the wheel zooms the palette around the pointer, any other button pans it;
* picking a tile while the Eraser or the Picker is active switches back to
  the Brush.

The toolbar holds five tools; one is active at a time:

.. list-table::
   :header-rows: 1
   :widths: 18 82

   * - Tool
     - What it does
   * - **Brush**
     - Stamps the picked block with its top-left on the cell under the
       pointer. Dragging keeps stamping, aligned to where the stroke began.
   * - **Eraser**
     - Clears one cell; drag to clear more.
   * - **Fill**
     - Fills the cells touching the clicked one (up, down, left, right) that
       hold the same tile. A picked block repeats as a pattern.
   * - **Rectangle**
     - Drag a rectangle; it is filled when the button is released. A yellow
       outline previews it.
   * - **Picker**
     - Picks the tile under the pointer into the palette and goes back to
       the Brush. An empty cell picks nothing.

In the scene view the map is outlined in green. Under the pointer the Brush
shows the tiles it will lay down, and every tool outlines the cells it will
touch: red for the Eraser, yellow for the others. The status line reads
``columns x rows cells of w x h   brush w x h``.

Each stroke is one undo step (*Paint tiles*), recorded only when it changed
something. Cells outside the map are ignored. There are no layers (one grid
per component: put two objects for a background and a foreground) and no
auto-tiling.

Collision
---------

Turn on the **Collision** toggle of the Tile Painter (*the tools mark cells
solid, the eraser clears them*). Solid cells show in translucent red, only
while the toggle is on. In this mode every tool works one cell at a time:
Brush, Fill and Rectangle mark cells solid, the Eraser clears them. The
Picker still picks tiles.

Collision belongs to the cell, not to the tile in the tileset: painting the
same wall tile elsewhere does not make it solid.

For the solid cells to collide in Play:

1. add a **RigidBody2D** to the object holding the TileMap, with
   ``bodyType`` set to **Static** for level geometry;
2. add a **TileMapCollider2D** to the same object. It has the usual collider
   fields: ``offset``, ``sensor``, ``category`` and ``mask``.

The solid cells are merged into boxes, in chunks of 16 × 16 cells, when the
body is made.

From a script
-------------

.. code-block:: python

   class Door(ScriptComponent):
       def on_start(self):
           self.map = self.node.get_component<TileMap>()

       def open(self, x, y):
           self.map.set_tile(x, y, 0)
           self.map.set_collision(x, y, False)

``get_tile(x, y)``, ``set_tile(x, y, id)``, ``has_collision(x, y)``,
``set_collision(x, y, solid)``, ``get_columns()``, ``get_rows()``,
``get_cell_size()`` → ``(w, h)``, ``world_to_cell(wx, wy)`` → ``(cx, cy)`` and
``cell_to_world(cx, cy)`` → the cell's top-left ``(wx, wy)``.

A tile id is 0 for an empty cell, otherwise ``row * atlasTilesX + column + 1``
counted in the tileset.

.. note::

   ``set_collision`` changes the flag, but the physics shapes were built when
   the body was made and are not rebuilt: in Play the change shows in
   ``has_collision`` and in pathfinding, not in what bodies collide with.
   Changes made by scripts in Play are, like everything in Play, gone after
   **Stop**.

In the scene file
-----------------

A TileMap is saved inside the scene (``.k2dscene``): ``texture``,
``cellWidth``, ``cellHeight``, ``columns``, ``rows``, ``atlasTilesX``,
``atlasPadding``, ``atlasGap``, ``blendMode``, and ``cells`` and ``collision``
as flat arrays row by row. ``atlasPadding`` (the margin around the tileset)
and ``atlasGap`` (the space between its tiles) have no field in the Inspector;
edit them in the scene file for tilesets that need them.
