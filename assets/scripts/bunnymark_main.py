class BunnymarkMain(ScriptComponent):
    bunnies_per_click = 100

    def on_start(self):
        set_number("bunnymark_bunnies", 0)
        set_number("bunnymark_next_id", 0)
        self.fps = 0
        print("Bunnymark ready: click in the Game view to spawn bunnies")

    def on_update(self, dt):
        fps = get_fps()
        if fps > 0:
            self.fps = fps
        if mouse_pressed(0):
            spawned = 0
            mouse_world_x, mouse_world_y = mouse_world_position()
            for i in range(self.bunnies_per_click):
                x = mouse_world_x + (i % 10) * 3
                y = mouse_world_y + (i // 10) * 3
                bunny = self.node.spawn("prefabs/bunnymark_bunny.k2dprefab", x, y)
                if bunny != None:
                    spawned = spawned + 1

            total = get_number("bunnymark_bunnies", 0) + spawned
            set_number("bunnymark_bunnies", total)

    def on_draw_ui(self):
        if profiler_visible():
            return
        set_draw_color(1, 1, 1, 1)
        draw_text(12, 12, "FPS: " + str(int(self.fps)), 20)
        draw_text(12, 38, "Bunnies: " + str(int(get_number("bunnymark_bunnies", 0))), 20)
        draw_text(12, 64, "Objects: " + str(object_count()), 16)
