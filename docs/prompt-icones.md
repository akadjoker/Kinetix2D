# Prompt: icon sheet for the Kinetix2D editor

Same format as the ZenStage and ZenHuman sheets. The names below each row are what the
editor looks up in `atlas.txt` (`Name x y w h`), cell by cell in the same order.

---

A sprite sheet of 96 flat minimalist UI icons for a 2D game engine editor,
arranged in a strict 8 columns by 12 rows grid, each cell exactly square, equal
spacing, no labels, no text, no borders. Style: simple solid glyphs in medium
grey (#9A9A9A) with small red (#E04040) accents, plain white background, clean
geometric shapes, consistent 2px stroke weight, centered in each cell, same
visual size. Row by row, left to right:

Row 1: plain node small circle with center dot, scene clapperboard, prefab cube with
a chain link, sprite picture frame with mountain and sun, three overlapping sprite
frames (sprite batch), animation film strip with two frames, 2D camera body with lens,
tile map 3x3 grid with two filled cells.
Row 2: polygon pentagon outline, point light bulb with three rays, directional light
sun with parallel arrows, light occluder wall casting a dark wedge, polyline with three
points, nine patch square with a 3x3 slice grid and filled corners, particles spraying
from a point, audio speaker with two sound arcs.
Row 3: rigid body filled circle with a motion arrow, character body stick figure,
box collider dashed square, circle collider dashed circle, edge collider single dashed
line, polygon collider dashed pentagon, chain collider dashed polyline with a link,
tile map collider 2x2 grid with dashed border.
Row 4: distance joint two dots joined by a bar, revolute joint hinge of two bars on a
pin, wheel joint wheel on a spring, motor joint gear with rotation arrow, mouse joint
pointer arrow with a short spring, gear joint two meshing gears, circle shape filled
circle, rectangle shape filled rectangle.
Row 5: capsule shape filled capsule, UI canvas window frame with title bar, UI panel
rounded rectangle, UI label letter T, UI button rounded rectangle with a short line,
UI checkbox square with a check mark, UI slider track with a knob, navigation region
flat area outline with a path through it.
Row 6: navigation agent pin with a dotted path behind it, motion tween two keyframe
diamonds joined by an easing curve, motion streak comet tail, skeleton three connected
bones, single bone, parallax three offset mountain layers, steering wheel, formation
four dots in a V.
Row 7: path motion curve with a dot on it, action sequence three blocks with arrows
between them, script paper sheet with a snake curve, folder, texture image file, scene
file with clapperboard mark, script file with snake mark, audio file with a note.
Row 8: open eye (visible), closed eye with a slash (hidden), closed padlock (lock),
open padlock (unlock), warning triangle with exclamation mark, error circle with an X,
magnifying glass (search), funnel (filter).
Row 9: new blank page with plus, open folder, save floppy, save-as floppy with a
pencil, undo curved arrow left, redo curved arrow right, add plus, remove trash bin.
Row 10: duplicate two overlapping squares, settings gear, export box with arrow leaving
it, export to web globe with arrow, refresh circular arrow, reload scripts sheet with
circular arrow, play right-pointing triangle with red accent, pause two vertical bars.
Row 11: stop square, step triangle with a bar after it, select pointer arrow, move four
arrows from a center, rotate circular arrow with a center dot, scale square with a
diagonal corner arrow, grid 3x3 thin lines, snap magnet.
Row 12: zoom in magnifier with plus, zoom out magnifier with minus, zoom fit four corner
brackets, center crosshair, editor camera with a small eye, reset layout window with its
panes outlined, console terminal prompt, inspector list with three bullet lines.

---

Names for `atlas.txt`, in the same order:

Row 1: Object Scene Prefab Sprite SpriteBatch Animation Camera TileMap
Row 2: Polygon2D Light2D DirectionalLight2D LightOccluder Line2D NinePatch Particle AudioPlayer
Row 3: RigidBody2D CharacterBody2D BoxCollider2D CircleCollider2D EdgeCollider2D PolygonCollider2D ChainCollider2D TileMapCollider2D
Row 4: DistanceJoint2D RevoluteJoint2D WheelJoint2D MotorJoint2D MouseJoint2D GearJoint2D CircleShape RectShape
Row 5: CapsuleShape UiCanvas UiPanel UiLabel UiButton UiCheckBox UiSlider NavigationRegion
Row 6: NavigationAgent MotionTween MotionStreak Skeleton Bone ParallaxLayer Steering Formation
Row 7: PathMotion ActionSequence Script Folder Texture SceneFile ScriptFile AudioFile
Row 8: VisibilityVisible VisibilityHidden Lock Unlock Warning Error Search Filter
Row 9: New Open Save SaveAs Undo Redo Add Remove
Row 10: Duplicate Settings Export ExportWeb Refresh ReloadScripts Play Pause
Row 11: Stop Step Select Move Rotate Scale Grid Snap
Row 12: ZoomIn ZoomOut ZoomFit Center EditorCamera ResetLayout Console Inspector
