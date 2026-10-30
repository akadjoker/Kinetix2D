import math


class Player(ScriptComponent):
    speed = 200

    def on_start(self):
        self.body = self.node.get_component<CharacterBody>()
        set_flag("player_has_body", self.body != None)

    def on_update(self, dt):
        mx, my = mouse_world_position()
        x, y = self.node.get_position()

        heading = math.atan2(my - y, mx - x)
        self.node.set_rotation(math.degrees(heading))

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

        if forward == 0 and strafe == 0:
            if self.body != None:
                self.body.set_velocity(0, 0)
            return

        # Normalised so a diagonal is not faster than a straight line.
        length = math.sqrt(forward * forward + strafe * strafe)
        forward = forward / length
        strafe = strafe / length

        fx = math.cos(heading)
        fy = math.sin(heading)
        vx = (fx * forward - fy * strafe) * self.speed
        vy = (fy * forward + fx * strafe) * self.speed

        # move_and_slide is what makes the buildings stop him; translate would
        # walk straight through the chain colliders.
        if self.body != None:
            self.body.set_velocity(vx, vy)
            self.node.move_and_slide()
        else:
            self.node.translate(vx * dt, vy * dt)
