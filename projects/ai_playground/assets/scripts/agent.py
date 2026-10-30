import math


# States mirror the enemy process of the original DIV tokenkai.prg:
# 0 idle, 1 pick patrol pair, 2..5 walk the pair with a pause at each end,
# 6 pick a roam destination, 7 roam, 8 chase, 9 attack, 10 flee.
IDLE = 0
PICK_PATROL = 1
PAUSE_A = 2
TO_B = 3
PAUSE_B = 4
TO_A = 5
PICK_ROAM = 6
ROAM = 7
CHASE = 8
ATTACK = 9
FLEE = 10


class Agent(ScriptComponent):
    target_name = "player"
    walk_speed = 90
    chase_speed = 160
    sight_range = 320
    sight_cone = 60
    attack_range = 90
    flee_distance = 480
    world_width = 1280
    world_height = 720

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        self.state = IDLE
        self.timer = 0.0
        self.ammo = 6
        self.point_a_x = 0.0
        self.point_a_y = 0.0
        self.point_b_x = 0.0
        self.point_b_y = 0.0
        self.has_pair = False
        set_flag("agent_ready", self.agent != None)

    # ---- helpers -------------------------------------------------------

    def player_node(self):
        return self.node.find(self.target_name)

    def distance_to(self, x, y):
        sx, sy = self.node.get_position()
        return math.sqrt((x - sx) * (x - sx) + (y - sy) * (y - sy))

    def random_walkable(self):
        # nav_point_free is path_free from DIV: never send the agent to dry
        # land, or the path request quietly fails and it stands still forever.
        tries = 0
        while tries < 24:
            tries = tries + 1
            x = math.random(0, self.world_width)
            y = math.random(0, self.world_height)
            if nav_point_free(x, y):
                return True, x, y
        return False, 0.0, 0.0

    def sees_player(self):
        player = self.player_node()
        if player == None:
            return False
        px, py = player.get_position()
        distance = self.distance_to(px, py)
        if distance > self.sight_range:
            return False
        if distance < self.attack_range:
            return True
        sx, sy = self.node.get_position()
        to_player = math.degrees(math.atan2(py - sy, px - sx))
        facing = self.node.get_rotation()
        delta = math.angle_delta(facing, to_player)
        if math.abs(delta) > self.sight_cone:
            return False
        # The chain colliders around the water double as walls for sight:
        # anything hit before the player means the view is blocked.
        hit, hx, hy = raycast(sx, sy, px - sx, py - sy, distance)
        return hit == None

    def go_to(self, x, y, speed):
        if self.agent == None:
            return
        self.agent.set_max_speed(speed)
        self.agent.set_auto_move(True)
        self.agent.clear_follow_target()
        self.agent.set_target(x, y)

    def stop(self):
        if self.agent == None:
            return
        self.agent.set_auto_move(False)
        self.agent.clear_path()

    def arrived(self):
        return self.agent == None or self.agent.is_finished() or self.agent.has_path() == False

    # ---- state machine -------------------------------------------------

    def on_update(self, dt):
        if self.agent == None:
            return
        self.timer = self.timer - dt
        set_number("agent_state", self.state)

        if self.state != FLEE and self.state != ATTACK and self.state != CHASE:
            if self.sees_player():
                self.state = CHASE

        if self.state == IDLE:
            self.stop()
            if self.timer <= 0.0:
                self.state = PICK_PATROL if math.random() < 0.5 else PICK_ROAM

        elif self.state == PICK_PATROL:
            ok_a, ax, ay = self.random_walkable()
            ok_b, bx, by = self.random_walkable()
            if ok_a and ok_b:
                self.point_a_x = ax
                self.point_a_y = ay
                self.point_b_x = bx
                self.point_b_y = by
                self.has_pair = True
                self.state = PAUSE_A
                self.timer = 0.5 + math.random(0, 2)
            else:
                self.state = IDLE
                self.timer = 1.0

        elif self.state == PAUSE_A:
            self.stop()
            if self.timer <= 0.0:
                self.go_to(self.point_b_x, self.point_b_y, self.walk_speed)
                self.state = TO_B

        elif self.state == TO_B:
            if self.arrived():
                self.state = PAUSE_B
                self.timer = 0.5 + math.random(0, 2)

        elif self.state == PAUSE_B:
            self.stop()
            if self.timer <= 0.0:
                self.go_to(self.point_a_x, self.point_a_y, self.walk_speed)
                self.state = TO_A

        elif self.state == TO_A:
            if self.arrived():
                self.state = PAUSE_A
                self.timer = 0.5 + math.random(0, 2)

        elif self.state == PICK_ROAM:
            ok, x, y = self.random_walkable()
            if ok:
                self.go_to(x, y, self.walk_speed)
                self.state = ROAM
            else:
                self.state = IDLE
                self.timer = 1.0

        elif self.state == ROAM:
            if self.arrived():
                self.state = IDLE
                self.timer = 0.5 + math.random(0, 2)

        elif self.state == CHASE:
            player = self.player_node()
            if player == None:
                self.state = IDLE
                return
            px, py = player.get_position()
            # Following by name lets the C++ side own the repath throttle, so
            # chasing costs one path every repath interval, not one per frame.
            self.agent.set_max_speed(self.chase_speed)
            self.agent.set_auto_move(True)
            self.agent.set_follow_target(self.target_name)
            if self.distance_to(px, py) < self.attack_range:
                self.state = ATTACK
                self.timer = 0.0

        elif self.state == ATTACK:
            player = self.player_node()
            if player == None:
                self.state = IDLE
                return
            px, py = player.get_position()
            distance = self.distance_to(px, py)
            self.stop()
            sx, sy = self.node.get_position()
            self.node.set_rotation(math.degrees(math.atan2(py - sy, px - sx)))
            if self.timer <= 0.0:
                self.ammo = self.ammo - 1
                self.timer = 0.6
                set_number("agent_shots", get_number("agent_shots", 0) + 1)
            if self.ammo <= 0:
                self.state = FLEE
            elif distance > self.attack_range * 1.4:
                self.state = CHASE

        elif self.state == FLEE:
            player = self.player_node()
            if player == None or self.arrived():
                self.ammo = 6
                self.state = IDLE
                self.timer = 1.0
                return
            if self.agent.has_path() == False:
                px, py = player.get_position()
                ok, x, y = self.random_walkable()
                if ok and self.distance_between(x, y, px, py) > self.flee_distance:
                    self.go_to(x, y, self.chase_speed)

    def distance_between(self, ax, ay, bx, by):
        return math.sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay))
