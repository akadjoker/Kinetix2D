import math

class FireworkSpark(ScriptComponent):
    def on_start(self):
        angle = math.random(0, math.pi * 2)
        speed = math.random(100, 310)
        self.vx = math.cos(angle) * speed
        self.vy = math.sin(angle) * speed
        self.life = math.random(0.65, 1.35)
        self.start_life = self.life
        self.r = math.random(120, 255)
        self.g = math.random(80, 230)
        self.b = math.random(100, 255)
        self.sprite = self.node.get_sprite()

    def on_update(self, dt):
        self.life = self.life - dt
        self.vy = self.vy + 280 * dt
        self.vx = self.vx * (1.0 - dt * 0.8)
        self.node.translate(self.vx * dt, self.vy * dt)

        if self.sprite != None:
            alpha = math.max(0, self.life / self.start_life * 255)
            self.sprite.set_color(self.r, self.g, self.b, alpha)

        if self.life <= 0:
            self.node.queue_destroy()
