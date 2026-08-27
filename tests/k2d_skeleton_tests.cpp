#include <k2d/Bone2D.h>
#include <k2d/Scene.h>
#include <k2d/Skeleton2D.h>

#include <cmath>
#include <cstdio>

static bool closeTo(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.001f;
}

int main()
{
    k2d::BoneTrack track;
    track.boneName = "Arm";
    track.property = k2d::BoneTrack::Rotation;
    track.addKeyframe(0.0f, 0.0f);
    track.addKeyframe(1.0f, 90.0f);
    const bool interpolation = closeTo(track.sample(0.5f), 45.0f);

    k2d::Scene scene;
    k2d::GameObject* root = scene.createObject("player");
    k2d::Skeleton2D* skeleton = root->addComponent<k2d::Skeleton2D>();
    k2d::GameObject* arm = scene.createObject("Arm", root);
    arm->setRotationDegrees(10.0f);
    arm->addComponent<k2d::Bone2D>();

    k2d::BoneAnimationClip clip;
    clip.name = "wave";
    clip.duration = 1.0f;
    clip.tracks.push_back(track);
    skeleton->addClip(clip);
    bool finished = false;
    skeleton->onAnimationFinished = [&finished](const char*) { finished = true; };

    const bool found = skeleton->findBone("Arm") != nullptr;
    const bool started = skeleton->play("wave", false);
    scene.update(0.5f);
    const bool halfway = closeTo(arm->rotationDegrees(), 55.0f);
    skeleton->seek(0.25f);
    const bool seeked = closeTo(arm->rotationDegrees(), 32.5f);
    scene.update(0.8f);
    const bool completed = !skeleton->playing() && finished && closeTo(arm->rotationDegrees(), 100.0f);
    skeleton->stop();
    const bool reset = closeTo(arm->rotationDegrees(), 10.0f);

    k2d::GameObject* forearm = scene.createObject("Forearm", arm);
    forearm->setPosition(Math::Vec2(40.0f, 0.0f));
    forearm->addComponent<k2d::Bone2D>()->setLength(40.0f);
    arm->setRotationDegrees(0.0f);
    forearm->setRotationDegrees(0.0f);
    const bool ikStarted = skeleton->solveIK("Forearm", Math::Vec2(40.0f, 40.0f), 2, 16, 0.1f);
    const Math::Vec2 ikTip = forearm->globalTransform().Transform(40.0f, 0.0f);
    const bool ikSolved = ikStarted && std::hypot(ikTip.x - 40.0f, ikTip.y - 40.0f) <= 0.1f;

    std::printf("skeleton: interpolation=%s found=%s started=%s halfway=%s seek=%s completed=%s reset=%s ik=%s\n",
                interpolation ? "pass" : "fail", found ? "pass" : "fail",
                started ? "pass" : "fail", halfway ? "pass" : "fail",
                seeked ? "pass" : "fail", completed ? "pass" : "fail",
                reset ? "pass" : "fail", ikSolved ? "pass" : "fail");
    return interpolation && found && started && halfway && seeked && completed && reset && ikSolved ? 0 : 1;
}
