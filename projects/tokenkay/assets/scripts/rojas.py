import math

RUN_CLIPS = ["run_e", "run_se", "run_s", "run_se", "run_e", "run_ne", "run_n", "run_ne"]
SHOOT_CLIPS = ["shoot_e", "shoot_se", "shoot_s", "shoot_se", "shoot_e", "shoot_ne", "shoot_n", "shoot_ne"]
FLIPS = [False, False, False, True, True, True, False, False]

PLAYER_TAG = "player"
ENEMY_BULLET_PATH = "assets/prefabs/enemy_bullet.k2dprefab"
ENEMY_BULLET_MUZZLE_OFFSET = 40.0
SIGHT_ORIGIN_OFFSET = 12.0
SHOOT_FRAME = 0

# State layout follows the original DIV tokenkai enemy process: idle/patrol,
# route roaming, chasing, shooting and finally fleeing to reload.
STATE_IDLE = 0
STATE_PICK_PATROL = 1
STATE_PAUSE_A = 2
STATE_TO_B = 3
STATE_PAUSE_B = 4
STATE_TO_A = 5
STATE_PICK_ROAM = 6
STATE_ROAM = 7
STATE_CHASE = 8
STATE_AIM = 9
STATE_SHOOT = 10
STATE_RECOVER = 11
STATE_PICK_FLEE = 12
STATE_FLEE = 13

# See prota.py's BUCKET_HYSTERESIS: the path-following heading sits right on
# a 45deg boundary just as often as a mouse aim does, and without this the
# clip flips (and resets to frame 0) almost every frame near one.
BUCKET_HYSTERESIS = 6.0


class Rojas(ScriptComponent):
    # scene.find() is case-sensitive; the player prefab's root is named
    # "Player" (capital), not "player".
    target_name = "Player"
    speed = 160
    patrol_speed = 90
    stop_distance = 4.0
    hp = 30.0
    death_delay = 0.6
    hit_blood_prefab = "assets/prefabs/blodEmitter.k2dprefab"
    attack_range = 200.0
    attack_release_range = 260.0
    proximity_range = 150.0
    sight_range = 600.0
    sight_half_angle = 120.0
    aim_pause = 0.30
    # Three frames at 15 FPS take 0.20s. Stop just before a looping clip can
    # wrap back to its muzzle-flash frame.
    shoot_duration = 0.19
    recover_pause = 0.50
    bullet_speed = 400.0
    ammo_capacity = 6
    flee_distance = 360.0
    alert_range = 180.0
    world_width = 1280.0
    world_height = 960.0

    def on_start(self):
        self.agent = self.node.get_component<NavigationAgent>()
        self.body = self.node.get_component<RigidBody>()
        self.anim = self.node.get_component<Animation>()
        self.sprite = self.node.get_component<Sprite>()
        self.blink = self.node.get_component<ActionSequence>()
        self.current_clip = ""
        self.last_bucket = int(math.random(0.0, 7.999))
        self.dead = False
        self.blood_spawned = False
        self.blink_started = False
        self.death_timer = 0.0
        self.fire_frame_active = False
        self.combat_state = STATE_IDLE
        self.state_timer = 0.0
        self.aim_heading = 0.0
        self.ammo = self.ammo_capacity
        self.point_a_x = 0.0
        self.point_a_y = 0.0
        self.point_b_x = 0.0
        self.point_b_y = 0.0
        set_flag("enemy_has_agent", self.agent != None)
        if self.agent == None:
            return
        self.agent.set_max_speed(self.speed)
        self.agent.set_auto_move(False)
        # Facing comes from the walk_*/run_* clip + flip below, not from
        # rotating the node -- directional pixel art doesn't rotate cleanly.
        self.agent.set_orient_to_path(False)
        self.agent.clear_follow_target()
        self.stop_pose()
        self.state_timer = math.random(0.25, 0.75)

    def on_update(self, dt):
        if self.dead:
            self.death_timer = self.death_timer - dt
            #if self.death_timer <= 0.0:
            #    self.node.queue_destroy()
            return

        if self.agent == None:
            return

        self.state_timer = self.state_timer - dt

        # Like observar() in the DIV game, idle and patrolling enemies only
        # engage when the player is close or inside their field of view.
        if self.combat_state <= STATE_ROAM and self.sees_player():
            self.begin_chase()
            # NavigationAgent resolves a newly assigned follow target during
            # its next update. Do not query it again in this same script tick.
            return

        if self.combat_state == STATE_IDLE:
            self.stop_agent()
            if self.state_timer <= 0.0:
                if math.random() < 0.5:
                    self.combat_state = STATE_PICK_PATROL
                else:
                    self.combat_state = STATE_PICK_ROAM

        elif self.combat_state == STATE_PICK_PATROL:
            self.pick_patrol()

        elif self.combat_state == STATE_PAUSE_A:
            self.stop_agent()
            if self.state_timer <= 0.0:
                self.go_to(self.point_b_x, self.point_b_y, self.patrol_speed)
                self.combat_state = STATE_TO_B

        elif self.combat_state == STATE_TO_B:
            self.update_move_pose()
            if self.arrived():
                self.begin_patrol_pause(STATE_PAUSE_B)

        elif self.combat_state == STATE_PAUSE_B:
            self.stop_agent()
            if self.state_timer <= 0.0:
                self.go_to(self.point_a_x, self.point_a_y, self.patrol_speed)
                self.combat_state = STATE_TO_A

        elif self.combat_state == STATE_TO_A:
            self.update_move_pose()
            if self.arrived():
                self.begin_patrol_pause(STATE_PAUSE_A)

        elif self.combat_state == STATE_PICK_ROAM:
            self.pick_roam()

        elif self.combat_state == STATE_ROAM:
            self.update_move_pose()
            if self.arrived():
                self.begin_idle()

        elif self.combat_state == STATE_CHASE:
            self.update_chase()

        elif self.combat_state == STATE_AIM:
            self.update_aim()

        elif self.combat_state == STATE_SHOOT:
            self.stop_motion()
            if self.state_timer <= 0.0:
                self.begin_recover()

        elif self.combat_state == STATE_RECOVER:
            self.stop_motion()
            if self.state_timer <= 0.0:
                if self.ammo <= 0:
                    self.begin_flee()
                else:
                    self.resume_combat()

        elif self.combat_state == STATE_PICK_FLEE:
            self.pick_flee()

        elif self.combat_state == STATE_FLEE:
            self.update_move_pose()
            if self.arrived():
                self.ammo = self.ammo_capacity
                self.begin_idle()

        set_number("enemy_state", self.combat_state)
        set_number("enemy_ammo", self.ammo)

    def player_node(self):
        return self.node.find(self.target_name)

    def random_walkable(self):
        tries = 0
        while tries < 24:
            tries = tries + 1
            x = math.random(0.0, self.world_width)
            y = math.random(0.0, self.world_height)
            if nav_point_free(x, y):
                return True, x, y
        return False, 0.0, 0.0

    def distance_between(self, ax, ay, bx, by):
        dx = bx - ax
        dy = by - ay
        return math.sqrt(dx * dx + dy * dy)

    def sees_player(self):
        player = self.player_node()
        if player == None:
            return False
        x, y = self.node.get_global_position()
        target_x, target_y = player.get_global_position()
        distance = self.distance_between(x, y, target_x, target_y)
        if distance <= self.proximity_range:
            return True
        if distance > self.sight_range:
            return False
        heading = math.degrees(math.atan2(target_y - y, target_x - x))
        facing = self.last_bucket * 45.0
        delta = heading - facing
        delta = delta - 360.0 * math.round(delta / 360.0)
        if math.abs(delta) > self.sight_half_angle:
            return False
        return self.has_clear_shot(x, y, target_x, target_y, distance)

    def go_to(self, x, y, speed):
        self.agent.set_max_speed(speed)
        self.agent.clear_follow_target()
        self.agent.set_auto_move(True)
        self.agent.set_target(x, y)

    def stop_agent(self):
        self.agent.set_auto_move(False)
        self.stop_motion()
        self.stop_pose()

    def arrived(self):
        return self.agent.is_finished() or not self.agent.has_path()

    def begin_idle(self):
        self.think("think_calm")
        self.combat_state = STATE_IDLE
        self.state_timer = math.random(0.25, 1.0)
        self.fire_frame_active = False
        self.agent.clear_follow_target()
        self.agent.clear_path()
        self.stop_agent()

    def begin_patrol_pause(self, state):
        self.combat_state = state
        self.state_timer = math.random(0.25, 1.25)
        self.agent.clear_path()
        self.stop_agent()

    def pick_patrol(self):
        start_x, start_y = self.node.get_global_position()
        self.point_a_x = start_x
        self.point_a_y = start_y
        ok, x, y = self.random_walkable()
        if not ok or self.distance_between(self.point_a_x, self.point_a_y, x, y) < 80.0:
            self.begin_idle()
            return
        self.point_b_x = x
        self.point_b_y = y
        self.begin_patrol_pause(STATE_PAUSE_A)

    def pick_roam(self):
        ok, x, y = self.random_walkable()
        if not ok:
            self.begin_idle()
            return
        self.go_to(x, y, self.patrol_speed)
        self.combat_state = STATE_ROAM

    def update_move_pose(self):
        set_number("enemy_waypoints", self.agent.path_count())
        set_flag("enemy_has_path", self.agent.has_path())
        if self.arrived():
            self.stop_pose()
            return
        x, y = self.node.get_global_position()
        next_x, next_y = self.agent.next_position()
        heading = math.atan2(next_y - y, next_x - x)
        bucket = self.bucket_for_heading(heading)
        self.last_bucket = bucket
        self.play_clip(RUN_CLIPS[bucket], FLIPS[bucket])

    # thought_bubble.py is a second ScriptComponent on this same node, so
    # node.call() reaches it. A child node would be found by name through
    # scene()->find(), which every other enemy would resolve to as well.
    # node.call() dispatches to every script on the node, and invoking a
    # method a script does not define is a VM runtime error, so these stubs
    # absorb the bubble's messages on this side.
    def think(self, message):
        self.node.call(message)

    def think_alert(self, value):
        return

    def think_shoot(self, value):
        return

    def think_flee(self, value):
        return

    def think_calm(self, value):
        return

    def think_silence(self, value):
        return

    def begin_chase(self):
        # Only shout when coming out of an unaware state: CHASE is re-entered
        # between shots, and every re-entry would otherwise be a new shout.
        if self.combat_state < STATE_CHASE:
            self.think("think_alert")
        self.combat_state = STATE_CHASE
        self.state_timer = 0.0
        self.fire_frame_active = False
        self.agent.set_max_speed(self.speed)
        self.agent.set_auto_move(True)
        self.agent.set_follow_target(self.target_name)

    def update_chase(self):
        target_found, target_x, target_y = self.agent.get_follow_position()
        if not target_found:
            # set_follow_target() may have been called after the agent's update
            # for this frame. The scene lookup is immediately available and
            # prevents CHASE from falling back to IDLE for one frame.
            player = self.player_node()
            if player == None:
                self.begin_idle()
                return
            target_x, target_y = player.get_global_position()
        x, y = self.node.get_global_position()
        distance = self.distance_between(x, y, target_x, target_y)
        clear_shot = self.has_clear_shot(x, y, target_x, target_y, distance)
        if distance <= self.attack_range and clear_shot:
            self.begin_aim(math.atan2(target_y - y, target_x - x))
            return
        self.update_move_pose()
        # Every other moving state answers arrived(); without this one the
        # agent losing its path (player off the navmesh, a failed repath)
        # leaves CHASE running forever with the walk clip stopped mid-stride.
        if self.arrived():
            self.begin_idle()

    def update_aim(self):
        player = self.player_node()
        if player == None:
            self.begin_idle()
            return
        x, y = self.node.get_global_position()
        target_x, target_y = player.get_global_position()
        distance = self.distance_between(x, y, target_x, target_y)
        if distance > self.attack_release_range or not self.has_clear_shot(x, y, target_x, target_y, distance):
            self.begin_chase()
            return
        self.stop_motion()
        self.aim_heading = math.atan2(target_y - y, target_x - x)
        self.face_heading(self.aim_heading)
        if self.state_timer <= 0.0:
            self.begin_shot(x, y)

    def resume_combat(self):
        player = self.player_node()
        if player == None:
            self.begin_idle()
            return
        x, y = self.node.get_global_position()
        target_x, target_y = player.get_global_position()
        distance = self.distance_between(x, y, target_x, target_y)
        if distance <= self.attack_release_range and self.has_clear_shot(x, y, target_x, target_y, distance):
            self.begin_aim(math.atan2(target_y - y, target_x - x))
        else:
            self.begin_chase()

    def begin_flee(self):
        self.think("think_flee")
        self.combat_state = STATE_PICK_FLEE
        self.agent.clear_follow_target()
        self.agent.clear_path()
        x, y = self.node.get_global_position()
        set_number("rojas_alert_x", x)
        set_number("rojas_alert_y", y)
        emit("rojas_alert", 1)

    def pick_flee(self):
        player = self.player_node()
        if player == None:
            self.ammo = self.ammo_capacity
            self.begin_idle()
            return
        player_x, player_y = player.get_global_position()
        tries = 0
        while tries < 24:
            tries = tries + 1
            x = math.random(0.0, self.world_width)
            y = math.random(0.0, self.world_height)
            if nav_point_free(x, y) and self.distance_between(x, y, player_x, player_y) >= self.flee_distance:
                self.go_to(x, y, self.speed)
                self.combat_state = STATE_FLEE
                return
        self.ammo = self.ammo_capacity
        self.begin_chase()

    def stop_motion(self):
        if self.body != None:
            self.body.set_velocity(0.0, 0.0)

    def face_heading(self, heading):
        bucket = self.bucket_for_heading(heading)
        self.last_bucket = bucket
        if self.sprite != None:
            self.sprite.set_flip(FLIPS[bucket], False)

    def begin_aim(self, heading):
        self.combat_state = STATE_AIM
        self.state_timer = self.aim_pause
        self.aim_heading = heading
        self.fire_frame_active = False
        if self.agent != None:
            self.agent.set_auto_move(False)
        self.stop_motion()
        self.face_heading(heading)
        self.stop_pose()

    def begin_shot(self, x, y):
        self.think("think_shoot")
        self.combat_state = STATE_SHOOT
        self.state_timer = self.shoot_duration
        self.fire_frame_active = False
        bucket = self.bucket_for_heading(self.aim_heading)
        self.last_bucket = bucket
        self.play_clip(SHOOT_CLIPS[bucket], FLIPS[bucket])
        self.fire_on_animation_frame(x, y, self.aim_heading)

    def begin_recover(self):
        self.combat_state = STATE_RECOVER
        self.state_timer = self.recover_pause
        # Never freeze on the final shooting frame: that frame can contain
        # the muzzle flash. Switch to frame 0 of the matching run direction
        # and stop there while the enemy waits before its next decision.
        self.stop_pose()

    def fire_on_animation_frame(self, x, y, heading):
        if self.anim == None:
            return
        on_fire_frame = self.anim.get_frame() == SHOOT_FRAME
        if not on_fire_frame:
            self.fire_frame_active = False
            return
        if self.fire_frame_active:
            return
        self.fire_frame_active = True
        self.fire(x, y, heading)

    def has_clear_shot(self, x, y, target_x, target_y, distance):
        if distance <= SIGHT_ORIGIN_OFFSET:
            return True
        dx = target_x - x
        dy = target_y - y
        fx = dx / distance
        fy = dy / distance
        origin_x = x + fx * SIGHT_ORIGIN_OFFSET
        origin_y = y + fy * SIGHT_ORIGIN_OFFSET
        cast_distance = distance - SIGHT_ORIGIN_OFFSET
        hit, hit_x, hit_y = raycast(origin_x, origin_y, dx, dy, cast_distance)
        return hit == None or hit.get_tag() == PLAYER_TAG

    def fire(self, x, y, heading):
        fx = math.cos(heading)
        fy = math.sin(heading)
        spawn_x = x + fx * ENEMY_BULLET_MUZZLE_OFFSET
        spawn_y = y + fy * ENEMY_BULLET_MUZZLE_OFFSET
        if self.anim != None:
            point_found, point_x, point_y = self.anim.get_real_point(0)
            if point_found:
                spawn_x = point_x
                spawn_y = point_y
        bullet = self.node.spawn(
            ENEMY_BULLET_PATH,
            spawn_x,
            spawn_y
        )
        if bullet == None:
            print("Rojas: could not spawn enemy bullet")
            return
        shot_sound = int(get_number("sfx_pistol", 0.0))
        if shot_sound != 0:
            audio_play_at(shot_sound, spawn_x, spawn_y, 0.42, math.random(0.92, 0.98), 90.0, 900.0)
        self.ammo = self.ammo - 1
        bullet.set_rotation(math.degrees(heading))
        bullet_body = bullet.get_component<RigidBody>()
        if bullet_body != None:
            bullet_body.set_velocity(fx * self.bullet_speed, fy * self.bullet_speed)


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

    # Standing still is frame 0 of the direction the enemy faces, never
    # anim.stop() on its own: stop() freezes whatever frame the walk cycle
    # happened to be on, which reads as a body halted mid-stride. play()
    # rewinds to frame 0 first, so stopping right after lands on the clean
    # standing pose -- the same split the DIV original makes between its
    # anima[] walk cycle and the single prodis[0] standing frame.
    def stop_pose(self):
        bucket = self.last_bucket
        if self.sprite != None:
            self.sprite.set_flip(FLIPS[bucket], False)
        if self.anim == None:
            return
        name = RUN_CLIPS[bucket]
        # play() rewinds to frame 0 even when the clip is already current,
        # which is exactly what a re-facing standing enemy needs.
        self.anim.play(name)
        self.anim.stop()
        self.current_clip = name

    def play_clip(self, name, flip):
        if self.sprite != None:
            self.sprite.set_flip(flip, False)
        if self.anim == None:
            return
        # Not just "name changed": on_update's idle branch stops the clip
        # without renaming it, so resuming the same direction after a stop
        # needs a re-play too, or it stays frozen forever.
        if self.current_clip != name or not self.anim.is_playing():
            if self.current_clip != name:
                self.fire_frame_active = False
            self.anim.play(name)
            self.current_clip = name

    # Called by bullet.py's on_collision through node.call() -- the bullet
    # only knows this node has tag "enemy", not that it runs Rojas.
    def take_damage(self, amount):
        if self.dead:
            return
        self.spawn_hit_blood()
        self.hp = self.hp - amount
        if self.hp <= 0.0:
            self.die()
        elif self.combat_state < STATE_CHASE:
            # Being hit alerts an unaware enemy, as in the original.
            self.begin_chase()

    def spawn_hit_blood(self):
        x, y = self.node.get_position()
        self.node.spawn(self.hit_blood_prefab, x, y - 8.0)

    def die(self):
        self.think("think_silence")
        self.dead = True
        self.combat_state = STATE_RECOVER
        print("morreu")
        death_sound = int(get_number("sfx_death", 0.0))
        if death_sound != 0:
            x, y = self.node.get_global_position()
            audio_play_at(death_sound, x, y, 0.72, math.random(0.70, 0.84), 80.0, 900.0)
        self.death_timer = self.death_delay
        if self.agent != None:
            self.agent.set_auto_move(False)
            self.agent.clear_follow_target()
            self.agent.clear_path()
        if self.body != None:
            self.body.set_velocity(0.0, 0.0)
        self.play_clip("death", False)

    def on_animation_event(self, name):
        # "die" is accepted for scene instances saved before this event was
        # renamed to "blood" in the prefab.
        if not self.dead or self.blood_spawned or (name != "blood" and name != "die"):
            return
        self.blood_spawned = True
        x, y = self.node.get_position()
        blood = self.node.spawn("assets/prefabs/blood.k2dprefab", x, y)
        if blood != None:
            blood.reparent(self.node)
            blood.set_position(40, 10)

    # Called by blood.py after the blood sequence emits "blood_finished".
    # node.call() supplies a numeric value, even when none was passed.
    def finish_death(self, value):
        if self.blink_started:
            return
        self.blink_started = True
        if self.blink != None:
            self.blink.play(True)
        else:
            self.node.queue_destroy()

    def on_event(self, name, value):
        if name == "rojas_cleanup":
            self.node.queue_destroy()
        elif name == "rojas_alert" and not self.dead and self.combat_state < STATE_CHASE:
            alert_x = get_number("rojas_alert_x", 0.0)
            alert_y = get_number("rojas_alert_y", 0.0)
            x, y = self.node.get_global_position()
            if self.distance_between(x, y, alert_x, alert_y) <= self.alert_range:
                self.begin_chase()
