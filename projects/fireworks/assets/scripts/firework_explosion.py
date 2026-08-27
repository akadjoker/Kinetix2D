import math

class FireworkExplosion(ScriptComponent):
    def on_start(self):
        self.life = math.random(0.70, 0.95)
        self.start_life = self.life
        self.sprite = self.node.get_sprite()
        self.r = math.random(180, 255)
        self.g = math.random(100, 230)
        self.b = math.random(120, 255)
        x, y = self.node.get_position()
        for i in range(260):
            self.node.spawn("prefabs/firework_spark.k2dprefab", x, y)

    def on_update(self, dt):
        self.life = self.life - dt
        progress = 1.0 - math.max(0, self.life / self.start_life)
        self.node.set_scale(0.25 + progress * 1.5, 0.25 + progress * 1.5)

        if self.sprite != None:
            alpha = math.max(0, (1.0 - progress) * 230)
            self.sprite.set_color(self.r, self.g, self.b, alpha)

        if self.life <= 0:
            self.node.queue_destroy()
