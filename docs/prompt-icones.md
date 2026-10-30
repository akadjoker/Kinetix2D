# Prompt: icon sheets for the Kinetix2D editor

Same format as the ZenStage and ZenHuman sheets, split in three sheets of 32 so the model
keeps the grid. The names under each sheet are what the editor looks up in `atlas.txt`
(`Name x y w h`), cell by cell in the same order. Every sheet starts and ends with the
no-text rule, because the model tends to write captions under the icons.

---

## Sheet 1 (scene objects)

A sprite sheet of 32 flat minimalist UI icons for a 2D game engine editor, glyphs only:
absolutely no lettering, no words, no captions, no numbers anywhere in the image.
Arranged in a strict 8 columns by 4 rows grid, each cell exactly square, equal spacing,
no borders between cells. Style: simple solid glyphs in medium grey (#9A9A9A) with small
red (#E04040) accents, plain white background, clean geometric shapes, consistent 2px
stroke weight, centered in each cell, same visual size. Row by row, left to right:

Row 1: plain node small circle with a center dot, scene clapperboard, prefab cube with a
chain link, sprite picture frame with mountain and sun, three overlapping picture frames,
film strip with two frames, 2D camera body with a lens, 3x3 tile grid with two filled cells.
Row 2: pentagon outline, light bulb with three rays, sun with parallel arrows leaving it,
wall segment casting a dark wedge, polyline with three points, square with a 3x3 slice grid
and filled corners, small circles spraying from a point, speaker with two sound arcs.
Row 3: filled circle with a motion arrow, standing stick figure, dashed square, dashed
circle, single dashed line, dashed pentagon, dashed polyline with a small chain link, 2x2
grid with a dashed border.
Row 4: two dots joined by a straight bar, hinge of two bars on a pin, wheel on a spring,
gear with a rotation arrow, pointer arrow with a short spring, two meshing gears, filled
circle, filled rectangle.

Glyphs only, no text of any kind in the image.

Names: Object Scene Prefab Sprite SpriteBatch Animation Camera TileMap /
Polygon2D Light2D DirectionalLight2D LightOccluder Line2D NinePatch Particle AudioPlayer /
RigidBody2D CharacterBody2D BoxCollider2D CircleCollider2D EdgeCollider2D PolygonCollider2D
ChainCollider2D TileMapCollider2D /
DistanceJoint2D RevoluteJoint2D WheelJoint2D MotorJoint2D MouseJoint2D GearJoint2D
CircleShape RectShape

## Sheet 2 (more objects, files, tree state)

A sprite sheet of 32 flat minimalist UI icons for a 2D game engine editor, glyphs only:
absolutely no lettering, no words, no captions, no numbers anywhere in the image.
Arranged in a strict 8 columns by 4 rows grid, each cell exactly square, equal spacing,
no borders between cells. Style: simple solid glyphs in medium grey (#9A9A9A) with small
red (#E04040) accents, plain white background, clean geometric shapes, consistent 2px
stroke weight, centered in each cell, same visual size. Row by row, left to right:

Row 1: filled capsule, window frame with a title bar, rounded rectangle, a short thick
horizontal line above a longer thin one, rounded rectangle with a short bar inside, square
with a check mark, horizontal track with a round knob, flat area outline with a path through it.
Row 2: map pin with a dotted trail behind it, two diamonds joined by an easing curve, comet
with a tail, three connected bones, a single bone, three offset mountain layers, steering
wheel, four dots in a V arrangement.
Row 3: curve with a dot on it, three blocks in a row with arrows between them, sheet of
paper with a snake curve, folder, image file with a mountain, file with a clapperboard mark,
file with a snake mark, file with a music note.
Row 4: open eye, eye with a diagonal slash, closed padlock, open padlock, triangle with an
exclamation mark in red, circle with an X in red, magnifying glass, funnel.

Glyphs only, no text of any kind in the image.

Names: CapsuleShape UiCanvas UiPanel UiLabel UiButton UiCheckBox UiSlider NavigationRegion /
NavigationAgent MotionTween MotionStreak Skeleton Bone ParallaxLayer Steering Formation /
PathMotion ActionSequence Script Folder Texture SceneFile ScriptFile AudioFile /
VisibilityVisible VisibilityHidden Lock Unlock Warning Error Search Filter

## Sheet 3 (toolbar)

A sprite sheet of 32 flat minimalist UI icons for a 2D game engine editor, glyphs only:
absolutely no lettering, no words, no captions, no numbers anywhere in the image.
Arranged in a strict 8 columns by 4 rows grid, each cell exactly square, equal spacing,
no borders between cells. Style: simple solid glyphs in medium grey (#9A9A9A) with small
red (#E04040) accents, plain white background, clean geometric shapes, consistent 2px
stroke weight, centered in each cell, same visual size. Row by row, left to right:

Row 1: blank page with a plus, open folder, floppy disk, floppy disk with a pencil, curved
arrow to the left, curved arrow to the right, plus sign, trash bin.
Row 2: two overlapping squares, gear, box with an arrow leaving it, globe with an arrow,
circular arrow, sheet of paper with a circular arrow, right-pointing triangle with a red
accent, two vertical bars.
Row 3: square, triangle with a vertical bar after it, pointer arrow, four arrows from a
center, circular arrow around a center dot, square with a diagonal arrow at one corner,
3x3 grid of thin lines, horseshoe magnet.
Row 4: magnifying glass with a plus, magnifying glass with a minus, four corner brackets,
crosshair, camera with a small eye, window with its panes outlined, rectangle with a small
angle bracket mark inside, list of three lines with a bullet each.

Glyphs only, no text of any kind in the image.

Names: New Open Save SaveAs Undo Redo Add Remove /
Duplicate Settings Export ExportWeb Refresh ReloadScripts Play Pause /
Stop Step Select Move Rotate Scale Grid Snap /
ZoomIn ZoomOut ZoomFit Center EditorCamera ResetLayout Console Inspector
