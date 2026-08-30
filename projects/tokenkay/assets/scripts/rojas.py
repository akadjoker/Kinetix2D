import math

# Same 8-way scheme as prota.py, but only walk/run poses exist for the rojas
# enemy prefab so far (no shoot wiring here yet -- that needs a combat system
# this script doesn't have, beyond taking damage from the player's bullets).
RUN_CLIPS = ["run_e", "run_se", "run_s", "run_se", "run_e", "run_ne", "run_n", "run_ne"]
FLIPS = [False, False, False, True, True, True, False, False]

# See prota.py's BUCKET_HYSTERESIS: the path-following heading sits right on
# a 45deg boundary just as often as a mouse aim does, and without this the
# clip flips (and resets to frame 0) almost every frame near one.
BUCKET_HYSTERESIS = 6.0


class Rojas(ScriptComponent):
    # scene.find() is case-sensitive; the player prefab's root is named
    # "Player" (capital), not "player".
    target_name = "Player"
    speed = 10
    stop_distance = 4.0
    hp = 30.0
    death_delay = 0.6

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        self.anim = self.node.get_component<Animation>()
        self.sprite = self.node.get_component<Sprite>()
        self.current_clip = ""
        self.last_bucket = 0
        self.dead = False
        self.death_timer = 0.0
        set_flag("enemy_has_agent", self.agent != None)
        if self.agent == None:
            return
        self.agent.set_max_speed(self.speed)
        self.agent.set_auto_move(True)
        # Facing comes from the walk_*/run_* clip + flip below, not from
        # rotating the node -- directional pixel art doesn't rotate cleanly.
        self.agent.set_orient_to_path(False)
        # Following by name leaves the pathfinding on the C++ side, which
        # repaths on its own interval instead of once per frame.
        self.agent.set_follow_target(self.target_name)

    def on_update(self, dt):
        if self.dead:
            self.death_timer = self.death_timer - dt
            if self.death_timer <= 0.0:
                self.node.queue_destroy()
            return

        if self.agent == None:
            return
        set_number("enemy_waypoints", self.agent.path_count())
        set_flag("enemy_has_path", self.agent.has_path())

        heading = 0.0
        moving = False
        if self.agent.has_path() and not self.agent.is_finished():
            x, y = self.node.get_position()
            nx, ny = self.agent.next_position()
            dx = nx - x
            dy = ny - y
            if math.sqrt(dx * dx + dy * dy) > self.stop_distance:
                moving = True
                heading = math.atan2(dy, dx)

        if not moving:
            # Freeze on whatever frame/direction was last playing instead of
            # snapping to a single fixed idle pose (facing right).
            if self.anim != None:
                self.anim.stop()
            return

        bucket = self.bucket_for_heading(heading)
        self.last_bucket = bucket
        self.play_clip(RUN_CLIPS[bucket], FLIPS[bucket])

    def bucket_for_heading(self, heading):
        degrees = math.degrees(heading)
        nearest = int(math.round(degrees / 45.0)) % 8
        if nearest == self.last_bucket:
            return nearest
        delta = degrees - self.last_bucket * 45.0
        delta = delta - 360.0 * math.round(delta / 360.0)
        if math.abs(delta) < 22.5 + BUCKET_HYSTERESIS:
            return self.last_bucket
        return nearest

    def play_clip(self, name, flip):
        if self.sprite != None:
            self.sprite.set_flip(flip, False)
        if self.anim == None:
            return
        # Not just "name changed": on_update's idle branch stops the clip
        # without renaming it, so resuming the same direction after a stop
        # needs a re-play too, or it stays frozen forever.
        if self.current_clip != name or not self.anim.is_playing():
            self.anim.play(name)
            self.current_clip = name

    # Called by bullet.py's on_collision through node.call() -- the bullet
    # only knows this node has tag "enemy", not that it runs Rojas.
    def take_damage(self, amount):
        if self.dead:
            return
        self.hp = self.hp - amount
        if self.hp <= 0.0:
            self.die()

    def die(self):
        self.dead = True
        self.death_timer = self.death_delay
        if self.agent != None:
            self.agent.set_auto_move(False)
            self.agent.clear_follow_target()
            self.agent.clear_path()
        self.play_clip("death", False)
