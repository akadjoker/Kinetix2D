SPEED = 200

class Player(ScriptComponent):
    def __init__(self):
        self.speed = SPEED

    def on_update(self, dt):
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
        self.node.translate(dx * self.speed * dt, dy * self.speed * dt)

        sprite = self.node.get_sprite()
        if sprite != None and dx != 0:
            sprite.set_flip(dx < 0, False)
