class Npc(ScriptComponent):
    speed = 130

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        set_flag("npc_ready", self.agent != None)
        if self.agent == None:
            return
        # Where to stand is the Formation2D's job, and walking there is the
        # agent's. This only says how fast and which way round.
        self.agent.set_max_speed(self.speed)
        self.agent.set_orient_to_path(True)
        self.agent.set_auto_move(True)
