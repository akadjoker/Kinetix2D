import math


class Smoke(ScriptComponent):
    horizontal_speed = 35.0
    upward_speed_min = 80.0
    upward_speed_max = 130.0
    gravity = 180.0
    finished_event = "smoke_finished"

    def on_start(self):
        self.vx = math.random(-self.horizontal_speed, self.horizontal_speed)
        self.vy = -math.random(self.upward_speed_min, self.upward_speed_max)
        self.anim = self.node.get_component<Animation>()
        if self.anim != None:
            self.anim.play("default")

    def on_update(self, dt):
        self.vy = self.vy + self.gravity * dt
        self.node.translate(self.vx * dt, self.vy * dt)

    def on_event(self, name, value):
        if name == self.finished_event:
            self.node.queue_destroy()
