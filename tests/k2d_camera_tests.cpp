#include <k2d/CameraComponent.h>

#include <cstdio>
#include <cmath>

namespace
{
bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}
} // namespace

int main()
{
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

    std::printf("camera: center=%s viewport=%s resize_zoom=%s limits=%s smoothing=%s dead_zone=%s trauma=%s shake=%s "
                "zoom_punch=%s stop=%s\n",
                centerMaps ? "pass" : "fail", viewportMaps ? "pass" : "fail", resized ? "pass" : "fail",
                limits ? "pass" : "fail", smoothing ? "pass" : "fail", deadZoneApplied ? "pass" : "fail",
                trauma ? "pass" : "fail", shake ? "pass" : "fail", zoomPunch ? "pass" : "fail",
                feedbackStops ? "pass" : "fail");
    return centerMaps && viewportMaps && resized && limits && smoothing && deadZoneApplied && trauma && shake &&
                   zoomPunch && feedbackStops
               ? 0
               : 1;
}
