def ready(node):
    set_number("score", 0)
    set_string("state", "playing")

def update(node, dt):
    if get_string("state", "") == "gameover":
        node.set_visible(True)

def on_event(node, name, value):
    if name == "shot":
        print("shots fired:", value)
    if name == "enemy_killed":
        set_number("score", get_number("score", 0) + value)
        print("score:", get_number("score", 0))
    if name == "player_died":
        set_string("state", "gameover")
