import math


class SeparationProbe(ScriptComponent):
    agent_count = 8

    def on_start(self):
        self.timer = 0.0
        self.nodes = []
        for i in range(self.agent_count):
            n = self.node.find(f"agent{i}")
            if n != None:
                self.nodes.append(n)

    def min_distance(self):
        best = -1.0
        count = len(self.nodes)
        for i in range(count):
            xi, yi = self.nodes[i].get_position()
            for j in range(i + 1, count):
                xj, yj = self.nodes[j].get_position()
                dx = xi - xj
                dy = yi - yj
                d = math.sqrt(dx * dx + dy * dy)
                if best < 0.0 or d < best:
                    best = d
        return best

    def on_update(self, dt):
        self.timer = self.timer + dt
        if self.timer >= 1.0:
            self.timer = self.timer - 1.0
            print(f"min_pairwise_distance={self.min_distance()}")
