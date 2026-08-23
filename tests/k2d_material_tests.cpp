#include <k2d/Material2D.h>

#include <cstdio>
#include <cmath>

namespace
{
    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 0.0001f;
    }
}

int main()
{
    k2d::Material2D material;
    material.setColor(128, 64, 255, 32);
    material.setSourceRect(16.0f, 8.0f, 32.0f, 24.0f);
    material.setPivot(Math::Vec2(0.0f, 1.0f));
    material.setFlip(true, false);
    material.setBlendMode(k2d::BLEND_ADD);

    bool color = Near(material.color().r, 128.0f / 255.0f) &&
                 Near(material.color().g, 64.0f / 255.0f) &&
                 Near(material.color().b, 1.0f) &&
                 Near(material.color().a, 32.0f / 255.0f);
    bool atlas = material.hasSourceRect() && material.sourceRect() == Math::Vec4(16.0f, 8.0f, 32.0f, 24.0f);
    bool state = material.pivot() == Math::Vec2(0.0f, 1.0f) && material.flipX() &&
                 !material.flipY() && material.blendMode() == k2d::BLEND_ADD;
    material.clearSourceRect();
    bool clear = !material.hasSourceRect() && material.sourceRect() == Math::Vec4(0.0f);

    std::printf("material: color=%s atlas=%s state=%s clear=%s\n",
                color ? "pass" : "fail", atlas ? "pass" : "fail",
                state ? "pass" : "fail", clear ? "pass" : "fail");
    return color && atlas && state && clear ? 0 : 1;
}