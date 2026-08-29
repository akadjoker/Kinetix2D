class ClickToMove(ScriptComponent):
    speed = 180

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        if self.agent == None:
            return
        self.agent.set_max_speed(self.speed)
        self.agent.set_auto_move(True)
        self.agent.set_orient_to_path(True)

    def on_update(self, dt):
        if self.agent == None:
            return
        if mouse_pressed(0):
            mx, my = mouse_world_position()
            # Refuse a click outside the walkable mesh instead of asking for a
            # path that cannot exist: set_target would clear the current path
            # and leave the agent stopped for no reason.
            if nav_point_free(mx, my):
                self.agent.set_target(mx, my)
                set_number("click_x", mx)
                set_number("click_y", my)
            else:
                set_flag("click_rejected", True)
