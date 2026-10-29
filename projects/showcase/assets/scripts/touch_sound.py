class TouchSound(ScriptComponent):
    def on_start(self):
        self.beep = audio_load("sounds/beep.wav")
        self.pad = audio_load_music("sounds/pad.wav")
        audio_play_music(self.pad, True, 0.4)

    def on_update(self, dt):
        if mouse_pressed(0):
            left, top, right, bottom = world_view_rect()
            x, y = mouse_world_position()
            audio_play(self.beep, 1, 0.75 + (x - left) / (right - left))
