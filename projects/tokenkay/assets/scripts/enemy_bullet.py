PLAYER_TAG = "player"
ENEMY_TAG = "enemy"
SMOKE_PREFAB = "assets/prefabs/smoke.k2dprefab"


class EnemyBullet(ScriptComponent):
    lifetime = 2.0
    damage = 10.0

    def on_start(self):
        self.age = 0.0
        self.dying = False
        self.body = self.node.get_component<RigidBody>()

    def on_update(self, dt):
        if self.dying:
            return
        self.age = self.age + dt
        if self.age >= self.lifetime:
            self.die()

    def on_collision(self, other, began):
        if not began or self.dying:
            return

        if other == None:
            self.die()
            return

        tag = other.get_tag()
        # The sensor bullet can overlap its shooter or another Rojas without
        # pushing or hurting either of them.
        if tag == ENEMY_TAG:
            return

        if tag == PLAYER_TAG:
            other.call("take_damage", self.damage)
        else:
            self.spawn_smoke()
        self.play_collision_sound(tag)
        self.die()

    def play_collision_sound(self, tag):
        if tag == PLAYER_TAG:
            sound = int(get_number("sfx_player_hit", 0.0))
        else:
            sound = int(get_number("sfx_impact_0", 0.0))
        if sound == 0:
            return
        x, y = self.node.get_global_position()
        audio_play_at(sound, x, y, 0.45, 1.0, 70.0, 750.0)

    def spawn_smoke(self):
        x, y = self.node.get_global_position()
        self.node.spawn(SMOKE_PREFAB, x, y)

    def die(self):
        if self.dying:
            return
        self.dying = True
        if self.body != None:
            self.body.set_velocity(0.0, 0.0)
        self.node.queue_destroy()
