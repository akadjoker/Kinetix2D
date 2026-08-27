The engine (``k2d``)
====================

``Device`` — window, input, main loop
-------------------------------------

Owns the SDL window, the GLES context and the frame timing. One instance per
application.

.. code-block:: cpp

   k2d::Device device;
   if (!device.Init("Game", 1280, 720, true, false))   // vsync, enableUi
       return 1;

   while (device.PollEvents())
   {
       float dt = device.DeltaTime();
       device.BeginUI();
       // ... update and render the scene ...
       device.EndUI();
       device.Swap();
   }

* ``Init(title, width, height, vsync = true, enableUi = true)`` — creates the
  window, the GL context and (optionally) the ImGui UI.
* ``PollEvents()`` / ``Swap()`` — pump events and present the frame.
* ``Width()`` / ``Height()`` — window size. ``DrawableWidth()`` /
  ``DrawableHeight()`` — framebuffer pixels, which can differ on HiDPI.
* ``DeltaTime()`` / ``FPS()`` — frame timing.
* ``GetInput()`` — the ``k2d::Input`` state for the frame.
* ``BeginUI()`` / ``EndUI()`` — wrap the ImGui frame; ``ImGuiWantsMouse()`` /
  ``ImGuiWantsKeyboard()`` tell whether the UI is capturing input.
* ``CaptureScreenshot()`` — writes a PNG. ``StartGifCapture(60)`` /
  ``StopGifCapture()`` / ``CaptureGifFrame()`` — PNG frame capture for GIFs.
* ``TimeSeconds()`` — a global clock in seconds.

Input
-----

``k2d::Input`` (``engine/include/k2d/Input.h``) is the per-frame input state:
``KeyDown(code)``, ``KeyPressed(code)``, ``KeyReleased(code)``, mouse state,
``MouseX()``/``MouseY()``, ``WheelY()`` and the physical key codes in
``k2d::Key``.

``k2d::InputActionMap`` (``engine/include/k2d/InputActionMap.h``) binds named
actions to keys:

.. code-block:: cpp

   k2d::GetInputActions().Bind("move_forward", SDL_SCANCODE_W);
   bool moving = k2d::GetInputActions().Down(input, "move_forward");

Scene, GameObject and Component
-------------------------------

``k2d::Scene`` owns the object hierarchy. The editor edits the same scene the
runner plays; Play serializes it into a runtime copy.

.. code-block:: cpp

   k2d::Scene scene;
   k2d::GameObject *obj = scene.createObject("Player");
   obj->setPosition(Math::Vec2(100.0f, 200.0f));

   k2d::SpriteComponent *sprite = obj->addComponent<k2d::SpriteComponent>();
   sprite->setTexture(assets.GetTexture("hero.png"));

   scene.update(dt);          // components update, disposed objects are flushed
   scene.render(canvas);      // build + draw through the canvas renderer

``k2d::GameObject`` (``engine/include/k2d/GameObject.h``)

* Hierarchy: ``parent()``, ``root()``, ``childCount()``, ``child(i)``,
  ``findChild(name, recursive)``, ``addChild`` / ``removeChild`` /
  ``deleteChild``, ``moveChildUp`` / ``moveChildDown``.
* State: ``name()``/``setName()``, ``tag()``/``setTag()``, ``active()`` /
  ``setActive()``, ``visible()``/``setVisible()``, ``locked()``/``setLocked()``,
  ``isActiveInHierarchy()``, ``dispose()``.
* Transform: ``position()``/``setPosition()``, ``rotationDegrees()`` /
  ``setRotationDegrees()``, ``scale()``/``setScale()``, ``translate()``,
  ``rotate()``, ``localTransform()`` / ``globalTransform()``,
  ``globalPosition()``, ``right()``, ``up()``.
* Components: ``addComponent<T>()``, ``getComponent<T>()``,
  ``getComponentAt<T>(i)``, ``componentCount<T>()``, ``contains<T>()``,
  ``removeComponent<T>()``.

``k2d::Component`` (``engine/include/k2d/Component.h``) is the base for every
component. Lifecycle hooks are protected: ``onAwake``, ``onStart``,
``onEnable``, ``onDisable``, ``onUpdate(dt)``, ``onLateUpdate(dt)``,
``onRender(RenderQueue&)``, ``onDestroy``. Public state: ``owner()``,
``type()``, ``id()``, ``active()``/``setActive()``.

Component types (the ``ComponentType`` enum)

Sprite, Script, Camera, TileMap, SpriteBatch, Polygon2D, Animation, Light,
Occluder, LinePath, NinePatch, Particle, AudioPlayer, RigidBody, Collider,
CircleShape, RectShape, CapsuleShape, UiCanvas, UiPanel, UiLabel, UiButton,
UiCheckBox, UiSlider, NavigationRegion, NavigationAgent, MotionTween,
MotionStreak, CharacterBody.

Several component types can share one ``ComponentType`` slot; the
``ComponentMatch<T>`` trait disambiguates them. This is how the visual shapes
and the colliders coexist without new enum entries.

Renderers
---------

* ``k2d::Batch`` (``Batch.h``) — the immediate-mode debug renderer: rects,
  circles, lines, text and simple unlit sprites. It owns one simple shader and
  never knows about lights or canvas items. Used for debug draw and HUDs.
* ``k2d::RenderQueue`` (``RenderQueue.h``) — the router. Collects
  ``RenderItem`` s from ``onRender`` and sorts them; it never touches GL.
* ``k2d::CanvasRenderer`` (``CanvasRenderer.h``) — the sprite mechanism. Owns
  the canvas shader and buffers and draws the item list, including point and
  directional lights and their shadow atlas.

Lighting

* ``Light2D`` / ``DirectionalLight2D`` — point and directional lights.
* ``LightOccluder2D`` — supplies polygon edges that cast shadows into the
  shadow atlas.
* Light parameters live in ``CanvasTypes.h`` (``PointLight``,
  ``RenderCommand``, ``RenderItem``, ``BlendMode``, ``kMaxPointLights``).

Serialization and prefabs
-------------------------

``k2d::Serializer`` (``engine/include/k2d/Serializer.h``) turns a scene into
``ct::Json`` and back. Component types register their own create/write/read
functions:

.. code-block:: cpp

   Serializer::RegisterType(ComponentType::Script, "ZenScript",
                            &createZenScript, &writeZenScript, &readZenScript,
                            &matchZenScript);
   ct::Json data = Serializer::WriteObject(scene.root(), &assets);
   GameObject *loaded = Serializer::ReadObject(scene, data, nullptr, &assets);

``k2d::Prefab`` (``engine/include/k2d/Prefab.h``) loads, saves and
instantiates a single object graph:

.. code-block:: cpp

   k2d::Prefab prefab;
   prefab.Load("assets/prefabs/ball.k2dprefab");
   k2d::GameObject *ball = prefab.Instantiate(scene);

Assets and the file system
--------------------------

* ``k2d::Assets`` (``engine/include/k2d/Assets.h``) loads textures and shaders
  by name with replace-on-reload semantics.
* ``k2d::FileSystem`` (``engine/include/k2d/FileSystem.h``) resolves asset
  paths and mounts ``.kpak`` packages:

  .. code-block:: cpp

     k2d::FileSystem::Instance().MountPack("game.kpak", "project-key");

* ``k2d::KPak`` (``engine/include/k2d/KPak.h``) is the archive reader used by
  the file system.

Audio
-----

``k2d::AudioEngine`` (``engine/include/k2d/AudioEngine.h``) loads sounds and
music, plays voices (with pitch, pan, positional playback and fades) and owns
the master/SFX/music buses. ``k2d::AudioPlayer`` is the scene component that
plays a sound on a GameObject. The script-side ``audio_*`` functions call the
same engine.

UI controls
-----------

Retained UI lives on a ``UiCanvas`` with ``UiPanel``, ``UiLabel``, ``UiButton``,
``UiCheckBox`` and ``UiSlider`` children. Layout uses normalized anchors plus
pixel offsets, so the UI is screen-space and ignores the world camera. The
default button skin is embedded in the engine. ``UiTheme`` holds the theme
textures and colors; ``VirtualPad`` provides touch controls on the same canvas.

Other systems
-------------

* ``ParticleComponent`` / ``ParticleSystem`` — deterministic particle systems.
* ``Animation2D`` — atlas-frame animation on a sprite, with play/stop/reset
  and one-shot behavior.
* ``TileMapComponent`` — renders a tile atlas; ``kx::TileMapCollider`` builds
  static collision from the solid cells.
* ``CameraComponent`` / ``Camera2D`` — viewport, projection, screen-to-world
  conversion and shake/trauma.
* ``SpriteAtlas``, ``SpriteBatch``, ``Polygon2D``, ``NinePatchComponent``,
  ``Line2D``, ``ParallaxLayer2D``, ``MotionTween2D``, ``MotionStreak2D``,
  ``ScreenFade``, ``SceneManager`` — the remaining scene features.
* ``AStar2D`` / ``AStarGrid2D`` — pathfinding; ``Navigation2D`` /
  ``NavigationAgent2D`` / ``NavigationRegion2D`` — navigation.
* ``Profiler`` / ``ProfilerUI`` — scoped frame profiling with an ImGui overlay.
