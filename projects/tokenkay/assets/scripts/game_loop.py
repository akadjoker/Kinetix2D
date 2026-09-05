class GameLoop(ScriptComponent):
    scene_path = "scenes/game.k2dscene"
    restart_delay = 1.0
    fade_duration = 0.45

    def on_start(self):
        self.state = 0
        self.timer = 0.0
        set_flag("player_dead", False)
        fade_in(self.fade_duration)

    def on_update(self, dt):
        # Wait a moment so the death feedback remains visible before reset.
        if self.state == 0:
            if get_flag("player_dead", False):
                self.state = 1
                self.timer = self.restart_delay
            return

        if self.state == 1:
            self.timer = self.timer - dt
            if self.timer <= 0.0:
                fade_out(self.fade_duration)
                self.state = 2
            return

        # load_scene is deferred until the end of the current engine frame.
        if self.state == 2 and not is_fading():
            self.state = 3
            load_scene(self.scene_path)
