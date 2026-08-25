class Spawner(ScriptComponent):
    def __init__(self):
        self.cooldown = 0.4
        self._timer = 0.0

    def on_start(self):
        set_number("spawned", 0)

    def on_update(self, dt):
        self._timer = self._timer - dt
        if self._timer <= 0 and key_down(KEY_SPACE):
            self._timer = self.cooldown
            bullet = self.node.spawn("assets/prefabs/bullet.k2dprefab",
                                     self.node.get_x(), self.node.get_y())
            if bullet != None:
                set_number("spawned", get_number("spawned", 0) + 1)
                emit("shot", get_number("spawned", 0))

    def on_event(self, name, value):
        if name == "player_died":
            set_flag("can_shoot", False)
