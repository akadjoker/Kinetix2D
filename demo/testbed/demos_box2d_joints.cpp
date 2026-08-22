#include "testbed.h"

class Box2DDistanceJoint : public Demo
{
public:
    explicit Box2DDistanceJoint(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f, -500.0f)), mSpring(false), mFrequency(5.0f), mDamping(0.5f)
    {
        T().Cam().center = glm::vec2(30.0f, 160.0f);
        T().Cam().zoom = 1.5f;

        kx::Body *ground = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f));
        kx::Body *body = World().CreateCircle(glm::vec2(90.0f, 210.0f), 18.0f, 10.0f);
        mJoint = new kx::DistanceJoint(ground, body, glm::vec2(0.0f, 210.0f), body->Position());
        World().AddJoint(mJoint);
        ApplySettings();
    }

    void UpdateUI() override
    {
        bool changed = ImGui::Checkbox("spring", &mSpring);
        changed = ImGui::SliderFloat("frequency", &mFrequency, 0.0f, 15.0f, "%.1f") || changed;
        changed = ImGui::SliderFloat("damping", &mDamping, 0.0f, 2.0f, "%.2f") || changed;
        if (changed)
            ApplySettings();
    }

private:
    void ApplySettings()
    {
        if (mSpring)
        {
            mJoint->SetLengthRange(30.0f, 180.0f);
            mJoint->SetSpring(mFrequency, mDamping);
        }
        else
        {
            mJoint->SetSpring(0.0f, 0.0f);
            mJoint->SetLengthRange(90.0f, 90.0f);
        }
    }

    kx::DistanceJoint *mJoint;
    bool mSpring;
    float mFrequency;
    float mDamping;
};

static Demo *CreateBox2DDistanceJoint(Testbed &tb) { return new Box2DDistanceJoint(tb); }
static int gDemoBox2DDistanceJoint = TestbedRegisterDemo("Box2D Joints", "Distance Joint", &CreateBox2DDistanceJoint);

class Box2DRevolute : public Demo
{
public:
    explicit Box2DRevolute(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f, -500.0f)), mLimit(true), mMotor(false),
          mMotorSpeed(2.0f), mMaxTorque(2000000.0f)
    {
        T().Cam().center = glm::vec2(0.0f, 130.0f);
        T().Cam().zoom = 1.5f;

        World().CreateEdge(glm::vec2(-500.0f, 0.0f), glm::vec2(500.0f, 0.0f));
        kx::Body *ground = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f));
        kx::Body *arm = World().CreateBox(glm::vec2(0.0f, 150.0f), 15.0f, 80.0f, 1.0f);
        mJoint = new kx::RevoluteJoint(ground, arm, glm::vec2(0.0f, 220.0f));
        World().AddJoint(mJoint);
        ApplySettings();
    }

    void UpdateUI() override
    {
        bool changed = ImGui::Checkbox("limit", &mLimit);
        changed = ImGui::Checkbox("motor", &mMotor) || changed;
        changed = ImGui::SliderFloat("motor speed", &mMotorSpeed, -10.0f, 10.0f, "%.1f") || changed;
        changed = ImGui::SliderFloat("max torque", &mMaxTorque, 1000.0f, 10000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic) || changed;
        if (changed)
            ApplySettings();
    }

private:
    void ApplySettings()
    {
        mJoint->SetLimits(mLimit, -0.25f * kx::kPi, 0.5f * kx::kPi);
        mJoint->SetMotor(mMotor, mMotorSpeed, mMaxTorque);
    }

    kx::RevoluteJoint *mJoint;
    bool mLimit;
    bool mMotor;
    float mMotorSpeed;
    float mMaxTorque;
};

static Demo *CreateBox2DRevolute(Testbed &tb) { return new Box2DRevolute(tb); }
static int gDemoBox2DRevolute = TestbedRegisterDemo("Box2D Joints", "Revolute", &CreateBox2DRevolute);

class Box2DWheel : public Demo
{
public:
    explicit Box2DWheel(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f, -500.0f)), mMotor(true), mFrequency(1.0f),
          mDamping(0.7f), mMotorSpeed(2.0f), mMaxTorque(1000000.0f)
    {
        T().Cam().center = glm::vec2(0.0f, 160.0f);
        T().Cam().zoom = 1.5f;

        kx::Body *ground = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f));
        kx::Body *wheel = World().CreateCircle(glm::vec2(0.0f, 190.0f), 30.0f, 1.0f);
        glm::vec2 axis = glm::normalize(glm::vec2(1.0f, 1.0f));
        mJoint = new kx::WheelJoint(ground, wheel, wheel->Position(), axis, mFrequency, mDamping);
        World().AddJoint(mJoint);
        ApplySettings();
    }

    void UpdateUI() override
    {
        bool changed = ImGui::Checkbox("motor", &mMotor);
        changed = ImGui::SliderFloat("spring frequency", &mFrequency, 0.1f, 10.0f, "%.1f") || changed;
        changed = ImGui::SliderFloat("spring damping", &mDamping, 0.0f, 2.0f, "%.2f") || changed;
        changed = ImGui::SliderFloat("motor speed", &mMotorSpeed, -20.0f, 20.0f, "%.1f") || changed;
        changed = ImGui::SliderFloat("max torque", &mMaxTorque, 1000.0f, 10000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic) || changed;
        if (changed)
            ApplySettings();
    }

private:
    void ApplySettings()
    {
        mJoint->SetSpring(mFrequency, mDamping);
        mJoint->SetMotor(mMotor, mMotorSpeed, mMaxTorque);
    }

    kx::WheelJoint *mJoint;
    bool mMotor;
    float mFrequency;
    float mDamping;
    float mMotorSpeed;
    float mMaxTorque;
};

static Demo *CreateBox2DWheel(Testbed &tb) { return new Box2DWheel(tb); }
static int gDemoBox2DWheel = TestbedRegisterDemo("Box2D Joints", "Wheel", &CreateBox2DWheel);

class Box2DBallAndChain : public Demo
{
public:
    explicit Box2DBallAndChain(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(20.0f, 80.0f);
        T().Cam().zoom = 1.0f;
        T().SetStatus("box links replace reference capsules until Capsule is available");

        const int count = 20;
        const float halfLength = 15.0f;
        const float y = 180.0f;
        kx::Body *previous = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f));

        for (int i = 0; i < count; ++i)
        {
            glm::vec2 position(-320.0f + (1.0f + 2.0f * i) * halfLength, y);
            kx::Body *link = World().CreateBox(position, halfLength, 4.0f, 20.0f);
            link->SetFilter(0x1, 0x2);

            glm::vec2 pivot(-320.0f + 2.0f * i * halfLength, y);
            kx::RevoluteJoint *joint = new kx::RevoluteJoint(previous, link, pivot);
            joint->SetMotor(true, 0.0f, 500000.0f);
            World().AddJoint(joint);
            previous = link;
        }

        glm::vec2 ballPosition(-320.0f + 2.0f * count * halfLength + 70.0f, y);
        kx::Body *ball = World().CreateCircle(ballPosition, 70.0f, 20.0f);
        ball->SetFilter(0x2, 0x1);
        glm::vec2 pivot(-320.0f + 2.0f * count * halfLength, y);
        kx::RevoluteJoint *last = new kx::RevoluteJoint(previous, ball, pivot);
        last->SetMotor(true, 0.0f, 500000.0f);
        World().AddJoint(last);
    }
};

static Demo *CreateBox2DBallAndChain(Testbed &tb) { return new Box2DBallAndChain(tb); }
static int gDemoBox2DBallAndChain = TestbedRegisterDemo("Box2D Joints", "Ball & Chain", &CreateBox2DBallAndChain);

class Box2DBridge : public Demo
{
public:
    explicit Box2DBridge(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 90.0f);
        T().Cam().zoom = 0.85f;
        T().SetStatus("40-link Box2D bridge stress variant");

        const int count = 40;
        const float halfWidth = 12.0f;
        const float y = 140.0f;
        const float xbase = -count * halfWidth;
        kx::Body *ground = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f));
        kx::Body *previous = ground;

        for (int i = 0; i < count; ++i)
        {
            glm::vec2 position(xbase + halfWidth + 2.0f * halfWidth * i, y);
            kx::Body *plank = World().CreateBox(position, halfWidth - 0.5f, 4.0f, 20.0f);
            glm::vec2 pivot(xbase + 2.0f * halfWidth * i, y);
            kx::RevoluteJoint *joint = new kx::RevoluteJoint(previous, plank, pivot);
            joint->SetMotor(true, 0.0f, 100000.0f);
            World().AddJoint(joint);
            previous = plank;
        }

        glm::vec2 lastPivot(xbase + 2.0f * halfWidth * count, y);
        kx::RevoluteJoint *last = new kx::RevoluteJoint(previous, ground, lastPivot);
        last->SetMotor(true, 0.0f, 100000.0f);
        World().AddJoint(last);

        glm::vec2 triangle[3] = {
            glm::vec2(-18.0f, 0.0f), glm::vec2(18.0f, 0.0f), glm::vec2(0.0f, 54.0f)};
        World().CreatePolygon(glm::vec2(-180.0f, 175.0f), triangle, 3, 20.0f);
        World().CreatePolygon(glm::vec2(180.0f, 175.0f), triangle, 3, 20.0f);
        World().CreateCircle(glm::vec2(-240.0f, 230.0f), 18.0f, 20.0f);
        World().CreateCircle(glm::vec2(0.0f, 260.0f), 18.0f, 20.0f);
        World().CreateCircle(glm::vec2(240.0f, 230.0f), 18.0f, 20.0f);
    }
};

static Demo *CreateBox2DBridge(Testbed &tb) { return new Box2DBridge(tb); }
static int gDemoBox2DBridge = TestbedRegisterDemo("Box2D Joints", "Bridge", &CreateBox2DBridge);

class Box2DJointGrid : public Demo
{
public:
    explicit Box2DJointGrid(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -200.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 100.0f);
        T().Cam().zoom = 0.9f;
        T().SetStatus("12x12 revolute-joint grid; reference debug grid is 20x20");

        const int count = 12;
        const float spacing = 28.0f;
        kx::Body *bodies[count * count];

        int index = 0;
        for (int column = 0; column < count; ++column)
        {
            for (int row = 0; row < count; ++row)
            {
                bool fixed = column >= count / 2 - 2 && column <= count / 2 + 2 && row == 0;
                glm::vec2 position((column - 0.5f * (count - 1)) * spacing, 260.0f - row * spacing);
                kx::Body *body;
                if (fixed)
                {
                    body = World().CreateBody(kx::BodyType::Static, position);
                    body->AddCircle(glm::vec2(0.0f), 0.4f * spacing, 0.0f);
                }
                else
                {
                    body = World().CreateCircle(position, 0.4f * spacing, 1.0f);
                }
                body->SetFilter(0x2, 0xFFFD);

                if (row > 0)
                {
                    kx::Body *above = bodies[index - 1];
                    glm::vec2 anchor = 0.5f * (above->Position() + body->Position());
                    World().AddJoint(new kx::RevoluteJoint(above, body, anchor));
                }
                if (column > 0)
                {
                    kx::Body *left = bodies[index - count];
                    glm::vec2 anchor = 0.5f * (left->Position() + body->Position());
                    World().AddJoint(new kx::RevoluteJoint(left, body, anchor));
                }

                bodies[index++] = body;
            }
        }
    }
};

static Demo *CreateBox2DJointGrid(Testbed &tb) { return new Box2DJointGrid(tb); }
static int gDemoBox2DJointGrid = TestbedRegisterDemo("Box2D Joints", "Joint Grid", &CreateBox2DJointGrid);

class Box2DFallingHinges : public Demo
{
public:
    explicit Box2DFallingHinges(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 170.0f);
        T().Cam().zoom = 1.0f;
        T().SetStatus("joint-limit determinism scene; spring branch deferred");

        World().CreateStaticBox(glm::vec2(0.0f, -20.0f), 600.0f, 20.0f);

        const int columns = 4;
        const int rows = 20;
        const float halfSize = 12.0f;
        const float dx = 120.0f;
        const float xbase = -0.5f * dx * (columns - 1);

        for (int column = 0; column < columns; ++column)
        {
            kx::Body *previous = nullptr;
            for (int row = 0; row < rows; ++row)
            {
                float angle = row % 2 == 0 ? -0.1f : 0.1f;
                glm::vec2 position(xbase + column * dx + 0.4f * halfSize * row,
                                   halfSize + 2.0f * halfSize * row);
                kx::Body *body = World().CreateBody(kx::BodyType::Dynamic, position, angle);
                body->AddBox(halfSize, halfSize, glm::vec2(0.0f), 1.0f);

                if (row % 2 == 0)
                {
                    previous = body;
                }
                else
                {
                    glm::vec2 anchor = previous->GetTransform().Transform(glm::vec2(-halfSize, halfSize));
                    kx::RevoluteJoint *joint = new kx::RevoluteJoint(previous, body, anchor);
                    joint->SetLimits(true, -0.1f * kx::kPi, 0.2f * kx::kPi);
                    joint->SetMotor(true, 0.0f, 25000.0f);
                    World().AddJoint(joint);
                    previous = nullptr;
                }
            }
        }
    }
};

static Demo *CreateBox2DFallingHinges(Testbed &tb) { return new Box2DFallingHinges(tb); }
static int gDemoBox2DFallingHinges = TestbedRegisterDemo("Box2D Joints", "Falling Hinges", &CreateBox2DFallingHinges);

class Box2DMotorJoint : public Demo
{
public:
    explicit Box2DMotorJoint(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f)), mBody(nullptr), mJoint(nullptr),
          mTarget(0.0f, 180.0f), mTime(0.0f), mSpeed(0.5f),
          mMaxForce(100000000.0f), mMaxTorque(10000000000.0f)
    {
        T().Cam().center = glm::vec2(0.0f, 150.0f);
        T().Cam().zoom = 1.2f;
        T().SetStatus("MotorJoint follows a moving linear and angular offset");

        kx::Body *ground = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f));
        mBody = World().CreateBox(mTarget, 60.0f, 15.0f, 1.0f);
        mJoint = new kx::MotorJoint(ground, mBody);
        mJoint->SetMaxForce(mMaxForce);
        mJoint->SetMaxTorque(mMaxTorque);
        World().AddJoint(mJoint);
    }

    void Step(float dt) override
    {
        mTime += mSpeed * dt;
        mTarget.x = 180.0f * sinf(2.0f * mTime);
        mTarget.y = 180.0f + 100.0f * sinf(mTime);
        mJoint->SetLinearOffset(mTarget);
        mJoint->SetAngularOffset(2.0f * mTime);
    }

    void UpdateUI() override
    {
        ImGui::SliderFloat("speed", &mSpeed, -2.0f, 2.0f, "%.2f");
        if (ImGui::SliderFloat("max force", &mMaxForce, 1000.0f, 1000000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic))
            mJoint->SetMaxForce(mMaxForce);
        if (ImGui::SliderFloat("max torque", &mMaxTorque, 1000.0f, 100000000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic))
            mJoint->SetMaxTorque(mMaxTorque);
        if (ImGui::Button("apply impulse"))
            mBody->ApplyImpulse(glm::vec2(500.0f * mBody->Mass(), 0.0f), mBody->WorldCenter());
    }

    void Draw() override
    {
        T().Batch().SetColor(1.0f, 0.8f, 0.2f, 1.0f);
        T().Batch().DrawCircle(mTarget.x, mTarget.y, 6.0f, 12);
    }

private:
    kx::Body *mBody;
    kx::MotorJoint *mJoint;
    glm::vec2 mTarget;
    float mTime;
    float mSpeed;
    float mMaxForce;
    float mMaxTorque;
};

static Demo *CreateBox2DMotorJoint(Testbed &tb) { return new Box2DMotorJoint(tb); }
static int gDemoBox2DMotorJoint = TestbedRegisterDemo("Box2D Joints", "Motor Joint", &CreateBox2DMotorJoint);
