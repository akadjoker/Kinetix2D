class Enemy(ScriptComponent):
    target_name = "player"
    speed = 130

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        set_flag("enemy_has_agent", self.agent != None)
        if self.agent == None:
            return
        self.agent.set_max_speed(self.speed)
        self.agent.set_auto_move(True)
        self.agent.set_orient_to_path(True)
        # Following by name leaves the pathfinding on the C++ side, which
        # repaths on its own interval instead of once per frame.
        self.agent.set_follow_target(self.target_name)

    def on_update(self, dt):
        if self.agent == None:
            return
        set_number("enemy_waypoints", self.agent.path_count())
        set_flag("enemy_has_path", self.agent.has_path())
