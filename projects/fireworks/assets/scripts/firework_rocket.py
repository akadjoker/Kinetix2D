import math

class FireworkRocket(ScriptComponent):
    def on_start(self):
        self.life = math.random(0.85, 1.45)
        self.vx = math.random(-45, 45)
        self.vy = math.random(-520, -400)
        self.trail_timer = 0
        self.sprite = self.node.get_sprite()
        self.r = math.random(170, 255)
        self.g = math.random(90, 210)
        self.b = math.random(130, 255)
        if self.sprite != None:
            self.sprite.set_color(self.r, self.g, self.b, 255)

    def on_update(self, dt):
        self.life = self.life - dt
        self.vy = self.vy + 230 * dt
        self.node.translate(self.vx * dt, self.vy * dt)

        self.trail_timer = self.trail_timer - dt
        if self.trail_timer <= 0:
            self.trail_timer = 0.012
            x, y = self.node.get_position()
            self.node.spawn("prefabs/firework_trail.k2dprefab", x, y)
            self.node.spawn("prefabs/firework_trail.k2dprefab", x, y)
            self.node.spawn("prefabs/firework_trail.k2dprefab", x, y)

        if self.life <= 0:
            x, y = self.node.get_position()
            self.node.spawn("prefabs/firework_explosion.k2dprefab", x, y)
            self.node.queue_destroy()
