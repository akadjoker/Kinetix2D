#include <k2d/Animation2D.h>

#include <cstdio>
#include <cstring>

int main()
{
    k2d::Animation2D animation;
    animation.addClip("idle", nullptr, 16, 16, 4, 8.0f, k2d::AnimationMode::Loop);
    animation.addClip("attack", nullptr, 16, 16, 3, 12.0f, k2d::AnimationMode::OneShot);
    bool clips = animation.clipCount() == 2 && animation.play("idle") &&
                 std::strcmp(animation.currentClip(), "idle") == 0;
    animation.setLoop(false);
    animation.setFrame(9);
    bool frameClamped = animation.frame() == 3;
    animation.stop();
    bool stopped = !animation.playing();
    bool noTexture = animation.texture() == nullptr;
    animation.setMode(k2d::AnimationMode::PingPong);
    bool pingPongMode = animation.mode() == k2d::AnimationMode::PingPong &&
                        !animation.loop();
    animation.setLoop(true);
    bool loopCompatibility = animation.mode() == k2d::AnimationMode::Loop && animation.loop();

    bool switchClip = animation.play("attack") &&
                      std::strcmp(animation.currentClip(), "attack") == 0 &&
                      animation.frameCount() == 3;
    std::printf("animation: clips=%s frame_clamped=%s stopped=%s no_texture=%s pingpong=%s loop=%s switch=%s\n",
                clips ? "pass" : "fail",
                frameClamped ? "pass" : "fail", stopped ? "pass" : "fail",
                noTexture ? "pass" : "fail", pingPongMode ? "pass" : "fail",
                loopCompatibility ? "pass" : "fail", switchClip ? "pass" : "fail");
    return clips && frameClamped && stopped && noTexture && pingPongMode &&
                   loopCompatibility && switchClip
               ? 0
               : 1;
}
