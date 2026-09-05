import math


class BloodEmitter(ScriptComponent):
    # This prefab is the animated blood effect itself.  Its movement is kept
    # here instead of using a ParticleComponent so every spawned copy can play
    # the Animation2D authored in the editor.
    horizontal_speed = 70.0
    upward_speed_min = 150.0
    upward_speed_max = 230.0
    gravity = 520.0
    finished_event = "blood_emitter_finished"

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
