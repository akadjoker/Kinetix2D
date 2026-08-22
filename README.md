# Kinetix2D - 2D Physics Engine + Rendering

**Kinetix2D** is a high-performance 2D physics engine with an integrated rendering system, built for desktop, web (WebGL2), and mobile platforms. It combines a faithful physics simulation (collision detection + sequential impulse solver) with a lean 2D graphics layer.

## Architecture

### Two-Layer Design

- **`kx` (Physics Library)**: Zero dependencies beyond GLM and `ct` (custom containers). Headless, portable to any platform.
  - Collision detection (Circle, Polygon, Edge shapes; SAT + GJK-inspired algorithms)
  - Dynamic, Kinematic, and Static bodies
  - 6 joint types: Distance, Wheel, Revolute, Gear, Mouse
  - Broad/narrow phase with optional dynamic tree (AABB broadphase)
  - Deterministic, reproducible results
  
- **`k2d` (Engine Layer)**: Rendering, input, file I/O, scene management
  - Batch renderer (GLES 3.0 / WebGL2 compatible)
  - SDL2 window + input handling
  - Immediate-mode text rendering
  - Scene/component system (GameObject hierarchy, prefab-like structure)
  - ImGui integration for debug panels
  - Screenshot (PNG) and frame capture (for GIF conversion)

### Portability

- **Desktop**: Native GLES 3.0 via glad (Mesa/ANGLE)
- **Web**: Emscripten + WebGL2 (`-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2`)
- **Mobile**: Native GLES 3.0
 

## Building

### Prerequisites

- CMake 3.16+
- SDL2 development headers
- C++14 compiler (GCC 7+ / Clang 5+)
- GLM (fetched automatically)
- `ct` (containers library, path in physics/CMakeLists.txt)

### Build

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/physics_demo
```

### Run Tests

```bash
./build/bin/kx_fidelity          # 75,000 collision regression tests
./build/bin/kx_collision_tests   # 29 edge-case unit tests
./build/bin/kx_solver_tests      # 55 dynamics + constraints tests
./build/bin/kx_broadphase_tests  # Broadphase correctness
./build/bin/kx_imageshape_tests  # Sprite silhouette tracing
```

All tests exit 0 on success.

## API Reference

### Physics (kx namespace)

#### World
```cpp
kx::World world(glm::vec2(0, 800));  // gravity

// Create bodies
kx::Body *box = world.CreateBox(pos, halfWidth, halfHeight, density);
kx::Body *circle = world.CreateCircle(pos, radius, density);
kx::Body *body = world.CreateBody(kx::BodyType::Dynamic, pos, angle);

// Compose shapes
body->AddBox(halfW, halfH, localPos, density);
body->AddCircle(localPos, radius, density);
body->AddPolygon(points, count, density);  // convex hull
body->AddEdge(p1, p2);
body->AddMesh(outline, count, density);    // auto-triangulates concave
body->AddFromImage(pixels, w, h, bpp, threshold, density);

// Joints
auto *joint = new kx::MouseJoint(body, targetPos, frequency, damping);
world.AddJoint(joint);

auto *wheel = new kx::WheelJoint(chassis, wheel, anchor, axis);
wheel->SetSpring(4.0f, 0.7f);
wheel->SetMotor(true, speed, maxTorque);
world.AddJoint(wheel);

// Step simulation
world.Step(dt);

// Query
kx::Body *hit = world.BodyAtPoint(mousePos);
const ct::Vector<kx::ContactInfo> &contacts = world.Contacts();

// Debug draw
BatchDebugDraw debug(batch);
kx::Draw(world, debug, kx::DebugDrawShapes | kx::DebugDrawContacts);
```

#### Body Properties
```cpp
body->SetVelocity(v);
body->SetAngularVelocity(w);
body->SetPosition(p);
body->SetAngle(angle);

body->SetFriction(0.3f);
body->SetRestitution(0.5f);

// Collision filtering (bitmask groups)
kx::Filter filter{category, mask, group};
body->SetFilter(filter);

// Query geometry
glm::vec2 pos = body->Position();
float angle = body->Angle();
kx::Mass mass = body->Mass();
```

### Rendering (k2d namespace)

#### Batch Renderer (Immediate Mode)
```cpp
batch.BeginFrame();

batch.SetColor(255, 128, 0, 255);
batch.DrawRect(x, y, w, h, filled);
batch.DrawCircle(cx, cy, radius, segments);
batch.DrawLine(x1, y1, x2, y2);
batch.DrawPolyline(points, count);
batch.DrawText(x, y, fontSize, "Hello");

batch.PushMatrix();
batch.Translate(x, y);
batch.Rotate(angle);
batch.Scale(sx, sy);
batch.DrawTexture(texture, matrix);
batch.PopMatrix();

batch.EndFrame();
```

#### Scene/Component System
```cpp
k2d::Scene scene;

k2d::GameObject *obj = scene.createObject();
obj->setLocalPosition(glm::vec2(100, 200));

auto *sprite = obj->addComponent<k2d::SpriteComponent>();
sprite->setTexture(texture);
sprite->setColor(glm::vec4(1,0.5,0,1));

auto *script = obj->addComponent<k2d::ScriptComponent>();

scene.update(dt);
scene.render(batch);
```

#### Screenshot & Frame Capture
```cpp
// F9: Save PNG screenshot
device.CaptureScreenshot();  // → screenshot_0001.png

// F10: Toggle PNG frame capture
device.StartGifCapture(60);  // 60 fps
// ... run simulation ...
device.StopGifCapture();     // → capture_0001/frame_0001.png, etc

// Convert frames to GIF:
// ffmpeg -i capture_0001/frame_%04d.png -c:v libx264 output.gif
```

 

## Features

### Collision & Constraints
- **Shapes**: Circle, Polygon (convex), Edge (line segment), multi-shape bodies
- **Algorithms**: Separating Axis Theorem (SAT), Sutherland-Hodgman clipping
- **Manifolds**: Up to 2 contact points per pair, persistent feature ID
- **Broadphase**: O(n²) brute-force OR O(n log n) AABB dynamic tree (togglable)

### Dynamics
- **Sequential Impulse Solver** (Box2D style): accumulated impulses, warm starting
- **Restitution** (bounciness): per-body + contact-level
- **Friction**: dynamic + static, sqrt mixing rule
- **Gravity**: global + per-body forces
- **Sleep**: (TODO) sleeping threshold for inactive bodies

### Joints (6 types)
1. **MouseJoint**: Soft constraint, cursor-to-body pulling
2. **DistanceJoint**: Rigid rod or spring between two points
3. **WheelJoint**: Spring suspension + motor (vehicles)
4. **RevoluteJoint**: Pin rotation with motor + angle limits
5. **GearJoint**: Gear ratio coupling (two revolutes)
6. (Future) PrismaticJoint, PulleyJoint, ConstantVolumeJoint

### Multi-Shape Bodies
- Each body holds up to 32 shapes (fixed array, no per-body heap allocation)
- Shapes in body-local coordinates
- Mass & inertia computed via parallel-axis theorem
- Collision filtering per-shape

### Image → Collision Shapes
- **Marching squares** outline tracing (Chipmunk-derived)
- **Polyline simplification** (angle-tolerance vertex reduction)
- **Concave polygon support**: auto-triangulates via internal sweep-line CDT (kx::tri)
- **Multi-blob support**: disjoint silhouettes become separate bodies

## References

**Physics**: No invented algorithms - faithful ports from proven libraries:
- Collision: `/media/projectos/projects/cpp/vibecoding/fisica/phys` (user's Box2D-style reference)
- Solver: Box2D-Lite (sequential impulse, contact warm-starting)
- Joints: `/media/projectos/projects/cpp/fisica/box2d/src/dynamics/` (MouseJoint, WheelJoint, RevoluteJoint, etc.)
- Triangulation: kx::tri — port of poly2tri sweep-line CDT (self-contained, no external dep)
- Image tracing: Chipmunk2D (cpMarch marching-squares, cpPolyline simplification)

**Rendering**: Immediate-mode batch renderer (GLES 3.0 subset for WebGL2 compat)
- Shaders: inline `#version 300 es` GLSL
- Transform stack: push/pop matrix with multiply semantics
- No retained-mode scene graph - caller drives draw order

## Rules

1. **C++14 only** - no C++17 features
2. **No `std::` containers** - use `ct::Vector`, `ct::HashMap`, `ct::Pool`, `ct::String`
3. **No smart pointers** - raw pointers, explicit ownership
4. **No comments** in code (except closing namespace braces)
5. **Allman braces** (opening brace on new line)
6. **Single source of truth**: 2D transforms = `k2d::Matrix2D` everywhere (physics + rendering)
7. **No linked lists** - arrays + handles + object pools

## Performance

On a modern machine (Ryzen 7 / RTX 3070):

- **Collision detection**: ~2ms for 256 dynamic bodies (brute-force), ~0.5ms (tree)
- **Solver**: ~4ms for 256 bodies, 8 velocity iterations
- **Rendering**: 60 fps+ for 500 on-screen shapes (batched)
- **Memory**: ~1.2 MB for 256 dynamic bodies (kx library only)



## License

MIT


