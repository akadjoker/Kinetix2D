class Hud(ScriptComponent):

    def on_start(self):
        set_number("score", 0)
        set_string("state", "playing")

    def on_update(self, dt):
        if get_string("state", "") == "gameover":
            self.node.set_visible(True)

    def on_event(self, name, value):
        if name == "shot":
            print("shots fired:", value)
        if name == "enemy_killed":
            set_number("score", get_number("score", 0) + value)
            print("score:", get_number("score", 0))
        if name == "player_died":
            set_string("state", "gameover")
