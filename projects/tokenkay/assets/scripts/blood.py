class Blood(ScriptComponent):
    finished_event = "blood_finished"

    def on_event(self, name, value):
        if name != self.finished_event:
            return

        # Rojas reparents this effect to itself. It stays visible while the
        # parent blinks and is removed automatically together with the parent.
        parent = self.node.get_parent()
        if parent != None:
            parent.call("finish_death")
