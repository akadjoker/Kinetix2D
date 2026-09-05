class DestructibleProp(ScriptComponent):
    hit_points = 30.0
    explosion_prefab = "assets/prefabs/explosion.k2dprefab"
    smoke_prefab = "assets/prefabs/smoke.k2dprefab"

    def on_start(self):
        self.destroyed = False
        self.sprites = self.node.get_components<Sprite>()

        # First sprite is intact; second sprite is the destroyed version.
        if len(self.sprites) > 0:
            self.sprites[0].set_active(True)
        if len(self.sprites) > 1:
            self.sprites[1].set_active(False)

    def take_damage(self, amount):
        if self.destroyed:
            return

        self.hit_points = self.hit_points - amount
        if self.hit_points <= 0.0:
            self.destroy()

    def destroy(self):
        if self.destroyed:
            return
        self.destroyed = True

        if len(self.sprites) > 0:
            self.sprites[0].set_active(False)
        if len(self.sprites) > 1:
            self.sprites[1].set_active(True)

        # A zero filter keeps the destroyed sprite in the scene but removes
        # every collision shape from contact with bullets and characters.
        colliders = self.node.get_components<Collider>()
        for collider in colliders:
            collider.set_filter(0, 0)

        self.node.set_tag("destroyed_prop")
        x, y = self.node.get_global_position()
        self.node.spawn(self.explosion_prefab, x, y)
        self.node.spawn(self.smoke_prefab, x, y)
