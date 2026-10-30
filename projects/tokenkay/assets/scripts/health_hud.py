import math


class HealthHud(ScriptComponent):
    x = 24.0
    y = 42.0
    width = 190.0
    height = 14.0

    def on_start(self):
        self.last_hp = get_number("player_hp", 50.0)
        self.trailing_hp = self.last_hp
        self.hit_flash = 0.0

    def on_update(self, dt):
        hp = get_number("player_hp", 50.0)
        if hp < self.last_hp:
            self.hit_flash = 0.18
        self.last_hp = hp

        # The orange delayed bar makes every lost chunk of energy easy to see.
        follow = dt * 5.0
        if follow > 1.0:
            follow = 1.0
        self.trailing_hp = self.trailing_hp + (hp - self.trailing_hp) * follow
        if math.abs(self.trailing_hp - hp) < 0.05:
            self.trailing_hp = hp

        if self.hit_flash > 0.0:
            self.hit_flash = self.hit_flash - dt

    def on_draw_ui(self):
        hp = get_number("player_hp", 50.0)
        max_hp = get_number("player_max_hp", 50.0)
        if max_hp <= 0.0:
            max_hp = 1.0
        ratio = hp / max_hp
        trailing_ratio = self.trailing_hp / max_hp
        if ratio < 0.0:
            ratio = 0.0
        if ratio > 1.0:
            ratio = 1.0
        if trailing_ratio < 0.0:
            trailing_ratio = 0.0
        if trailing_ratio > 1.0:
            trailing_ratio = 1.0

        set_draw_color(0.0, 0.0, 0.0, 0.78)
        draw_rect(self.x - 4.0, self.y - 4.0, self.width + 8.0, self.height + 8.0, True)

        set_draw_color(0.22, 0.06, 0.04, 1.0)
        draw_rect(self.x, self.y, self.width, self.height, True)

        set_draw_color(0.95, 0.42, 0.08, 1.0)
        draw_rect(self.x, self.y, self.width * trailing_ratio, self.height, True)

        if ratio > 0.55:
            set_draw_color(0.15, 0.86, 0.28, 1.0)
        elif ratio > 0.25:
            set_draw_color(1.0, 0.72, 0.08, 1.0)
        else:
            set_draw_color(0.95, 0.12, 0.08, 1.0)
        draw_rect(self.x, self.y, self.width * ratio, self.height, True)

        if self.hit_flash > 0.0:
            set_draw_color(1.0, 0.12, 0.08, 1.0)
        else:
            set_draw_color(0.82, 0.86, 0.82, 1.0)
        draw_rect(self.x - 2.0, self.y - 2.0, self.width + 4.0, self.height + 4.0, False, 2.0)

        set_draw_color(1.0, 1.0, 1.0, 1.0)
        draw_text(self.x, self.y - 24.0, "POWER  " + str(int(hp)) + " / " + str(int(max_hp)), 16)

        wave = int(get_number("wave", 1.0))
        enemies = int(get_number("enemies_alive", 0.0))
        pending = int(get_number("enemies_pending", 0.0))
        set_draw_color(0.92, 0.92, 0.82, 1.0)
        draw_text(self.x, self.y + 24.0, "ONDA " + str(wave) + "   ROJAS " + str(enemies + pending), 16)

        if get_flag("player_dead", False):
            set_draw_color(1.0, 0.18, 0.12, 1.0)
            draw_text(self.x, self.y + 48.0, "KILLED", 18)
