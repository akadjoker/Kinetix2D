import math


class Npc(ScriptComponent):
    target_name = "player"
    speed = 130
    back_off_distance = 40
    stop_distance = 60
    resume_distance = 90
    # Where around the player this one wants to stand, in degrees. Give each
    # npc a different angle and they surround him instead of queueing up behind
    # him down the same corridor.
    slot_angle = 0
    slot_repath = 0.4

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        print(self.agent)
        # Resolved once. Searching the tree for the player every frame is a
        # walk of the whole scene for an answer that never changes.
        self.player = self.node.find(self.target_name)
        self.moving = False
        set_flag("npc_ready", self.agent != None and self.player != None)
        if self.agent == None:
            return
        self.agent.set_max_speed(self.speed)
        self.agent.set_orient_to_path(True)
        # The follow target stays set for good: the C++ side already repaths on
        # its own interval once the player has moved far enough, so asking for
        # a new path from here would only repeat work it has already done.
        self.agent.set_auto_move(True)
        self.moving = True
        self.backing = False
        self.slot_timer = 0.0
        radians = math.radians(self.slot_angle)
        self.slot_x = math.cos(radians)
        self.slot_y = math.sin(radians)

    def on_update(self, dt):
        if self.agent == None:
            return
        if self.player == None:
            self.player = self.node.find(self.target_name)
            if self.player == None:
                print("no player")
                return

        px, py = self.player.get_position()
        x, y = self.node.get_position()
        dx = px - x
        dy = py - y
        distance = math.sqrt(dx * dx + dy * dy)

        if distance < self.back_off_distance:
            self.back_away(x, y, dx, dy, distance)
            return

        self.backing = False

        # Two distances, not one: stopping and starting on the same threshold
        # makes the npc flicker between moving and standing while it sits right
        # on the line.
        if self.moving:
            if distance <= self.stop_distance:
                self.moving = False
                self.agent.set_auto_move(False)
        else:
            if distance >= self.resume_distance:
                self.moving = True
                self.agent.set_auto_move(True)

        if not self.moving:
            return

        # Re-aim at this npc's own place around the player on an interval. Doing
        # it every frame would repath every frame, and the C++ follow target
        # cannot be used here because it always aims at the player himself.
        self.slot_timer = self.slot_timer - dt
        if self.slot_timer > 0:
            return
        self.slot_timer = self.slot_repath

        tx = px + self.slot_x * self.stop_distance
        ty = py + self.slot_y * self.stop_distance
        if not nav_point_free(tx, ty):
            tx = px
            ty = py
        self.agent.set_target(tx, ty)

    def back_away(self, x, y, dx, dy, distance):
        if distance < 0.001:
            return
        # Let the current retreat play out instead of asking for a new path
        # every frame, but take a fresh one the moment it runs out: a player
        # who keeps walking in has to be backed away from more than once.
        if self.backing and self.agent.has_path():
            return

        # Retreat along the line away from the player, far enough to land back
        # in the band it wants to hold. A follow target would drag it straight
        # back in, so it has to be dropped while backing off.
        step = self.stop_distance - distance + 10
        tx = x - dx / distance * step
        ty = y - dy / distance * step
        if not nav_point_free(tx, ty):
            return

        self.backing = True
        self.moving = True
        self.agent.clear_follow_target()
        self.agent.set_auto_move(True)
        self.agent.set_target(tx, ty)
