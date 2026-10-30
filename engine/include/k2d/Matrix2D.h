#pragma once

#include <mathc.h>
#include <cmath>

namespace k2d
{

    struct Matrix2D
    {
        float a, b, c, d, tx, ty;

        Matrix2D() : a(1.0f), b(0.0f), c(0.0f), d(1.0f), tx(0.0f), ty(0.0f)
        {
        }

        Matrix2D(float a_, float b_, float c_, float d_, float tx_, float ty_)
            : a(a_), b(b_), c(c_), d(d_), tx(tx_), ty(ty_)
        {
        }

        static Matrix2D Identity()
        {
            return Matrix2D();
        }

        static Matrix2D Translation(float x, float y)
        {
            return Matrix2D(1.0f, 0.0f, 0.0f, 1.0f, x, y);
        }

        static Matrix2D Rotation(float degrees)
        {
            float r = degrees * 0.01745329251f;
            float cs = cosf(r);
            float sn = sinf(r);
            return Matrix2D(cs, sn, -sn, cs, 0.0f, 0.0f);
        }

        static Matrix2D Scaling(float x, float y)
        {
            return Matrix2D(x, 0.0f, 0.0f, y, 0.0f, 0.0f);
        }

        static Matrix2D TRS(const Math::Vec2 &position, float degrees, const Math::Vec2 &scale)
        {
            float r = degrees * 0.01745329251f;
            float cs = cosf(r);
            float sn = sinf(r);
            return Matrix2D(cs * scale.x, sn * scale.x,
                            -sn * scale.y, cs * scale.y,
                            position.x, position.y);
        }

        Matrix2D operator*(const Matrix2D &o) const
        {
            return Matrix2D(
                a * o.a + c * o.b,
                b * o.a + d * o.b,
                a * o.c + c * o.d,
                b * o.c + d * o.d,
                a * o.tx + c * o.ty + tx,
                b * o.tx + d * o.ty + ty);
        }

        Math::Vec2 Transform(float x, float y) const
        {
            return Math::Vec2(a * x + c * y + tx, b * x + d * y + ty);
        }

        Math::Vec2 Transform(const Math::Vec2 &p) const
        {
            return Transform(p.x, p.y);
        }

        Math::Vec2 Position() const
        {
            return Math::Vec2(tx, ty);
        }

        float RotationDegrees() const
        {
            return std::atan2(b, a) * 57.29577951308232088f;
        }

        Matrix2D AffineInverse() const
        {
            float det = a * d - b * c;
            float ia = d / det;
            float ib = -b / det;
            float ic = -c / det;
            float id = a / det;
            return Matrix2D(ia, ib, ic, id,
                            -(ia * tx + ic * ty),
                            -(ib * tx + id * ty));
        }

        bool IsTranslationOnly() const
        {
            return a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f;
        }

        Math::Mat4 ToMat4() const
        {
            Math::Mat4 m;
            m[0].x = a;
            m[0].y = b;
            m[1].x = c;
            m[1].y = d;
            m[3].x = tx;
            m[3].y = ty;
            return m;
        }
    };

} 