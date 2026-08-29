import math


# The original tokenkai prota: left mouse walks a pathfound route to the click,
# right mouse aims at the cursor and fires the selected weapon.
WEAPON_GUN = 0
WEAPON_GRENADE = 1


class Player(ScriptComponent):
    speed = 150
    fire_interval = 0.12
    grenade_interval = 0.6
    bullet_range = 700
    bullet_damage = 1

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        if self.agent != None:
            self.agent.set_max_speed(self.speed)
            self.agent.set_auto_move(True)
            self.agent.set_orient_to_path(False)
        self.cooldown = 0.0
        self.weapon = WEAPON_GUN
        self.bullets = 200
        self.grenades = 5
        set_flag("player_ready", self.agent != None)

    def on_update(self, dt):
        if self.cooldown > 0.0:
            self.cooldown = self.cooldown - dt

        mx, my = mouse_world_position()
        x, y = self.node.get_position()

        # The body always faces the cursor, whichever way it is walking.
        self.node.set_rotation(math.degrees(math.atan2(my - y, mx - x)))

        if key_pressed(KEY_1):
            self.weapon = WEAPON_GUN
        if key_pressed(KEY_2):
            self.weapon = WEAPON_GRENADE

        if mouse_pressed(0):
            self.walk_to(mx, my)

        if mouse_down(1):
            self.fire(x, y, mx, my)

    def walk_to(self, mx, my):
        if self.agent == None:
            return
        # Refusing an unwalkable click beats clearing the current path for a
        # route that cannot exist and leaving the player stranded.
        if nav_point_free(mx, my):
            self.agent.set_target(mx, my)
            set_number("walk_x", mx)
            set_number("walk_y", my)

    def fire(self, x, y, mx, my):
        if self.cooldown > 0.0:
            return

        dx = mx - x
        dy = my - y
        length = math.sqrt(dx * dx + dy * dy)
        if length < 0.001:
            return
        dx = dx / length
        dy = dy / length

        if self.weapon == WEAPON_GUN:
            self.fire_gun(x, y, dx, dy)
        else:
            self.throw_grenade(x, y, dx, dy, length)

    def fire_gun(self, x, y, dx, dy):
        if self.bullets <= 0:
            return
        self.bullets = self.bullets - 1
        self.cooldown = self.fire_interval

        # Hitscan: the shot is resolved this frame, and the sparks are what the
        # player actually sees.
        hit, hx, hy = raycast(x, y, dx, dy, self.bullet_range)
        if hit != None:
            set_string("last_hit", hit.get_name())
            set_number("hits", get_number("hits", 0) + 1)
            particle_explode(hx, hy, 6, 40, 140, 0.25, "", 2, 0, "kill")
        else:
            set_number("misses", get_number("misses", 0) + 1)

        set_number("bullets", self.bullets)

    def throw_grenade(self, x, y, dx, dy, distance):
        if self.grenades <= 0:
            return
        self.grenades = self.grenades - 1
        self.cooldown = self.grenade_interval

        # Debris that bounces off the world, unlike the gun's one-frame sparks.
        blast = 220
        if distance < blast:
            blast = distance
        particle_explode(x + dx * blast, y + dy * blast, 24, 60, 260, 1.2, "", 3, 0, "bounce")
        set_number("grenades", self.grenades)
