SPEED = 200

def ready(node):
    print("player ready:", node.get_name())

def update(node, dt):
    dx = 0
    dy = 0
    if key_down("a") or key_down("left"):
        dx = dx - 1
    if key_down("d") or key_down("right"):
        dx = dx + 1
    if key_down("w") or key_down("up"):
        dy = dy - 1
    if key_down("s") or key_down("down"):
        dy = dy + 1
    node.translate(dx * SPEED * dt, dy * SPEED * dt)

    sprite = node.get_sprite()
    if sprite != None and dx != 0:
        sprite.set_flip(dx < 0, False)
