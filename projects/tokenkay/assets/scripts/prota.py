import math

 
RUN_CLIPS = ["run_e", "run_se", "run_s", "run_se", "run_e", "run_ne", "run_n", "run_ne"]
SHOOT_CLIPS = ["shoot_e", "shoot_se", "shoot_s", "shoot_se", "shoot_e", "shoot_ne", "shoot_n", "shoot_ne"]
FLIPS = [False, False, False, True, True, True, False, False]

BULLET_PATH = "assets/prefabs/bullet.k2dprefab"
BULLET_SPEED = 600
BULLET_MUZZLE_OFFSET = 46.0
FIRE_INTERVAL = 0.15
SHOOT_FRAME = 1

 
BUCKET_HYSTERESIS = 6.0


class Prota(ScriptComponent):
    speed = 200
    hp = 50.0
    hit_blood_prefab = "assets/prefabs/blodEmitter.k2dprefab"

    def on_start(self):
        self.body = self.node.get_component<CharacterBody>()
        self.anim = self.node.get_component<Animation>()
        self.sprite = self.node.get_component<Sprite>()
        self.current_clip = ""
        self.last_bucket = 0
        self.fire_cooldown = 0.0
        self.fire_frame_active = False
        self.trigger_armed = False
        self.dead = False
        set_number("player_max_hp", self.hp)
        set_number("player_hp", self.hp)
        set_flag("player_dead", False)
     

    def on_update(self, dt):
        if self.dead:
            return

        mx, my = mouse_world_position()
 
        x, y = self.node.get_global_position()
        heading = math.atan2(my - y, mx - x)

        # Do not inherit the mouse button used to press Play in the editor.
        # Automatic fire only starts from a fresh press inside the Game view.
        trigger_down = mouse_down(0)
        if mouse_pressed(0):
            self.trigger_armed = True
        if not trigger_down:
            self.trigger_armed = False
        shooting = trigger_down and self.trigger_armed

        forward = 0
        strafe = 0
        if key_down(KEY_W):
            forward = forward + 1
        if key_down(KEY_S):
            forward = forward - 1
        if key_down(KEY_D):
            strafe = strafe + 1
        if key_down(KEY_A):
            strafe = strafe - 1

        moving = forward != 0 or strafe != 0
        bucket = self.bucket_for_heading(heading)

        self.fire_cooldown = self.fire_cooldown - dt
        self.set_pose(heading, shooting, moving)
        if shooting:
            self.fire_on_animation_frame(x, y, heading)
        else:
            self.fire_frame_active = False

        if not moving:
            if self.body != None:
                self.body.set_velocity(0, 0)
            return

 
        length = math.sqrt(forward * forward + strafe * strafe)
        forward = forward / length
        strafe = strafe / length

        fx = math.cos(heading)
        fy = math.sin(heading)
        vx = (fx * forward - fy * strafe) * self.speed
        vy = (fy * forward + fx * strafe) * self.speed

 
        if self.body != None:
            self.body.set_velocity(vx, vy)
            self.node.move_and_slide()
        else:
            self.node.translate(vx * dt, vy * dt)

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
        if self.fire_cooldown > 0.0:
            return
        self.fire_cooldown = FIRE_INTERVAL
        self.fire(x, y, heading)

    def set_pose(self, heading, shooting, moving):
 
        if moving or shooting:
            bucket = self.bucket_for_heading(heading)
            self.last_bucket = bucket
            clip = SHOOT_CLIPS[bucket] if shooting else RUN_CLIPS[bucket]
            self.play_clip(clip, FLIPS[bucket])
            return

        self.play_clip(RUN_CLIPS[self.last_bucket], FLIPS[self.last_bucket])
        if self.anim != None:
            self.anim.stop()

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

    def fire(self, x, y, heading):
        fx = math.cos(heading)
        fy = math.sin(heading)
        spawn_x = x + fx * BULLET_MUZZLE_OFFSET
        spawn_y = y + fy * BULLET_MUZZLE_OFFSET
        if self.anim != None:
            point_found, point_x, point_y = self.anim.get_real_point(0)
            if point_found:
                spawn_x = point_x
                spawn_y = point_y
        bullet = self.node.spawn(BULLET_PATH, spawn_x, spawn_y)
        if bullet == None:
            print("no bullet")
            return
        shot_sound = int(get_number("sfx_pistol", 0.0))
        if shot_sound != 0:
            audio_play_at(shot_sound, spawn_x, spawn_y, 0.58, math.random(0.97, 1.03), 80.0, 850.0)
        bullet.set_rotation(math.degrees(heading))
        body = bullet.get_component<RigidBody>()
        if body != None:
            body.set_velocity(fx * BULLET_SPEED, fy * BULLET_SPEED)

    def play_clip(self, name, flip):
        if self.sprite != None:
            self.sprite.set_flip(flip, False)
        if self.anim == None:
            print("no animation")
            return
        if self.current_clip != name or not self.anim.is_playing():
            if self.current_clip != name:
                self.fire_frame_active = False
            self.anim.play(name)
            self.current_clip = name

    def on_animation_event(self, name):
        print("player animation event: " + name)

    # Called by enemy_bullet.py when a Rojas bullet reaches the Player.
    def take_damage(self, amount):
        if self.dead:
            return
        self.spawn_hit_blood()
        self.hp = self.hp - amount
        set_number("player_hp", self.hp)
        if self.hp <= 0.0:
            self.die()

    def spawn_hit_blood(self):
        x, y = self.node.get_global_position()
        self.node.spawn(self.hit_blood_prefab, x, y - 8.0)

    def die(self):
        if self.dead:
            return
        self.dead = True
        set_flag("player_dead", True)
        death_sound = int(get_number("sfx_death", 0.0))
        if death_sound != 0:
            x, y = self.node.get_global_position()
            audio_play_at(death_sound, x, y, 0.95, 0.62, 80.0, 950.0)
        if self.body != None:
            self.body.set_velocity(0.0, 0.0)
        self.node.queue_destroy()
