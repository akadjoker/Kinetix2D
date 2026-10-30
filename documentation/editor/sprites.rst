Sprites and atlases
===================

A sprite is an object with a **Sprite** component (``SpriteComponent``)
showing an image, or a rectangle of it. The **Sprite Editor** cuts an image
into named rectangles, its *regions*, and turns them into sprite objects or
animation frames. The **Atlas** tab packs many images into one, with a region
for each.

The quickest sprite
-------------------

Drag an image from **Assets** onto the scene view. It becomes an object named
after the file, with a Sprite showing the whole image, where it was dropped.
Dropped on an object's row in the Hierarchy instead, the image goes to that
object's Sprite (or TileMap, NinePatch...), or gives it a Sprite.

Cut an image into regions
-------------------------

Double-click an image in **Assets** (or drop one onto the Sprite Editor's
canvas). The Sprite Editor opens it, with the regions it had last time.

There are three ways to make regions:

**Draw them**
   Left-drag over empty pixels draws a new region. Click a region to select
   it, drag it to move it, drag one of its eight handles to resize it.
**Detect**
   *Detect: replace the regions with the islands of opaque pixels.* Every
   group of touching pixels whose alpha reaches **alpha** becomes a region,
   if it has at least **min px** pixels. Good for loose sprites on a
   transparent sheet.
**Split**
   *Split: replace the regions with a grid of cols x rows cells.* Good for
   sprite sheets of equal frames. Pixels left over by the division are left
   out.

Detect and Split **replace every region** the image had.

The column on the right lists the regions. Select one to rename it (type the
name and press Enter; a name already in use is refused) or to set its
**x**, **y**, **w**, **h** exactly. With a region selected on the canvas:

* **Delete** or **Backspace** removes it;
* the arrow keys move it one pixel, **Shift** + arrow ten.

New regions are named ``image_001``, ``image_002``… after the image.

The canvas pans with any button but the left one and zooms with the wheel
(10 % to 6400 %, pixel grid from 800 %). *Fit the image in view* brings it
back.

Press *Save the regions* to keep them. They are saved beside the image as
``image.sprite.json``:

.. code-block:: json

   {"texture": "assets/hero.png",
    "regions": [{"id": "hero_001", "rect": [0, 0, 32, 32]}]}

Put a region in the scene
-------------------------

* Drag the region's preview (*Drag into the scene*) onto the scene view: a
  sprite object appears where it is dropped; or
* press **Add** (*Add to scene*): it appears in the middle of the view.

The object is named after the region; its Sprite shows the region
(``sourceRect``) at the region's size. This is one undo step, *Add sprite*.

The scene keeps the rectangle, not the region's name: changing the region
later does not change the sprites already placed.

Regions become animation frames the same way: drag them onto the Animator's
frame strip, or use the Animator's button that adds every region of the
image open in the Sprite Editor. See :doc:`animation`.

Pack an atlas
-------------

The **Atlas** tab packs images into pages of one texture, so a scene draws
them in fewer batches.

1. Drop images from **Assets** onto the Atlas list, or press *Add every image
   of a folder* (the folder's own images, not its subfolders). An image
   already listed is skipped; *Take the selected image out* removes one.
2. Choose the largest page size (512, 1024, 2048 or 4096 px), the
   **padding** between images (0–16, 2 by default) and whether to **Trim**
   the transparent border of each image.
3. Press *Build the atlas* and choose where the image goes (``atlas.png`` by
   default).

The build writes:

``name.png``
   The page. When the images need more than one: ``name_1.png``,
   ``name_2.png``…
``name.sprite.json`` (one per page)
   A region per image, named after the image's file.
``name.atlas.json``
   The recipe: the list of images, the size, the padding and the trim.
   Double-click it in Assets to open the Atlas tab again and rebuild after
   changing an image.

The first page then opens in the Sprite Editor, from where its regions go to
the scene and to the Animator as above. Textures already loaded from those
pages are reloaded at once.

.. warning::

   * **Trim** moves each image's pixels and keeps no offset: animation
     frames that must line up lose their alignment. Leave it off for them.
   * Two images with the same file name in different folders give the same
     region name, and only the first gets a region.
   * Region edits have no undo. Opening another image saves the unsaved
     regions **without asking**, and quitting the editor does not ask about
     them.
   * **Detect** reads the image file on disk: save the Image Editor first.
