#include <k2d/Animation2D.h>
#include <k2d/SpriteAtlas.h>
#include <k2d/Scene.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Pixmap.h>

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

    animation.addClip("frames", nullptr, 1, 1, 1, 10.0f, k2d::AnimationMode::Loop);
    animation.addFrame("frames", nullptr, Math::Vec4(0.0f, 0.0f, 16.0f, 24.0f));
    animation.addFrame("frames", nullptr, Math::Vec4(16.0f, 0.0f, 16.0f, 24.0f));
    animation.play("frames");
    const k2d::AnimationFrame *firstFrame = animation.frameAt("frames", 0);
    bool individualFrames = animation.frameCount() == 2 && firstFrame && firstFrame->rect.z == 16.0f;
    animation.Advance(0.11f);
    bool individualAdvance = animation.frame() == 1 && animation.removeFrame("frames", 1) &&
                             animation.frameCount() == 1;

    bool switchClip = animation.play("attack") &&
                      std::strcmp(animation.currentClip(), "attack") == 0 &&
                      animation.frameCount() == 3;

    animation.removeClip("frames");
    bool removeMissing = !animation.removeClip("nope") && animation.clipCount() == 2;
    bool removeActive = animation.removeClip("attack") && animation.clipCount() == 1 &&
                        std::strcmp(animation.currentClip(), "idle") == 0;
    animation.addClip("attack", nullptr, 16, 16, 3, 12.0f, k2d::AnimationMode::OneShot);
    animation.play("attack");
    bool removeBefore = animation.removeClip("idle") && animation.clipCount() == 1 &&
                        std::strcmp(animation.currentClip(), "attack") == 0;
    bool removeLast = animation.removeClip("attack") && animation.clipCount() == 0 &&
                      std::strcmp(animation.currentClip(), "") == 0;

    animation.addClip("offset", nullptr, 0, 0, 0, 10.0f, k2d::AnimationMode::Loop);
    animation.addFrame("offset", nullptr, Math::Vec4(32.0f, 0.0f, 16.0f, 24.0f));
    animation.setFrameOffset("offset", 0, Math::Vec2(3.0f, -5.0f));
    const k2d::AnimationFrame *offsetFrame = animation.frameAt("offset", 0);
    bool frameOffsets = offsetFrame && offsetFrame->offset.x == 3.0f && offsetFrame->offset.y == -5.0f;

    k2d::SpriteAtlas atlas;
    atlas.setTexturePath("assets/hero.png");
    bool spriteMetadata = atlas.add("hero_idle", Math::Vec4(0.0f, 0.0f, 16.0f, 16.0f)) &&
                          !atlas.add("hero_idle", Math::Vec4(0.0f, 0.0f, 16.0f, 16.0f)) &&
                          atlas.find("hero_idle") != nullptr &&
                          atlas.set("hero_idle", "hero_idle_edited", Math::Vec4(16.0f, 0.0f, 16.0f, 16.0f)) &&
                          atlas.find("hero_idle_edited") != nullptr && atlas.remove("hero_idle_edited") &&
                          atlas.regionCount() == 0;

    k2d::Scene scene;
    k2d::GameObject *animatedObject = scene.createObject("animated");
    k2d::Animation2D *autoAnimation = animatedObject->addComponent<k2d::Animation2D>();
    autoAnimation->addClip("default", nullptr, 16, 16, 1, 1.0f, k2d::AnimationMode::Loop);
    autoAnimation->play("default");
    bool explicitSpriteRenderer = animatedObject->getComponent<k2d::SpriteComponent>() == nullptr;

    k2d::Pixmap alphaImage(12, 8);
    alphaImage.Clear(0, 0, 0, 0);
    alphaImage.FillRect(1, 1, 3, 2, 255, 255, 255, 255);
    alphaImage.FillRect(8, 3, 2, 4, 255, 255, 255, 255);
    const size_t detected = atlas.detectTransparentRegions(alphaImage, 1, 2);
    const k2d::SpriteRegion *detectedFirst = atlas.regionAt(0);
    bool transparentDetection = detected == 2 && detectedFirst && detectedFirst->rect.x == 1.0f &&
                               detectedFirst->rect.y == 1.0f && detectedFirst->rect.z == 3.0f &&
                               detectedFirst->rect.w == 2.0f;

    std::printf("animation: clips=%s frame_clamped=%s stopped=%s no_texture=%s pingpong=%s loop=%s frames=%s frame_advance=%s switch=%s "
                "remove_missing=%s remove_active=%s remove_before=%s remove_last=%s frame_offsets=%s sprite_metadata=%s explicit_sprite=%s alpha_detect=%s\n",
                clips ? "pass" : "fail",
                frameClamped ? "pass" : "fail", stopped ? "pass" : "fail",
                noTexture ? "pass" : "fail", pingPongMode ? "pass" : "fail",
                loopCompatibility ? "pass" : "fail", individualFrames ? "pass" : "fail",
                individualAdvance ? "pass" : "fail", switchClip ? "pass" : "fail",
                removeMissing ? "pass" : "fail", removeActive ? "pass" : "fail",
                removeBefore ? "pass" : "fail", removeLast ? "pass" : "fail", frameOffsets ? "pass" : "fail",
                spriteMetadata ? "pass" : "fail", explicitSpriteRenderer ? "pass" : "fail",
                transparentDetection ? "pass" : "fail");
    return clips && frameClamped && stopped && noTexture && pingPongMode &&
                   loopCompatibility && individualFrames && individualAdvance && switchClip && removeMissing && removeActive &&
                   removeBefore && removeLast && frameOffsets && spriteMetadata && explicitSpriteRenderer && transparentDetection
               ? 0
               : 1;
}
