Image Editor
============

The **Image Editor** paints the pixels of an image in the project. It does not
create objects: every stroke goes straight to the texture the scene already
uses, so the sprites showing that image change while you paint, before the
file is saved.

Open an image
-------------

* In the **Sprite Editor**, press *Edit pixels: open the image in the Image
  Editor*; or
* drag a png, jpg, jpeg, bmp or tga from **Assets** onto the Image Editor's
  canvas.

The tab's title shows the image's path, with ``*`` while there are unsaved
changes. The image is fitted to the view when it opens.

Tools
-----

.. list-table::
   :header-rows: 1
   :widths: 18 82

   * - Tool
     - What it does
   * - **Pencil**
     - A square brush of **size** pixels (1–64) in the current colour,
       drawn along the drag without gaps. The default tool.
   * - **Eraser**
     - The same brush, painting transparent pixels.
   * - **Fill**
     - Fills the pixels of exactly the clicked colour that touch it (up,
       down, left, right).
   * - **Line**
     - A one-pixel line from where the button goes down to where it comes
       up, previewed while dragging.
   * - **Rectangle**
     - A one-pixel outline, previewed while dragging. Use **Fill** inside it
       for a solid rectangle.
   * - **Picker**
     - Takes the colour under the pointer, and keeps taking it while
       dragged.

**size** applies to the Pencil and the Eraser only. The colour button picks
the colour, alpha included; the swatches beside it are the last twelve
colours painted with, and a click takes one back.

The canvas:

* left button: the tool;
* any other button: pan;
* wheel: zoom about the pointer, from 10 % to 6400 %; a pixel grid shows
  from 800 %.

The pointer shows the brush outline, red for the Eraser. The status line reads
``width x height  zoom%  x,y``.

Save, undo and normal maps
--------------------------

* **Ctrl+S**, or *Save the PNG*, writes the image back to its file.
* *Revert to the saved PNG* drops the unsaved changes; the revert itself can
  be undone.
* **Ctrl+Z** / **Ctrl+Y** (or **Ctrl+Shift+Z**) undo and redo. The Image
  Editor keeps its own history of 40 steps, apart from the scene's, and
  starts a new one each time an image is opened.
* *Make a normal map beside the image* writes ``name_normal.png`` next to the
  image, from the pixels as they are on screen (unsaved changes included),
  replacing any file of that name. Drop it on a Sprite's ``normalMap`` field
  and Light2D lights it with relief.

The keys work while the Image Editor has the focus: click the canvas first.

.. warning::

   * Opening another image while this one has unsaved changes **saves them
     without asking**, and quitting the editor does not ask about them.
   * Saving always writes PNG data, whatever the file's extension: keep the
     images you paint as ``.png``.
