class AudioDirector(ScriptComponent):
    music_volume = 0.28
    music_fade = 1.2

    def on_start(self):
        # The blackboard survives a runtime scene reload, so sounds are loaded
        # once and the music continues instead of restarting after a death.
        if not get_flag("tokenkay_audio_loaded", False):
            set_number("sfx_pistol", audio_load("assets/audio/pistola0.wav"))
            set_number("sfx_explosion", audio_load("assets/audio/explo1.wav"))
            body_hit = audio_load("assets/audio/golpe27.wav")
            set_number("sfx_player_hit", body_hit)
            set_number("sfx_death", body_hit)
            set_number("sfx_impact_0", audio_load("assets/audio/impacto0.wav"))
            set_number("sfx_impact_1", audio_load("assets/audio/impacto1.wav"))
            set_number("sfx_impact_2", audio_load("assets/audio/impacto2.wav"))
            set_number("sfx_impact_3", audio_load("assets/audio/impacto3.wav"))
            set_number("sfx_impact_4", audio_load("assets/audio/impacto4.wav"))
            set_number("sfx_impact_5", audio_load("assets/audio/impacto5.wav"))
            set_number("sfx_impact_6", audio_load("assets/audio/impacto6.wav"))
            set_number("music_tokenkay", audio_load_music("assets/audio/token_theme.ogg"))
            set_flag("tokenkay_audio_loaded", True)

        voice = int(get_number("music_voice", 0.0))
        if voice == 0 or not audio_playing(voice):
            music = int(get_number("music_tokenkay", 0.0))
            if music != 0:
                voice = audio_crossfade_music(music, True, self.music_volume, self.music_fade)
                set_number("music_voice", voice)
