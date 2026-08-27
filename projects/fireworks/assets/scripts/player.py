class Player(ScriptComponent):
    speed = 200
    nome = "Luis"

    def on_update(self, dt):
        dx = 0
        dy = 0
        if key_down(KEY_A) or key_down(KEY_LEFT):
            dx = dx - 1
        if key_down(KEY_D) or key_down(KEY_RIGHT):
            dx = dx + 1
        if key_down(KEY_W) or key_down(KEY_UP):
            dy = dy - 1
        if key_down(KEY_S) or key_down(KEY_DOWN):
            dy = dy + 1
        self.node.translate(dx * self.speed * dt, dy * self.speed * dt)

        sprite = self.node.get_sprite()
        if sprite != None and dx != 0:
            sprite.set_flip(dx < 0, False)
