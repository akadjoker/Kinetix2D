class BunnymarkBunny(ScriptComponent):
    gravity = 780
    floor_y = 700
    ceiling_y = 18
    left_x = 14
    right_x = 1266

    def on_start(self):
        left, top, right, bottom = world_view_rect()
        self.left_x = left + 14
        self.right_x = right - 14
        self.ceiling_y = top + 18
        self.floor_y = bottom - 20
        self.id = get_number("bunnymark_next_id", 0)
        set_number("bunnymark_next_id", self.id + 1)
        self.vx = 120 + (self.id % 7) * 32
        if self.id % 2 == 0:
            self.vx = -self.vx
        self.vy = -260 - (self.id % 5) * 38

    def on_update(self, dt):
        self.vy = self.vy + self.gravity * dt
        x = self.node.get_x() + self.vx * dt
        y = self.node.get_y() + self.vy * dt

        if x < self.left_x:
            x = self.left_x
            self.vx = -self.vx
        elif x > self.right_x:
            x = self.right_x
            self.vx = -self.vx

        if y < self.ceiling_y:
            y = self.ceiling_y
            self.vy = -self.vy
        elif y > self.floor_y:
            y = self.floor_y
            self.vy = -self.vy * 0.82

        self.node.set_position(x, y)
