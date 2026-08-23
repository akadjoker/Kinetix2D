COOLDOWN = 0.4
timer = 0.0

def ready(node):
    set_number("spawned", 0)

def update(node, dt):
    global timer
    timer = timer - dt
    if timer <= 0 and key_down("space"):
        timer = COOLDOWN
        bullet = node.spawn("assets/prefabs/bullet.k2dprefab", node.get_x(), node.get_y())
        if bullet != None:
            set_number("spawned", get_number("spawned", 0) + 1)
            emit("shot", get_number("spawned", 0))

def on_event(node, name, value):
    if name == "player_died":
        set_flag("can_shoot", False)
