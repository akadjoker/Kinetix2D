# Kinetix2D API style

New public APIs use `camelCase`: `setPosition`, `getComponent`, `loadTexture`,
`isPlaying`. Existing PascalCase methods remain as compatibility aliases while
projects migrate; new code should use the camelCase spelling.

Type names distinguish purpose:

- `CircleShape` and `RectShape` draw visual geometry only.
- `CircleCollider2D` and `BoxCollider2D` are physics geometry and require a
  `RigidBody2D` to take part in simulation.

Components are named by role (`SpriteComponent`, `CameraComponent`) except for
established 2D drawing nodes such as `Polygon2D` and `Line2D`. Prefer a clear
role suffix for new components.
