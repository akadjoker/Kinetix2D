# Prompt: icon set for the Kinetix2D editor

Format is the one `prisma3d/editor/icons` and ZenHuman already load: one PNG atlas on a 32 px
grid and an `atlas.txt` with one line per icon, `Name x y 32 32`, names in PascalCase exactly
as listed below. Hand the prompt to the image tool as is; the names are what the editor code
will look up, so they must not change.

---

Design a flat, monochrome icon set for a 2D game editor (hierarchy tree, toolbar and
inspector), in the style of the Godot and Unity editor icons.

Rules:
- Every icon fits a 32x32 px cell with 3 px of transparent margin on each side; the drawing
  is centred and reads at 16x16 when scaled down.
- Single colour: light grey #E6E6E6 on a fully transparent background, anti-aliased, no
  gradients, no shadows, no outlines of a second colour. Strokes are 2 px at 32 px.
- One visual language for the whole set: rounded corners of 2 px, 45 degree diagonals, the
  same stroke weight everywhere, filled shapes only where the metaphor needs a solid (a body,
  a swatch).
- Deliver one PNG, 512 px wide, 16 cells per row, 8 rows (512x256), cells filled left to
  right, top to bottom, in the order below, and an `atlas.txt` with one line per icon:
  `Name x y 32 32`, where x and y are the cell's top-left pixel.

Icons, in this order (Name: what to draw):

Tree, scene objects
1. Object: a small hollow circle with a dot in the centre (a plain node)
2. Scene: a clapperboard, or a rectangle with a small circle and square inside
3. Prefab: a cube with a small chain link
4. Sprite: a picture frame with a mountain and sun
5. SpriteBatch: three overlapping picture frames
6. Animation: a film strip with two frames
7. Camera: a classic camera body with a lens
8. TileMap: a 3x3 grid of squares with two filled
9. Polygon2D: an irregular pentagon outline
10. Light2D: a light bulb with three rays
11. DirectionalLight2D: a sun with parallel arrows leaving it
12. LightOccluder: a wall segment casting a dark wedge
13. Line2D: a polyline with three points
14. NinePatch: a square with a 3x3 slice grid, corners filled
15. Particle: a few small circles spraying from a point
16. AudioPlayer: a speaker with two sound arcs
17. RigidBody2D: a filled circle with a small motion arrow
18. CharacterBody2D: a standing stick figure
19. BoxCollider2D: a dashed square outline
20. CircleCollider2D: a dashed circle outline
21. EdgeCollider2D: a single dashed line
22. PolygonCollider2D: a dashed pentagon outline
23. ChainCollider2D: a dashed polyline with a small link
24. TileMapCollider2D: a 2x2 grid with a dashed border
25. DistanceJoint2D: two dots joined by a straight bar
26. RevoluteJoint2D: a hinge, two bars sharing a pin
27. WheelJoint2D: a wheel on a spring
28. MotorJoint2D: a gear with a rotation arrow
29. MouseJoint2D: a pointer arrow with a short spring
30. GearJoint2D: two meshing gears
31. CircleShape: a filled circle
32. RectShape: a filled rectangle
33. CapsuleShape: a filled capsule
34. UiCanvas: a window frame with a title bar
35. UiPanel: a rounded rectangle
36. UiLabel: a letter T
37. UiButton: a rounded rectangle with a short line inside
38. UiCheckBox: a square with a check mark
39. UiSlider: a horizontal track with a knob
40. NavigationRegion: a flat area outline with a path through it
41. NavigationAgent: a pin with a dotted path behind it
42. MotionTween: two keyframes joined by an easing curve
43. MotionStreak: a comet tail
44. Skeleton: three connected bones
45. Bone: a single bone
46. ParallaxLayer: three horizontal layers offset like mountains
47. Steering: a steering wheel
48. Formation: four dots in a V arrangement
49. PathMotion: a curve with a dot moving along it
50. ActionSequence: three blocks in a row with arrows between them
51. Script: a sheet of paper with a snake curve (the Zen script)
52. Folder: a folder
53. Texture: a picture file
54. SceneFile: a file with the scene mark
55. ScriptFile: a file with the script mark
56. AudioFile: a file with a note

Tree state
57. VisibilityVisible: an open eye
58. VisibilityHidden: a closed eye, or an eye with a slash
59. Lock: a closed padlock
60. Unlock: an open padlock
61. Warning: a triangle with an exclamation mark
62. Error: a circle with an X
63. Search: a magnifying glass
64. Filter: a funnel

Toolbar, file and edit
65. New: a blank page with a plus
66. Open: an open folder
67. Save: a floppy disk
68. SaveAs: a floppy disk with a pencil
69. Undo: a curved arrow to the left
70. Redo: a curved arrow to the right
71. Add: a plus
72. Remove: a trash bin
73. Duplicate: two overlapping squares
74. Settings: a gear
75. Export: a box with an arrow leaving it
76. ExportWeb: a globe with an arrow
77. Refresh: a circular arrow
78. ReloadScripts: a script sheet with a circular arrow

Toolbar, play
79. Play: a right-pointing triangle
80. Pause: two vertical bars
81. Stop: a square
82. Step: a triangle with a bar after it

Toolbar, viewport tools
83. Select: a pointer arrow
84. Move: four arrows from a centre
85. Rotate: a circular arrow with a centre dot
86. Scale: a square with a diagonal arrow at one corner
87. Grid: a 3x3 grid of thin lines
88. Snap: a magnet
89. ZoomIn: a magnifying glass with a plus
90. ZoomOut: a magnifying glass with a minus
91. ZoomFit: four corner brackets
92. Center: a crosshair
93. EditorCamera: a camera with a small eye
94. ResetLayout: a window with its panes outlined
95. Console: a terminal prompt `>_`
96. Inspector: a list with three lines and a bullet each

Cells 97 to 128 stay empty and transparent.
