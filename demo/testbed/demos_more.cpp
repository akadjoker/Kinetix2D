#include "testbed.h"

#include <cmath>

static const float kPi = 3.14159265f;
static const int SCANCODE_UP = 82;
static const int SCANCODE_DOWN = 81;
static const int SCANCODE_RIGHT = 79;
static const int SCANCODE_LEFT = 80;

static float Clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

class BouncyHexagons : public Demo
{
public:
    explicit BouncyHexagons(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -100.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 0.0f);
        T().SetStatus("SPACE spawns bouncy hexagons");

        kx::Body *floor = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(320.0f, -240.0f));
        floor->SetRestitution(1.0f);
        floor->SetFriction(1.0f);

        for (int i = 0; i < 6; ++i)
        {
            float a = 2.0f * kPi * i / 6.0f;
            mHex[i] = glm::vec2(18.0f * cosf(a), 18.0f * sinf(a));
        }

        for (int i = 0; i < 10; ++i)
            Spawn();
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        if (input.KeyPressed(SCANCODE_SPACE))
            Spawn();
    }

private:
    void Spawn()
    {
        kx::Body *h = World().CreatePolygon(glm::vec2(Rnd() * 560.0f - 280.0f, 220.0f), mHex, 6, 1.0f);
        h->SetRestitution(1.0f);
        h->SetFriction(0.8f);
    }

    glm::vec2 mHex[6];
};

static Demo *CreateBouncyHexagons(Testbed &tb) { return new BouncyHexagons(tb); }
static int gDemoBouncyHexagons = TestbedRegisterDemo("Basic", "Bouncy Hexagons", &CreateBouncyHexagons);

class PyramidTopple : public Demo
{
public:
    explicit PyramidTopple(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -300.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 20.0f);
        T().SetStatus("grab the dominoes with the mouse to topple the pyramid");

        World().SetVelocityIterations(12);

        kx::Body *floor = World().CreateEdge(glm::vec2(-600.0f, -240.0f), glm::vec2(600.0f, -240.0f));
        floor->SetRestitution(1.0f);
        floor->SetFriction(1.0f);

        const int n = 12;
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n - i; ++j)
            {
                glm::vec2 offset((j - (n - 1 - i) * 0.5f) * 1.5f * kHeight,
                                 (i + 0.5f) * (kHeight + 2.0f * kWidth) - kWidth - 240.0f);
                AddDomino(offset, false);
                AddDomino(offset + glm::vec2(0.0f, (kHeight + kWidth) * 0.5f), true);

                if (j == 0)
                    AddDomino(offset + glm::vec2(0.5f * (kWidth - kHeight), kHeight + kWidth), false);

                if (j != n - i - 1)
                    AddDomino(offset + glm::vec2(kHeight * 0.75f, (kHeight + 3.0f * kWidth) * 0.5f), true);
                else
                    AddDomino(offset + glm::vec2(0.5f * (kHeight - kWidth), kHeight + kWidth), false);
            }
        }
    }

private:
    void AddDomino(const glm::vec2 &pos, bool flipped)
    {
        kx::Body *body = World().CreateBody(kx::BodyType::Dynamic, pos);
        if (flipped)
            body->AddBox(kHeight * 0.5f, kWidth * 0.5f, glm::vec2(0.0f, 0.0f), 1.0f);
        else
            body->AddBox(kWidth * 0.5f - kRadius, kHeight * 0.5f, glm::vec2(0.0f, 0.0f), 1.0f);
        body->SetFriction(0.6f);
    }

    static const float kWidth;
    static const float kHeight;
    static const float kRadius;
};

const float PyramidTopple::kWidth = 5.0f;
const float PyramidTopple::kHeight = 36.0f;
const float PyramidTopple::kRadius = 0.75f;

static Demo *CreatePyramidTopple(Testbed &tb) { return new PyramidTopple(tb); }
static int gDemoPyramidTopple = TestbedRegisterDemo("Basic", "Pyramid Topple", &CreatePyramidTopple);

class Convex : public Demo
{
public:
    explicit Convex(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 0.0f);
        T().SetStatus("right click adds a vertex, the convex hull is rebuilt; SPACE clears");

        World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(320.0f, -240.0f));
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        if (input.MousePressed(1) && !T().Device().ImGuiWantsMouse())
        {
            mPoints.push_back(T().MouseWorld());
            Rebuild();
        }
        if (input.KeyPressed(SCANCODE_SPACE))
        {
            mPoints.clear();
            Rebuild();
        }
    }

    void Draw() override
    {
        T().Batch().SetColor((unsigned char)255, (unsigned char)200, (unsigned char)100, (unsigned char)255);
        for (size_t i = 0; i < mPoints.size(); ++i)
            T().Batch().DrawCircle(mPoints[i].x, mPoints[i].y, 3.0f, 8);
    }

private:
    void Rebuild()
    {
        if (mHull)
        {
            World().Destroy(mHull);
            mHull = nullptr;
        }
        if (mPoints.size() >= 3)
            mHull = World().CreatePolygon(glm::vec2(0.0f, 0.0f), mPoints.data(), (int)mPoints.size(), 1.0f);
    }

    ct::Vector<glm::vec2> mPoints;
    kx::Body *mHull;
};

static Demo *CreateConvex(Testbed &tb) { return new Convex(tb); }
static int gDemoConvex = TestbedRegisterDemo("Basic", "Convex", &CreateConvex);

class TheoJansen : public Demo
{
public:
    explicit TheoJansen(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, -20.0f);
        T().SetStatus("arrow keys drive the walking machine");

        World().SetVelocityIterations(20);

        kx::Body *left = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(-320.0f, 240.0f));
        left->SetRestitution(1.0f);
        left->SetFriction(1.0f);
        kx::Body *right = World().CreateEdge(glm::vec2(320.0f, -240.0f), glm::vec2(320.0f, 240.0f));
        right->SetRestitution(1.0f);
        right->SetFriction(1.0f);
        kx::Body *floor = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(320.0f, -240.0f));
        floor->SetRestitution(1.0f);
        floor->SetFriction(1.0f);

        const float offset = 30.0f;
        const float side = 30.0f;
        const float crankRadius = 13.0f;

        mChassis = World().CreateBody(kx::BodyType::Dynamic, glm::vec2(0.0f, 0.0f));
        mChassis->AddBox(offset, mSegRadius, glm::vec2(0.0f, 0.0f), 1.0f);

        mCrank = World().CreateCircle(glm::vec2(0.0f, 0.0f), crankRadius, 1.0f);

        mMotor = new kx::RevoluteJoint(mChassis, mCrank, glm::vec2(0.0f, 0.0f));
        World().AddJoint(mMotor);

        const int offsets[4] = {30, -30, 30, -30};
        const glm::vec2 anchors[4] = {
            glm::vec2(crankRadius, 0.0f),
            glm::vec2(0.0f, crankRadius),
            glm::vec2(-crankRadius, 0.0f),
            glm::vec2(0.0f, -crankRadius)};

        for (int i = 0; i < 4; ++i)
            MakeLeg(side, (float)offsets[i], anchors[i]);
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        float x = 0.0f;
        if (input.KeyDown(SCANCODE_RIGHT))
            x += 1.0f;
        if (input.KeyDown(SCANCODE_LEFT))
            x -= 1.0f;
        float y = 0.0f;
        if (input.KeyDown(SCANCODE_UP))
            y += 1.0f;
        if (input.KeyDown(SCANCODE_DOWN))
            y -= 1.0f;

        float coef = (2.0f + y) / 3.0f;
        float rate = x * 10.0f * coef;
        mMotor->SetMotor(rate != 0.0f, rate, rate != 0.0f ? 100000.0f : 0.0f);
    }

private:
    void MakeLeg(float side, float offset, const glm::vec2 &anchor)
    {
        glm::vec2 pin(offset, 0.0f);

        kx::Body *upper = World().CreateBody(kx::BodyType::Dynamic, pin);
        upper->AddBox(mSegRadius, side * 0.5f, glm::vec2(0.0f, side * 0.5f), 1.0f);

        kx::Body *lower = World().CreateBody(kx::BodyType::Dynamic, pin);
        lower->AddBox(mSegRadius, side * 0.5f, glm::vec2(0.0f, -side * 0.5f), 1.0f);
        lower->AddCircle(glm::vec2(0.0f, -side), mSegRadius * 2.0f, 1.0f);
        lower->SetFriction(1.0f);

        kx::RevoluteJoint *upperPivot = new kx::RevoluteJoint(mChassis, upper, pin);
        World().AddJoint(upperPivot);
        kx::RevoluteJoint *lowerPivot = new kx::RevoluteJoint(mChassis, lower, pin);
        World().AddJoint(lowerPivot);

        kx::GearJoint *gear = new kx::GearJoint(upperPivot, lowerPivot, 1.0f);
        World().AddJoint(gear);

        kx::DistanceJoint *rodUpper = new kx::DistanceJoint(mCrank, upper, anchor, glm::vec2(offset, side));
        World().AddJoint(rodUpper);
        kx::DistanceJoint *rodLower = new kx::DistanceJoint(mCrank, lower, anchor, glm::vec2(offset, 0.0f));
        World().AddJoint(rodLower);
    }

    static const float mSegRadius;
    kx::Body *mChassis;
    kx::Body *mCrank;
    kx::RevoluteJoint *mMotor;
};

const float TheoJansen::mSegRadius = 3.0f;

static Demo *CreateTheoJansen(Testbed &tb) { return new TheoJansen(tb); }
static int gDemoTheoJansen = TestbedRegisterDemo("Joints", "Theo Jansen", &CreateTheoJansen);

class Tank : public Demo
{
public:
    explicit Tank(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 60.0f);
        T().Cam().zoom = 1.2f;
        T().SetStatus("the tank chases the mouse; SPACE spawns boxes to push");

        World().CreateStaticBox(glm::vec2(0.0f, -60.0f), 520.0f, 10.0f);

        mHull = World().CreateBody(kx::BodyType::Dynamic, glm::vec2(0.0f, 0.0f));
        mHull->AddBox(42.0f, 22.0f, glm::vec2(0.0f, 0.0f), 1.0f);
        mHull->AddBox(16.0f, 8.0f, glm::vec2(0.0f, 28.0f), 0.5f);
        mHull->SetFriction(0.8f);

        const float x = 30.0f;
        const float y = -14.0f;
        const float radius = 16.0f;
        const glm::vec2 wheelPos[4] = {
            glm::vec2(-x, y), glm::vec2(x, y),
            glm::vec2(-x, -y), glm::vec2(x, -y)};

        for (int i = 0; i < 4; ++i)
        {
            kx::Body *wheel = World().CreateCircle(wheelPos[i], radius, 1.0f);
            wheel->SetFriction(1.0f);
            mWheels[i] = new kx::WheelJoint(mHull, wheel, wheelPos[i], glm::vec2(0.0f, 1.0f), 4.0f, 0.7f);
            World().AddJoint(mWheels[i]);
        }

        for (int i = 0; i < 6; ++i)
            SpawnBox();
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        if (input.KeyPressed(SCANCODE_SPACE))
            SpawnBox();

        glm::vec2 mouse = T().MouseWorld();
        glm::vec2 toMouse = mouse - mHull->Position();
        float targetAngle = atan2f(toMouse.y, toMouse.x);
        float diff = targetAngle - mHull->Angle();
        while (diff > kPi)
            diff -= 2.0f * kPi;
        while (diff < -kPi)
            diff += 2.0f * kPi;

        float turn = Clampf(diff * 5.0f, -4.0f, 4.0f);
        float drive = -12.0f;

        for (int i = 0; i < 2; ++i)
            mWheels[i]->SetMotor(true, drive + turn, mMaxTorque);
        for (int i = 2; i < 4; ++i)
            mWheels[i]->SetMotor(true, drive - turn, mMaxTorque);
    }

    void UpdateUI() override
    {
        ImGui::SliderFloat("maxTorque", &mMaxTorque, 1.0e5f, 2.0e7f, "%.0f", ImGuiSliderFlags_Logarithmic);
    }

private:
    void SpawnBox()
    {
        kx::Body *box = World().CreateBox(glm::vec2(Rnd() * 400.0f - 200.0f, 180.0f), 12.0f, 12.0f, 1.0f);
        box->SetFriction(0.5f);
    }

    kx::Body *mHull;
    kx::WheelJoint *mWheels[4];
    float mMaxTorque = 2000000.0f;
};

static Demo *CreateTank(Testbed &tb) { return new Tank(tb); }
static int gDemoTank = TestbedRegisterDemo("Joints", "Tank", &CreateTank);