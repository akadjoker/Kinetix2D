import math

class Orbit:
    def __init__(self, node):
        self.node = node
        self.radius = 120
        self.speed = 2.0
        self._t = 0.0

    def on_start(self):
        target = self.node.find("player")
        if target != None:
            print("orbiting around:", target.get_name())

    def on_update(self, dt):
        self._t = self._t + dt * self.speed
        target = self.node.find("player")
        if target != None:
            cx, cy = target.get_position()
            self.node.set_position(cx + math.cos(self._t) * self.radius,
                                   cy + math.sin(self._t) * self.radius)
        self.node.rotate(180 * dt)
