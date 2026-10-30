import math


class Player(ScriptComponent):
    speed = 220

    def on_start(self):
        self.body = self.node.get_component<CharacterBody>()

    def on_update(self, dt):
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

        mx, my = mouse_world_position()
        x, y = self.node.get_position()
        heading = math.atan2(my - y, mx - x)
        self.node.set_rotation(math.degrees(heading))

        if forward == 0 and strafe == 0:
            if self.body != None:
                self.body.set_velocity(0, 0)
                self.node.move_and_slide()
            return

        length = math.sqrt(forward * forward + strafe * strafe)
        forward = forward / length
        strafe = strafe / length
        fx = math.cos(heading)
        fy = math.sin(heading)
        vx = (fx * forward - fy * strafe) * self.speed
        vy = (fy * forward + fx * strafe) * self.speed

        # The chain colliders around the water only stop the boat when the move
        # goes through the physics solver; translate() would walk over them.
        if self.body != None:
            self.body.set_velocity(vx, vy)
            self.node.move_and_slide()
        else:
            self.node.translate(vx * dt, vy * dt)
