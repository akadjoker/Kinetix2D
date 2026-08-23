import math

RADIUS = 120
SPEED = 2.0
t = 0.0

def ready(node):
    target = node.find("player")
    if target != None:
        print("orbiting around:", target.get_name())

def update(node, dt):
    global t
    t = t + dt * SPEED
    target = node.find("player")
    if target != None:
        cx, cy = target.get_position()
        node.set_position(cx + math.cos(t) * RADIUS, cy + math.sin(t) * RADIUS)
    node.rotate(180 * dt)
