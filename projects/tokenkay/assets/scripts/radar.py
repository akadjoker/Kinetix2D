import math

ENEMY_TAG = "enemy"
PLAYER_NAME = "Player"

# The radar disc is a 112x112 sprite pivoted at its centre, so blips live
# inside this radius of the node's own origin.
RADIUS = 52.0

# How much world fits on the disc. Anything past this clamps to the rim, the
# way the DIV original keeps off-screen contacts on the edge instead of
# dropping them.
WORLD_RANGE = 900.0


class Radar(ScriptComponent):
    blip_size = 3.0
    player_blip_size = 4.0
    sweep_speed = 90.0
    # Drawn above the disc sprite and every world object. on_draw() uses the
    # node's own zIndex, so the radar node has to out-rank the scene.
    show_sweep = True

    def on_start(self):
        self.enemies = []
        self.rescan_timer = 0.0
        self.sweep_angle = 0.0
        self.player = self.node.find(PLAYER_NAME)
        self.rescan()

    def rescan(self):
        # Rebuilt on a timer rather than every frame: walking the root's
        # children is the only way to enumerate by tag, and the wave director
        # only spawns a few times a second.
        self.enemies = []
        root = self.node.get_root()
        if root == None:
            return
        i = 0
        while i < root.child_count():
            child = root.get_child(i)
            if child != None and child.get_tag() == ENEMY_TAG:
                self.enemies.append(child)
            i = i + 1

    def on_update(self, dt):
        self.rescan_timer = self.rescan_timer - dt
        if self.rescan_timer <= 0.0:
            self.rescan_timer = 0.5
            self.rescan()
            if self.player == None:
                self.player = self.node.find(PLAYER_NAME)

        self.sweep_angle = self.sweep_angle + self.sweep_speed * dt
        if self.sweep_angle >= 360.0:
            self.sweep_angle = self.sweep_angle - 360.0

    def blip_position(self, center_x, center_y, target_x, target_y):
        dx = target_x - center_x
        dy = target_y - center_y
        distance = math.sqrt(dx * dx + dy * dy)
        scale = RADIUS / WORLD_RANGE
        bx = dx * scale
        by = dy * scale
        # Clamp to the rim instead of letting a distant contact fall off the
        # disc: knowing the direction of something far away is the point.
        limit = distance * scale
        if limit > RADIUS:
            factor = RADIUS / limit
            bx = bx * factor
            by = by * factor
        return bx, by

    def on_draw(self):
        if self.player == None:
            return

        center_x, center_y = self.player.get_global_position()
        origin_x, origin_y = self.node.get_global_position()

        if self.show_sweep:
            radians = math.radians(self.sweep_angle)
            set_draw_color(0.35, 0.95, 0.45, 0.35)
            draw_line(origin_x, origin_y,
                      origin_x + math.cos(radians) * RADIUS,
                      origin_y + math.sin(radians) * RADIUS, 1.5)

        i = 0
        while i < len(self.enemies):
            enemy = self.enemies[i]
            i = i + 1
            if enemy == None:
                continue
            enemy_x, enemy_y = enemy.get_global_position()
            bx, by = self.blip_position(center_x, center_y, enemy_x, enemy_y)
            set_draw_color(0.95, 0.25, 0.20, 0.95)
            draw_circle(origin_x + bx, origin_y + by, self.blip_size, True)

        set_draw_color(0.35, 0.95, 0.45, 1.0)
        draw_circle(origin_x, origin_y, self.player_blip_size, True)
