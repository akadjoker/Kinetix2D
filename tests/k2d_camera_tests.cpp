#include <k2d/CameraComponent.h>
#include <k2d/GameViewport.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>

#include <cstdio>
#include <cmath>

namespace
{
bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

bool testCameraFollowsOwnerTransform()
{
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("camera");
    object->setPosition(Math::Vec2(100.0f, 50.0f));
    k2d::CameraComponent* camera = object->addComponent<k2d::CameraComponent>();
    camera->setViewport(640.0f, 480.0f);

    scene.update(1.0f / 60.0f);
    bool followsInitialTransform = Near(camera->camera().position.x, 100.0f) && Near(camera->camera().position.y, 50.0f);

    object->setPosition(Math::Vec2(200.0f, 80.0f));
    scene.update(1.0f / 60.0f);
    bool followsMovedTransform = Near(camera->camera().position.x, 200.0f) && Near(camera->camera().position.y, 80.0f);

    std::printf("  camera_follows_transform: initial=%s moved=%s\n", followsInitialTransform ? "pass" : "fail",
                followsMovedTransform ? "pass" : "fail");
    return followsInitialTransform && followsMovedTransform;
}

bool testCameraWritesBackWhileFollowingTarget()
{
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("camera");
    object->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::CameraComponent* camera = object->addComponent<k2d::CameraComponent>();
    camera->setViewport(640.0f, 480.0f);

    k2d::Camera2D& camera2D = camera->camera();
    camera2D.setSmoothing(false, 0.0f);
    camera2D.setTarget(Math::Vec2(300.0f, 150.0f));

    scene.update(1.0f / 60.0f);
    bool cameraAtTarget = Near(camera2D.position.x, 300.0f) && Near(camera2D.position.y, 150.0f);
    bool ownerFollowsCamera =
        Near(object->position().x, camera2D.position.x) && Near(object->position().y, camera2D.position.y);

    std::printf("  camera_writes_back: camera_at_target=%s owner_follows=%s (owner=%.1f,%.1f)\n",
                cameraAtTarget ? "pass" : "fail", ownerFollowsCamera ? "pass" : "fail", object->position().x,
                object->position().y);
    return cameraAtTarget && ownerFollowsCamera;
}

bool testLegacyCameraPositionMigratesOntoOwnerTransform()
{
    k2d::Scene source;
    k2d::GameObject* legacy = source.createObject("legacy_camera");
    k2d::CameraComponent* legacyCamera = legacy->addComponent<k2d::CameraComponent>();
    legacyCamera->setViewport(640.0f, 480.0f);
    legacyCamera->setViewportScaleMode(k2d::ViewportScaleMode::Expand);
    legacyCamera->setIntegerScale(true);
    legacyCamera->camera().position = Math::Vec2(10.0f, 20.0f);
    legacyCamera->camera().rotationDegrees = 5.0f;
    const ct::Json legacyJson = k2d::Serializer::WriteObject(*legacy);

    k2d::Scene migratedScene;
    k2d::GameObject* migrated = k2d::Serializer::ReadObject(migratedScene, legacyJson);
    k2d::CameraComponent* migratedCamera = migrated ? migrated->getComponent<k2d::CameraComponent>() : nullptr;
    bool migrated_ok = migrated && migratedCamera && Near(migrated->position().x, 10.0f) &&
                       Near(migrated->position().y, 20.0f) && Near(migrated->rotationDegrees(), 5.0f) &&
                       migratedCamera->viewportScaleMode() == k2d::ViewportScaleMode::Expand &&
                       migratedCamera->integerScale();

    k2d::GameObject* placed = source.createObject("placed_camera");
    placed->setPosition(Math::Vec2(7.0f, 7.0f));
    k2d::CameraComponent* placedCamera = placed->addComponent<k2d::CameraComponent>();
    placedCamera->camera().position = Math::Vec2(999.0f, 999.0f);
    const ct::Json placedJson = k2d::Serializer::WriteObject(*placed);

    k2d::Scene placedScene;
    k2d::GameObject* reloaded = k2d::Serializer::ReadObject(placedScene, placedJson);
    bool ownerTransformWins = reloaded && Near(reloaded->position().x, 7.0f) && Near(reloaded->position().y, 7.0f);

    std::printf("  camera_legacy_migration: adopted=%s owner_wins=%s\n", migrated_ok ? "pass" : "fail",
                ownerTransformWins ? "pass" : "fail");
    return migrated_ok && ownerTransformWins;
}

} // namespace

int main()
{
    const bool followsTransform = testCameraFollowsOwnerTransform();
    const bool writesBack = testCameraWritesBackWhileFollowingTarget();
    const bool legacyMigration = testLegacyCameraPositionMigratesOntoOwnerTransform();

    const k2d::GameViewport fit =
        k2d::CalculateGameViewport(1920.0f, 1080.0f, 640.0f, 480.0f, k2d::ViewportScaleMode::Fit);
    const bool fitViewport = Near(fit.x, 240.0f) && Near(fit.y, 0.0f) && Near(fit.width, 1440.0f) &&
                             Near(fit.height, 1080.0f) && Near(fit.toVirtualX(960.0f), 320.0f) &&
                             Near(fit.toVirtualY(540.0f), 240.0f);
    const k2d::GameViewport integerFit =
        k2d::CalculateGameViewport(1920.0f, 1080.0f, 640.0f, 480.0f, k2d::ViewportScaleMode::Fit, true);
    const bool integerViewport = Near(integerFit.x, 320.0f) && Near(integerFit.y, 60.0f) &&
                                 Near(integerFit.width, 1280.0f) && Near(integerFit.height, 960.0f);
    const k2d::GameViewport expand =
        k2d::CalculateGameViewport(1920.0f, 1080.0f, 640.0f, 480.0f, k2d::ViewportScaleMode::Expand);
    const bool expandViewport = Near(expand.x, 0.0f) && Near(expand.y, 0.0f) && Near(expand.width, 1920.0f) &&
                                Near(expand.height, 1080.0f) && Near(expand.virtualWidth, 853.333333f) &&
                                Near(expand.virtualHeight, 480.0f);

    k2d::CameraComponent camera;
    camera.setViewport(1280.0f, 720.0f);
    camera.camera().position = Math::Vec2(640.0f, 360.0f);

    Math::Vec2 center = camera.screenToWorld(640.0f, 360.0f);
    float minX, minY, maxX, maxY;
    camera.visibleRect(minX, minY, maxX, maxY);
    bool centerMaps = Near(center.x, 640.0f) && Near(center.y, 360.0f);
    bool viewportMaps = Near(minX, 0.0f) && Near(minY, 0.0f) && Near(maxX, 1280.0f) && Near(maxY, 720.0f);

    camera.camera().zoom = Math::Vec2(2.0f);
    camera.setViewport(800.0f, 600.0f);
    camera.camera().position = Math::Vec2(400.0f, 300.0f);
    camera.visibleRect(minX, minY, maxX, maxY);
    bool resized = Near(minX, 200.0f) && Near(minY, 150.0f) && Near(maxX, 600.0f) && Near(maxY, 450.0f);

    k2d::Camera2D follow;
    follow.setLimits(0.0f, 0.0f, 2000.0f, 1000.0f);
    follow.setTarget(Math::Vec2(1900.0f, 900.0f));
    follow.update(1.0f, 800.0f, 600.0f);
    bool limits = Near(follow.position.x, 1600.0f) && Near(follow.position.y, 700.0f);

    k2d::Camera2D smooth;
    smooth.setSmoothing(true, 5.0f);
    smooth.setTarget(Math::Vec2(100.0f, 0.0f));
    smooth.update(0.1f, 800.0f, 600.0f);
    bool smoothing = smooth.position.x > 0.0f && smooth.position.x < 100.0f;

    k2d::Camera2D deadZone;
    deadZone.setDeadZonePixels(300.0f, 200.0f, 500.0f, 400.0f);
    deadZone.setTarget(Math::Vec2(200.0f, 200.0f));
    deadZone.update(1.0f, 800.0f, 600.0f);
    bool deadZoneApplied = Near(deadZone.position.x, 100.0f) && Near(deadZone.position.y, 100.0f);

    k2d::Camera2D feedback;
    feedback.position = Math::Vec2(400.0f, 300.0f);
    feedback.setTraumaProfile(14.0f, 9.0f, 8.0f, 2.0f);
    feedback.addTrauma(1.0f);
    feedback.update(0.1f, 800.0f, 600.0f);
    const Math::Vec2 shakenCenter = feedback.ScreenToWorld(400.0f, 300.0f, 800.0f, 600.0f);
    const bool trauma = feedback.isShaking() &&
                        (!Near(shakenCenter.x, feedback.position.x) || !Near(shakenCenter.y, feedback.position.y));
    feedback.clearTrauma();
    feedback.startShake(12.0f, 8.0f, 10.0f, 2.0f);
    feedback.update(0.01f, 800.0f, 600.0f);
    const bool shake =
        feedback.isShaking() && (std::fabs(feedback.shakeOffset.x) > 0.0f || std::fabs(feedback.shakeOffset.y) > 0.0f);
    feedback.stopShake();
    feedback.startZoomPunch(0.25f, 0.5f);
    feedback.update(0.25f, 800.0f, 600.0f);
    const bool zoomPunch = feedback.isZoomPunching() && Near(feedback.zoomPunch.scale, 1.25f);
    feedback.update(0.5f, 800.0f, 600.0f);
    const bool feedbackStops = !feedback.isShaking() && !feedback.isZoomPunching() &&
                               Near(feedback.shakeOffset.x, 0.0f) && Near(feedback.shakeOffset.y, 0.0f);

    std::printf("camera: transform_follow=%s writes_back=%s legacy_migration=%s fit=%s integer=%s expand=%s center=%s viewport=%s "
                "resize_zoom=%s limits=%s smoothing=%s dead_zone=%s trauma=%s shake=%s zoom_punch=%s stop=%s\n",
                followsTransform ? "pass" : "fail", writesBack ? "pass" : "fail", legacyMigration ? "pass" : "fail",
                fitViewport ? "pass" : "fail", integerViewport ? "pass" : "fail", expandViewport ? "pass" : "fail",
                centerMaps ? "pass" : "fail", viewportMaps ? "pass" : "fail", resized ? "pass" : "fail",
                limits ? "pass" : "fail", smoothing ? "pass" : "fail", deadZoneApplied ? "pass" : "fail",
                trauma ? "pass" : "fail", shake ? "pass" : "fail", zoomPunch ? "pass" : "fail",
                feedbackStops ? "pass" : "fail");
    return followsTransform && writesBack && legacyMigration && fitViewport && integerViewport && expandViewport &&
                   centerMaps && viewportMaps && resized && limits && smoothing && deadZoneApplied && trauma && shake &&
                   zoomPunch && feedbackStops
               ? 0
               : 1;
}
