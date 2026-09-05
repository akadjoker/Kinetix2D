class WaveDirector(ScriptComponent):
    enemy_prefab = "assets/prefabs/enemy_rojas.k2dprefab"
    point_1 = "point1"
    point_2 = "point2"
    first_wave_size = 4
    max_wave_size = 8
    first_spawn_delay = 0.75
    spawn_interval = 1.25
    between_waves = 2.5

    def on_start(self):
        self.points = []
        self.enemies = []
        self.wave = 1
        self.next_point = 0
        self.spawn_timer = self.first_spawn_delay
        self.waiting_for_wave = False

        point = self.node.find(self.point_1)
        if point != None:
            self.points.append(point)
        point = self.node.find(self.point_2)
        if point != None:
            self.points.append(point)

        # Include enemies already placed by hand in the scene.
        root = self.node.get_root()
        i = 0
        while root != None and i < root.child_count():
            child = root.get_child(i)
            if child != None and child.get_tag() == "enemy":
                self.enemies.append(child)
            i = i + 1

        self.to_spawn = self.wave_size() - len(self.enemies)
        if self.to_spawn < 0:
            self.to_spawn = 0
        self.publish_state()

        if len(self.points) == 0:
            print("WaveDirector: point1/point2 not found")

    def on_update(self, dt):
        if get_flag("player_dead", False):
            return

        self.remove_destroyed_enemies()

        if self.to_spawn > 0:
            self.spawn_timer = self.spawn_timer - dt
            if self.spawn_timer <= 0.0:
                if self.spawn_rojas():
                    self.to_spawn = self.to_spawn - 1
                self.spawn_timer = self.spawn_interval
        elif len(self.enemies) == 0:
            if not self.waiting_for_wave:
                self.waiting_for_wave = True
                self.spawn_timer = self.between_waves
            else:
                self.spawn_timer = self.spawn_timer - dt
                if self.spawn_timer <= 0.0:
                    self.wave = self.wave + 1
                    self.to_spawn = self.wave_size()
                    self.waiting_for_wave = False
                    self.spawn_timer = 0.1

        self.publish_state()

    def wave_size(self):
        size = self.first_wave_size + self.wave - 1
        if size > self.max_wave_size:
            size = self.max_wave_size
        return size

    def spawn_rojas(self):
        count = len(self.points)
        if count == 0:
            return False

        point = self.points[self.next_point % count]
        self.next_point = self.next_point + 1
        x, y = point.get_global_position()
        enemy = self.node.spawn(self.enemy_prefab, x, y)
        if enemy == None:
            print("WaveDirector: could not spawn Rojas")
            return False

        self.enemies.append(enemy)
        return True

    def remove_destroyed_enemies(self):
        active = []
        i = 0
        while i < len(self.enemies):
            enemy = self.enemies[i]
            if enemy != None and enemy.is_active_in_hierarchy():
                active.append(enemy)
            i = i + 1
        self.enemies = active

    def publish_state(self):
        set_number("wave", self.wave)
        set_number("enemies_alive", len(self.enemies))
        set_number("enemies_pending", self.to_spawn)
