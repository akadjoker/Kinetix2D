#pragma once

#include <glm/glm.hpp>
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

        static Matrix2D TRS(const glm::vec2 &position, float degrees, const glm::vec2 &scale)
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

        glm::vec2 Transform(float x, float y) const
        {
            return glm::vec2(a * x + c * y + tx, b * x + d * y + ty);
        }

        glm::vec2 Transform(const glm::vec2 &p) const
        {
            return Transform(p.x, p.y);
        }

        glm::vec2 Position() const
        {
            return glm::vec2(tx, ty);
        }

        bool IsTranslationOnly() const
        {
            return a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f;
        }

        glm::mat4 ToMat4() const
        {
            glm::mat4 m(1.0f);
            m[0][0] = a;
            m[0][1] = b;
            m[1][0] = c;
            m[1][1] = d;
            m[3][0] = tx;
            m[3][1] = ty;
            return m;
        }
    };

} // namespace k2d
