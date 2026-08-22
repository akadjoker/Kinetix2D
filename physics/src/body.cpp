#include "kx/body.h"
#include "kx/internal/triangulate.h"

#include <cmath>

namespace kx
{

    Body::Body()
        : mType(BodyType::Static),
          mPosition(0.0f, 0.0f), mAngle(0.0f),
          mLinearVelocity(0.0f, 0.0f), mAngularVelocity(0.0f),
          mSleepTime(0.0f), mAwake(true), mFixedRotation(false),
          mInvMass(0.0f), mInvI(0.0f), mLocalCenter(0.0f, 0.0f),
          mFriction(0.2f), mRestitution(0.0f),
          mLinearDamping(0.0f), mAngularDamping(0.0f), mGravityScale(1.0f),
          mForce(0.0f, 0.0f), mTorque(0.0f), mId(0),
          mUserData(nullptr), mContactCallback(nullptr), mContactContext(nullptr),
          mProxyId(-1), mProxyPosition(0.0f, 0.0f),
          mShapeCount(0)
    {
    }

    int Body::AddCircle(const glm::vec2 &localCenter, float radius, float density)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Circle;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.circle.center = localCenter;
        shape.circle.radius = radius;

        RecomputeMass();
        return 1;
    }

    int Body::AddBox(float halfWidth, float halfHeight, const glm::vec2 &localCenter, float density)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Polygon;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.polygon.SetAsBox(halfWidth, halfHeight, localCenter, 0.0f);

        RecomputeMass();
        return 1;
    }

    int Body::AddPolygon(const glm::vec2 *points, int count, float density)
    {
        if (mShapeCount >= kMaxShapes || count < 3)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Polygon;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.polygon.Set(points, count);

        RecomputeMass();
        return 1;
    }

    int Body::AddEdge(const glm::vec2 &localA, const glm::vec2 &localB)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Edge;
        shape.density = 0.0f;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.edge.SetTwoSided(localA, localB);

        RecomputeMass();
        return 1;
    }

    int Body::AddChain(const glm::vec2 *points, int count, bool loop)
    {
        int minCount = loop ? 3 : 2;
        if (count < minCount)
            return 0;

        int segments = loop ? count : count - 1;
        int added = 0;

        for (int i = 0; i < segments && mShapeCount < kMaxShapes; ++i)
        {
            int i1 = i;
            int i2 = (i + 1) % count;
            glm::vec2 v1 = points[i1];
            glm::vec2 v2 = points[i2];

            // Nas pontas de uma cadeia aberta nao ha vizinho real: o ghost fica igual ao
            // proprio extremo do segmento, o que anula com seguranca o ajuste de
            // convexidade nesse lado (CollideEdgeAndCircle/CollideEdgeAndPolygon tratam
            // um ghost degenerado — normal(0,0) — como "convexo", ou seja, nao rejeitam).
            glm::vec2 vPrev = v1;
            glm::vec2 vNext = v2;
            if (loop || i > 0)
                vPrev = points[(i1 - 1 + count) % count];
            if (loop || i + 2 < count)
                vNext = points[(i2 + 1) % count];

            Shape &shape = mShapes[mShapeCount++];
            shape.type = ShapeType::Edge;
            shape.density = 0.0f;
            shape.filter = mDefaultFilter;
            shape.isSensor = false;
            shape.userData = nullptr;
            shape.edge.vertex1 = v1;
            shape.edge.vertex2 = v2;
            shape.edge.vertex0 = vPrev;
            shape.edge.vertex3 = vNext;
            shape.edge.radius = kPolygonRadius;
            shape.edge.oneSided = true;

            ++added;
        }

        if (added > 0)
            RecomputeMass();
        return added;
    }

    int Body::AddMesh(const glm::vec2 *outline, int count, float density)
    {
        if (count < 3)
            return 0;

        glm::vec2 triangles[kMaxShapes * 3];
        int triCount = Triangulate(outline, count, triangles, kMaxShapes);

        int added = 0;
        for (int i = 0; i < triCount; ++i)
            added += AddPolygon(triangles + i * 3, 3, density);

        return added;
    }

    void Body::RecomputeMass()
    {
        mInvMass = 0.0f;
        mInvI = 0.0f;
        mLocalCenter = glm::vec2(0.0f, 0.0f);

        // NOTA: aqui so se verifica o tipo, nao o estado de sono. AddCircle/AddBox/
        // AddPolygon/AddEdge/AddMesh (API publica, chamavel a qualquer momento) chamam
        // RecomputeMass() diretamente sem acordar o corpo primeiro; excluir corpos
        // adormecidos deixava-os com massa "infinita" (mInvMass=0) ate algo os acordar
        // explicitamente. O Box2D recalcula sempre a massa ao adicionar/remover uma
        // fixture, independentemente do estado de sono — mantemos esse comportamento.
        if (mType != BodyType::Dynamic)
            return;

        float mass = 0.0f;
        glm::vec2 center(0.0f, 0.0f);
        float I = 0.0f;

        for (int i = 0; i < mShapeCount; ++i)
        {
            const Shape &shape = mShapes[i];
            MassData massData;
            switch (shape.type)
            {
            case ShapeType::Circle:
                massData = shape.circle.ComputeMass(shape.density);
                break;
            case ShapeType::Polygon:
                massData = shape.polygon.ComputeMass(shape.density);
                break;
            case ShapeType::Edge:
            default:
                massData = shape.edge.ComputeMass(shape.density);
                break;
            }

            mass += massData.mass;
            center += massData.mass * massData.center;
            I += massData.I;
        }

        if (mass > 0.0f)
        {
            mInvMass = 1.0f / mass;
            center *= mInvMass;
        }
        else
        {
            mass = 1.0f;
            mInvMass = 1.0f;
        }

        if (I > 0.0f)
        {
            I -= mass * Dot(center, center);
            mInvI = I > 0.0f ? 1.0f / I : 0.0f;
        }

        // Port de b2BodyDef.fixedRotation / cpBodySetMoment(INFINITY):
        // rotacao fixa => momento de inercia infinito => mInvI = 0.
        // Nenhum impulso/torque do solver gira o corpo (character).
        if (mFixedRotation)
            mInvI = 0.0f;

        mLocalCenter = center;
    }

    void Body::IntegrateVelocity(const glm::vec2 &gravity, float dt)
    {
        if (mType != BodyType::Dynamic)
            return;

        if (!mAwake)
        {
            // BUG corrigido: esta funcao nao verificava mAwake, pelo que um corpo
            // "adormecido" continuava a acumular gravidade em mLinearVelocity todos os
            // steps (SetAwake(false) so zera a velocidade UMA vez, ao adormecer). Se
            // nada mais o acordasse, a velocidade crescia indefinidamente e escondida
            // — na primeira vez que o corpo fosse acordado (ou colidisse), aplicava-se
            // de repente um "pop" com a velocidade acumulada de todos esses steps.
            // Uma forca em fila enquanto dormia tambem nao deve sobreviver ao despertar.
            mForce = glm::vec2(0.0f);
            mTorque = 0.0f;
            return;
        }

        mLinearVelocity += dt * (mGravityScale * gravity + mInvMass * mForce);
        mAngularVelocity += dt * mInvI * mTorque;

        // Damping implicito (mesma formula do Box2D): estavel para qualquer dt, ao
        // contrario de "v *= (1 - damping*dt)" que pode inverter o sinal com dt grande.
        mLinearVelocity *= 1.0f / (1.0f + dt * mLinearDamping);
        mAngularVelocity *= 1.0f / (1.0f + dt * mAngularDamping);

        mForce = glm::vec2(0.0f);
        mTorque = 0.0f;
    }

    void Body::IntegratePosition(float dt)
    {
        if (mType == BodyType::Static || !mAwake)
            return;

        glm::vec2 translation = dt * mLinearVelocity;
        float translationSq = translation.x * translation.x + translation.y * translation.y;
        if (translationSq > kMaxTranslation * kMaxTranslation)
        {
            float ratio = kMaxTranslation / sqrtf(translationSq);
            mLinearVelocity *= ratio;
            translation *= ratio;
        }

        glm::vec2 center = GetTransform().Transform(mLocalCenter);
        center += translation;
        mAngle += dt * mAngularVelocity;
        Transform xf = MakeTransform(glm::vec2(0.0f, 0.0f), mAngle);
        mPosition = center - xf.Transform(mLocalCenter);
    }

    void Body::ShiftCenter(const glm::vec2 &deltaCenter, float deltaAngle)
    {
        glm::vec2 center = GetTransform().Transform(mLocalCenter);
        center += deltaCenter;
        mAngle += deltaAngle;
        Transform xf = MakeTransform(glm::vec2(0.0f, 0.0f), mAngle);
        mPosition = center - xf.Transform(mLocalCenter);
    }

} // namespace kx
