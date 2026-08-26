import math

class FireworkTrail(ScriptComponent):
    def on_start(self):
        self.life = math.random(0.70, 1.15)
        self.start_life = self.life
        self.vx = math.random(-20, 20)
        self.vy = math.random(20, 70)
        self.sprite = self.node.get_sprite()

    def on_update(self, dt):
        self.life = self.life - dt
        self.vx = self.vx * (1.0 - dt * 2.0)
        self.vy = self.vy + 55 * dt
        self.node.translate(self.vx * dt, self.vy * dt)

        if self.sprite != None:
            alpha = math.max(0, self.life / self.start_life * 220)
            self.sprite.set_color(255, 155, 45, alpha)

        if self.life <= 0:
            self.node.queue_destroy()
