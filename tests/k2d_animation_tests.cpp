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

    bool removeMissing = !animation.removeClip("nope") && animation.clipCount() == 2;
    bool removeActive = animation.removeClip("attack") && animation.clipCount() == 1 &&
                        std::strcmp(animation.currentClip(), "idle") == 0;
    animation.addClip("attack", nullptr, 16, 16, 3, 12.0f, k2d::AnimationMode::OneShot);
    animation.play("attack");
    bool removeBefore = animation.removeClip("idle") && animation.clipCount() == 1 &&
                        std::strcmp(animation.currentClip(), "attack") == 0;
    bool removeLast = animation.removeClip("attack") && animation.clipCount() == 0 &&
                      std::strcmp(animation.currentClip(), "") == 0;

    std::printf("animation: clips=%s frame_clamped=%s stopped=%s no_texture=%s pingpong=%s loop=%s switch=%s "
                "remove_missing=%s remove_active=%s remove_before=%s remove_last=%s\n",
                clips ? "pass" : "fail",
                frameClamped ? "pass" : "fail", stopped ? "pass" : "fail",
                noTexture ? "pass" : "fail", pingPongMode ? "pass" : "fail",
                loopCompatibility ? "pass" : "fail", switchClip ? "pass" : "fail",
                removeMissing ? "pass" : "fail", removeActive ? "pass" : "fail",
                removeBefore ? "pass" : "fail", removeLast ? "pass" : "fail");
    return clips && frameClamped && stopped && noTexture && pingPongMode &&
                   loopCompatibility && switchClip && removeMissing && removeActive &&
                   removeBefore && removeLast
               ? 0
               : 1;
}