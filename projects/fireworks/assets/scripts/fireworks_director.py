class FireworksDirector(ScriptComponent):
    rockets_per_second = 16

    def __init__(self):
        self.cooldown = 0
        self.launched = 0

    def on_update(self, dt):
        self.cooldown = self.cooldown - dt
        if mouse_down(0) and self.cooldown <= 0:
            self.cooldown = 1.0 / self.rockets_per_second
            x, y = mouse_world_position()
            left, top, right, bottom = world_view_rect()
            rocket = self.node.spawn("prefabs/firework_rocket.k2dprefab", x, bottom - 18)
            if rocket != None:
                self.launched = self.launched + 1

    def on_draw_ui(self):
        set_draw_color(1, 1, 1, 1)
        draw_text(12, 12, "Fireworks script stress test", 20)
        draw_text(12, 38, "Hold left mouse and drag to launch rockets", 16)
        draw_text(12, 60, "Rockets: " + str(self.launched) + "  Objects: " + str(object_count()), 16)
