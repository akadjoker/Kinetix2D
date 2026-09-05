import math


# One shared particle emitter for every bullet's impact, instead of each
# bullet carrying its own Particle system -- repositioned and burst() from
# wherever it's needed, never spawned/destroyed per hit.
IMPACT_PARTICLES_NAME = "ImpactParticles"
ENEMY_TAG = "enemy"
DESTRUCTIBLE_TAG = "destructible"
SMOKE_PREFAB = "assets/prefabs/smoke.k2dprefab"


class Bullet(ScriptComponent):
    lifetime = 2.0
    burst_count = 12
    death_delay = 0.05
    damage = 10.0

    def on_start(self):
        self.sprite = self.node.get_component<Sprite>()
        self.body = self.node.get_component<RigidBody>()
        # Resolved once here, like self.sprite/self.body above -- not
        # re-searched on every death. node.find() reaches the whole scene,
        # not just this node's own children (ImpactParticles is a sibling).
        self.impact = self.node.find(IMPACT_PARTICLES_NAME)
        self.dying = False
        self.death_timer = 0.0
        self.age = 0.0

    def on_update(self, dt):
        if self.dying:
            self.death_timer = self.death_timer - dt
            if self.death_timer <= 0.0:
                self.node.queue_destroy()
            return

        self.age = self.age + dt
        if self.age > self.lifetime:
            self.die()

    def on_collision(self, other, began):
        if not began or self.dying:
            return
        if other != None:
            tag = other.get_tag()
            if tag != ENEMY_TAG:
                self.spawn_smoke()
            if tag == ENEMY_TAG or tag == DESTRUCTIBLE_TAG:
                other.call("take_damage", self.damage)
        self.play_impact_sound()
        self.die()

    def play_impact_sound(self):
        index = int(math.random(0.0, 6.999))
        sound = int(get_number("sfx_impact_0", 0.0))
        if index == 1:
            sound = int(get_number("sfx_impact_1", 0.0))
        elif index == 2:
            sound = int(get_number("sfx_impact_2", 0.0))
        elif index == 3:
            sound = int(get_number("sfx_impact_3", 0.0))
        elif index == 4:
            sound = int(get_number("sfx_impact_4", 0.0))
        elif index == 5:
            sound = int(get_number("sfx_impact_5", 0.0))
        elif index == 6:
            sound = int(get_number("sfx_impact_6", 0.0))
        if sound == 0:
            return
        x, y = self.node.get_global_position()
        audio_play_at(sound, x, y, 0.38, math.random(0.94, 1.06), 70.0, 750.0)

    def spawn_smoke(self):
        x, y = self.node.get_global_position()
        self.node.spawn(SMOKE_PREFAB, x, y)

    def die(self):
        self.dying = True
        self.death_timer = self.death_delay
        if self.body != None:
            self.body.set_velocity(0, 0)
        if self.sprite != None:
            self.sprite.set_active(False)
        self.burst_impact()

    def burst_impact(self):
        if self.impact == None:
            return
        x, y = self.node.get_position()
        self.impact.set_position(x, y)
        particle = self.impact.get_component<Particle>()
        if particle != None:
            particle.burst(self.burst_count)
