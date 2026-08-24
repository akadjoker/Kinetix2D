class Test:
    speed = 100
    count = 20

    def __init__(self, node):
        self.node = node

    def on_start(self):
        print("Teste iniciado")

    def on_update(self, dt):
        self.node.translate(self.speed * dt, 0)
        self.count = self.count + 1
