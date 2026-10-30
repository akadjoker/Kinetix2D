class FollowTouch(ScriptComponent):
    follow = 10.0  # range(1, 30)

    def on_update(self, dt):
        if not mouse_down(0):
            return
        tx, ty = mouse_world_position()
        x, y = self.node.get_position()
        t = min(1.0, self.follow * dt)
        self.node.set_position(x + (tx - x) * t, y + (ty - y) * t)
