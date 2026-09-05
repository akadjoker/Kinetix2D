import math

IDLE_LINES = [
    "Another quiet shift...",
    "My feet are killing me.",
    "Did I lock the gate?",
    "Coffee. I need coffee.",
    "This place gives me the creeps.",
    "Nothing ever happens here.",
    "Six more hours of this.",
    "I should have taken the docks job.",
    "Was that a noise?",
    "Boss never pays overtime."
]

ALERT_LINES = [
    "Who's there?!",
    "I saw something move!",
    "Hey! Stop right there!",
    "Intruder! Sound the alarm!",
    "Found you!",
    "Not on my watch!",
    "You picked the wrong port, pal.",
    "Get him!"
]

SHOOT_LINES = [
    "Eat lead!",
    "Hold still!",
    "Take this!",
    "You're done for!",
    "Die already!"
]

FLEE_LINES = [
    "I'm out! I'm out!",
    "Need to reload!",
    "Cover me!",
    "This is way above my pay grade!",
    "Falling back!"
]

MOOD_NONE = 0
MOOD_IDLE = 1
MOOD_ALERT = 2
MOOD_SHOOT = 3
MOOD_FLEE = 4


class ThoughtBubble(ScriptComponent):
    offset_x = 0.0
    offset_y = -70.0
    font_size = 14.0
    padding = 7.0
    hold_time = 2.2
    shout_chance = 0.35
    fade_time = 0.35
    idle_min_gap = 5.0
    idle_max_gap = 11.0
    idle_chance = 0.55
    max_line_width = 190.0

    def on_start(self):
        self.text = ""
        self.timer = 0.0
        self.duration = 0.0
        self.mood = MOOD_NONE
        self.silenced = False
        self.alerted = False
        self.idle_timer = math.random(self.idle_min_gap * 0.25, self.idle_max_gap)
        self.lines = []
        self.line_width = 0.0

    def on_update(self, dt):
        if self.silenced:
            return

        if self.text != "":
            self.timer = self.timer - dt
            if self.timer <= 0.0:
                self.text = ""
            return

        if self.alerted:
            return

        self.idle_timer = self.idle_timer - dt
        if self.idle_timer <= 0.0:
            self.idle_timer = math.random(self.idle_min_gap, self.idle_max_gap)
            if math.random() < self.idle_chance:
                self.say(pick(IDLE_LINES), MOOD_IDLE)

    # rojas.py drives these through node.call(), which always passes a single
    # numeric value. The bubble never reads the enemy's state itself.
    def think_alert(self, value):
        self.alerted = True
        self.say(pick(ALERT_LINES), MOOD_ALERT)

    def think_shoot(self, value):
        self.alerted = True
        if self.text == "" and math.random() < self.shout_chance:
            self.say(pick(SHOOT_LINES), MOOD_SHOOT)

    def think_flee(self, value):
        self.alerted = True
        self.say(pick(FLEE_LINES), MOOD_FLEE)

    def think_calm(self, value):
        self.alerted = False

    def think_silence(self, value):
        self.silenced = True
        self.text = ""

    # rojas.py's node.call() reaches this script too; absorb its messages.
    def take_damage(self, amount):
        return

    def finish_death(self, value):
        return

    def say(self, text, mood):
        self.text = text
        self.mood = mood
        self.duration = self.hold_time
        self.timer = self.duration
        self.wrap(text)

    def wrap(self, text):
        self.lines = []
        widest = 0.0
        line = ""
        word = ""
        index = 0
        length = len(text)
        while index <= length:
            character = " "
            if index < length:
                character = text[index]
            index = index + 1
            if character != " ":
                word = word + character
                continue
            if word == "":
                continue
            candidate = word
            if line != "":
                candidate = line + " " + word
            if draw_text_width(candidate, self.font_size) > self.max_line_width and line != "":
                width = draw_text_width(line, self.font_size)
                if width > widest:
                    widest = width
                self.lines.append(line)
                line = word
            else:
                line = candidate
            word = ""
        if line != "":
            width = draw_text_width(line, self.font_size)
            if width > widest:
                widest = width
            self.lines.append(line)
        self.line_width = widest

    def bubble_color(self):
        if self.mood == MOOD_ALERT:
            return 0.95, 0.78, 0.16
        if self.mood == MOOD_SHOOT:
            return 0.95, 0.30, 0.16
        if self.mood == MOOD_FLEE:
            return 0.45, 0.62, 0.95
        return 0.92, 0.92, 0.88

    def on_draw(self):
        if self.text == "" or len(self.lines) == 0:
            return

        alpha = 1.0
        if self.timer < self.fade_time:
            alpha = self.timer / self.fade_time
        appear = self.duration - self.timer
        if appear < self.fade_time:
            alpha = appear / self.fade_time
        if alpha <= 0.0:
            return
        if alpha > 1.0:
            alpha = 1.0

        line_height = self.font_size + 4.0
        count = len(self.lines)
        width = self.line_width + self.padding * 2.0
        height = line_height * count + self.padding * 2.0

        x, y = self.node.get_global_position()
        left = x + self.offset_x - width * 0.5
        top = y + self.offset_y - height

        set_draw_color(0.0, 0.0, 0.0, 0.55 * alpha)
        draw_rect(left + 2.0, top + 3.0, width, height, True)

        red, green, blue = self.bubble_color()
        set_draw_color(red, green, blue, 0.92 * alpha)
        draw_rect(left, top, width, height, True)

        tail_x = x + self.offset_x
        step = 0
        steps = 5
        while step < steps:
            half = 6.0 * (1.0 - step / steps)
            draw_rect(tail_x - half, top + height - 1.0 + step * 2.0, half * 2.0, 2.0, True)
            step = step + 1

        set_draw_color(0.08, 0.07, 0.06, 0.85 * alpha)
        draw_rect(left, top, width, height, False, 1.5)

        set_draw_color(0.06, 0.05, 0.05, alpha)
        index = 0
        while index < count:
            line = self.lines[index]
            line_x = tail_x - draw_text_width(line, self.font_size) * 0.5
            draw_text(line_x, top + self.padding + line_height * index, line, self.font_size)
            index = index + 1


def pick(lines):
    return lines[int(math.random(0.0, len(lines) - 0.001))]
