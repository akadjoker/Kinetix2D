import math


ENEMY_TAG = "enemy"


class Explosion(ScriptComponent):
    camera_name = "camera"
    blast_radius = 110.0
    blast_damage = 60.0
    alert_radius = 420.0
    shake_max_distance = 650.0
    shake_trauma = 0.85
    shake_amplitude_x = 12.0
    shake_amplitude_y = 8.0
    shake_frequency = 26.0
    shake_decay = 2.8
    zoom_punch = 0.035
    zoom_duration = 0.16

    def on_start(self):
        sound = int(get_number("sfx_explosion", 0.0))
        if sound != 0:
            x, y = self.node.get_global_position()
            audio_play_at(sound, x, y, 0.8, 1.0, 100.0, 1100.0)
        self.trigger_camera_feedback()
        self.damage_nearby()
        self.alert_nearby()
        self.anim = self.node.get_component<Animation>()
        if self.anim != None:
            self.anim.play("default")
        else:
            self.node.queue_destroy()

    # Anything caught in the blast takes damage; falls off with distance so a
    # rojas at the rim is hurt, not deleted.
    def damage_nearby(self):
        x, y = self.node.get_global_position()
        root = self.node.get_root()
        if root == None:
            return
        i = 0
        while i < root.child_count():
            child = root.get_child(i)
            i = i + 1
            if child == None or child.get_tag() != ENEMY_TAG:
                continue
            ex, ey = child.get_global_position()
            dx = ex - x
            dy = ey - y
            distance = math.sqrt(dx * dx + dy * dy)
            if distance >= self.blast_radius:
                continue
            falloff = 1.0 - distance / self.blast_radius
            child.call("take_damage", self.blast_damage * falloff)

    def alert_nearby(self):
        x, y = self.node.get_global_position()
        set_number("rojas_alert_x", x)
        set_number("rojas_alert_y", y)
        emit("rojas_alert", self.alert_radius)

    def trigger_camera_feedback(self):
        camera_node = self.node.find(self.camera_name)
        if camera_node == None:
            return
        camera = camera_node.get_component<Camera>()
        if camera == None:
            return

        camera_x, camera_y = camera_node.get_global_position()
        distance = self.node.distance_to(camera_x, camera_y)
        if distance >= self.shake_max_distance:
            return

        strength = 1.0 - distance / self.shake_max_distance
        camera.set_trauma_profile(
            self.shake_amplitude_x,
            self.shake_amplitude_y,
            self.shake_frequency,
            self.shake_decay
        )
        camera.add_trauma(self.shake_trauma * strength)
        camera.start_zoom_punch(self.zoom_punch * strength, self.zoom_duration)

    def on_animation_finished(self, clip):
        if clip == "default":
            self.node.queue_destroy()
