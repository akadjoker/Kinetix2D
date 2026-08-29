import math


class Npc(ScriptComponent):
    target_name = "player"
    speed = 130
    stop_distance = 60
    resume_distance = 90

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
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
        self.agent.set_follow_target(self.target_name)
        self.agent.set_auto_move(True)
        self.moving = True

    def on_update(self, dt):
        if self.agent == None:
            return
        if self.player == None:
            self.player = self.node.find(self.target_name)
            if self.player == None:
                return

        px, py = self.player.get_position()
        x, y = self.node.get_position()
        dx = px - x
        dy = py - y
        distance = math.sqrt(dx * dx + dy * dy)

        # Two distances, not one: stopping and starting on the same threshold
        # makes the npc flicker between moving and standing while it sits right
        # on the line. The follow target is never cleared, so the path stays
        # current even while stopped and the chase resumes immediately.
        if self.moving:
            if distance <= self.stop_distance:
                self.moving = False
                self.agent.set_auto_move(False)
        else:
            if distance >= self.resume_distance:
                self.moving = True
                self.agent.set_auto_move(True)

        set_number("npc_distance", distance)
        set_flag("npc_moving", self.moving)
