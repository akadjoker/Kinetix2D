import math

 
RUN_CLIPS = ["run_e", "run_se", "run_s", "run_se", "run_e", "run_ne", "run_n", "run_ne"]
SHOOT_CLIPS = ["shoot_e", "shoot_se", "shoot_s", "shoot_se", "shoot_e", "shoot_ne", "shoot_n", "shoot_ne"]
FLIPS = [False, False, False, True, True, True, False, False]

BULLET_PATH = "assets/prefabs/bullet.k2dprefab"
BULLET_SPEED = 600
BULLET_MUZZLE_OFFSET = 46.0
FIRE_INTERVAL = 0.15

 
BUCKET_HYSTERESIS = 6.0


class Prota(ScriptComponent):
    speed = 200

    def on_start(self):
        self.body = self.node.get_component<CharacterBody>()
        self.anim = self.node.get_component<Animation>()
        self.sprite = self.node.get_component<Sprite>()
        self.current_clip = ""
        self.last_bucket = 0
        self.fire_cooldown = 0.0
     

    def on_update(self, dt):
        mx, my = mouse_world_position()
 
        x, y = self.node.get_global_position()
        heading = math.atan2(my - y, mx - x)

        shooting = mouse_down(0)

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
        if shooting and self.fire_cooldown <= 0.0:
            self.fire_cooldown = FIRE_INTERVAL
            self.fire(x, y, heading)

        if not moving:
            if self.body != None:
                self.body.set_velocity(0, 0)
            self.set_pose(heading, shooting,False)
            return

        self.set_pose(heading, shooting, True)

 
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
        bullet = self.node.spawn(BULLET_PATH, spawn_x, spawn_y)
        if bullet == None:
            print("no bullet")
            return
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
            self.anim.play(name)
            self.current_clip = name
