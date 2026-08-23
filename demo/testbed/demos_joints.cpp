#include "testbed.h"

#include <cstdio>

static const int SCANCODE_RIGHT = 79;
static const int SCANCODE_LEFT = 80;

class Bridge : public Demo
{
public:
    explicit Bridge(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 60.0f);

        const int kPlanks = 14;
        const float kSpan = 700.0f;
        const float kPlankHalfHeight = 6.0f;
        const float kPlankWidth = kSpan / (float)kPlanks;
        const float kPlankHalfWidth = kPlankWidth * 0.5f - 1.0f;

        float leftX = -kSpan * 0.5f;
        float y = 100.0f;

        kx::Body *leftPillar = World().CreateStaticBox(glm::vec2(leftX, y + 60.0f), 10.0f, 35.0f);
        kx::Body *rightPillar = World().CreateStaticBox(glm::vec2(leftX + kSpan, y + 60.0f), 10.0f, 35.0f);

        kx::Body *prev = leftPillar;
        glm::vec2 anchor(leftX, y);

        for (int i = 0; i < kPlanks; ++i)
        {
            float cx = leftX + kPlankWidth * ((float)i + 0.5f);
            kx::Body *plank = World().CreateBox(glm::vec2(cx, y), kPlankHalfWidth, kPlankHalfHeight, 1.0f);

            kx::RevoluteJoint *joint = new kx::RevoluteJoint(prev, plank, anchor);
            World().AddJoint(joint);

            prev = plank;
            anchor = glm::vec2(cx + kPlankHalfWidth + 1.0f, y);
        }

        kx::RevoluteJoint *last = new kx::RevoluteJoint(prev, rightPillar, anchor);
        World().AddJoint(last);

        DropWeight(0.0f);
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        if (input.KeyPressed(SCANCODE_SPACE))
            DropWeight((Rnd() - 0.5f) * 400.0f);
    }

private:
    void DropWeight(float x)
    {
        kx::Body *weight = World().CreateBox(glm::vec2(x, 260.0f), 40.0f, 40.0f, 5.0f);
        weight->SetFriction(0.6f);
    }
};

static Demo *CreateBridge(Testbed &tb) { return new Bridge(tb); }
static int gDemoBridge = TestbedRegisterDemo("Joints", "Bridge", &CreateBridge);

class Gears : public Demo
{
public:
    explicit Gears(Testbed &tb) : Demo(tb, glm::vec2(0.0f, 0.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 0.0f);
        T().SetStatus("gear ratio 2:1  -  the small disc spins twice as fast, counter-rotating");

        glm::vec2 posA(-50.0f, 0.0f);
        glm::vec2 posB(50.0f, 0.0f);

        kx::Body *anchorA = World().CreateBody(kx::BodyType::Static, posA);
        kx::Body *discA = World().CreateCircle(posA, 40.0f, 1.0f);
        kx::RevoluteJoint *jointA = new kx::RevoluteJoint(anchorA, discA, posA);
        jointA->SetMotor(true, 3.0f, 1.0e7f);
        World().AddJoint(jointA);

        kx::Body *anchorB = World().CreateBody(kx::BodyType::Static, posB);
        kx::Body *discB = World().CreateCircle(posB, 40.0f, 1.0f);
        kx::RevoluteJoint *jointB = new kx::RevoluteJoint(anchorB, discB, posB);
        World().AddJoint(jointB);

        kx::GearJoint *gear = new kx::GearJoint(jointA, jointB, 2.0f);
        World().AddJoint(gear);
    }
};

static Demo *CreateGears(Testbed &tb) { return new Gears(tb); }
static int gDemoGears = TestbedRegisterDemo("Joints", "Gears", &CreateGears);

class Blob : public Demo
{
public:
    explicit Blob(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 80.0f);
        World().CreateStaticBox(glm::vec2(0.0f, -20.0f), 420.0f, 20.0f);

        const int kRing = 12;
        const float kRingRadius = 30.0f;
        const float kNodeRadius = 7.0f;
        const float kCenterRadius = 10.0f;
        const int16_t kBlobGroup = -1;
        const float kSpringFrequency = 4.0f;
        const float kSpringDamping = 0.5f;

        glm::vec2 dropCenter(0.0f, 200.0f);

        kx::Body *center = World().CreateCircle(dropCenter, kCenterRadius, 1.0f);
        center->SetFilter(1, 0xFFFF, kBlobGroup);

        kx::Body *nodes[kRing];
        for (int i = 0; i < kRing; ++i)
        {
            float angle = (float)i * (6.28318531f / (float)kRing);
            glm::vec2 pos = dropCenter + glm::vec2(kRingRadius * cosf(angle), kRingRadius * sinf(angle));
            nodes[i] = World().CreateCircle(pos, kNodeRadius, 1.0f);
            nodes[i]->SetFilter(1, 0xFFFF, kBlobGroup);
        }

        for (int i = 0; i < kRing; ++i)
        {
            int next = (i + 1) % kRing;

            kx::DistanceJoint *ringJoint = new kx::DistanceJoint(nodes[i], nodes[next],
                                                                 nodes[i]->Position(), nodes[next]->Position());
            ringJoint->SetSpring(kSpringFrequency, kSpringDamping);
            ringJoint->SetLengthRange(0.0f, 1000.0f);
            World().AddJoint(ringJoint);

            kx::DistanceJoint *spokeJoint = new kx::DistanceJoint(center, nodes[i],
                                                                  center->Position(), nodes[i]->Position());
            spokeJoint->SetSpring(kSpringFrequency, kSpringDamping);
            spokeJoint->SetLengthRange(0.0f, 1000.0f);
            World().AddJoint(spokeJoint);
        }
    }
};

static Demo *CreateBlob(Testbed &tb) { return new Blob(tb); }
static int gDemoBlob = TestbedRegisterDemo("Joints", "Blob", &CreateBlob);

class Car : public Demo
{
public:
    explicit Car(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 40.0f);
        T().Cam().zoom = 1.2f;
        T().SetStatus("Left/Right arrow keys to drive");

        World().CreateStaticBox(glm::vec2(0.0f, -60.0f), 520.0f, 10.0f);

        const float kScale = 50.0f;

        glm::vec2 chassisOutline[6] = {
            glm::vec2(-1.5f * kScale, -0.5f * kScale),
            glm::vec2(1.5f * kScale, -0.5f * kScale),
            glm::vec2(1.5f * kScale, 0.0f * kScale),
            glm::vec2(0.0f * kScale, 0.9f * kScale),
            glm::vec2(-1.15f * kScale, 0.9f * kScale),
            glm::vec2(-1.5f * kScale, 0.2f * kScale)};

        mChassis = World().CreatePolygon(glm::vec2(0.0f, 0.0f), chassisOutline, 6, 1.0f);

        const float wheelRadius = 0.4f * kScale;
        glm::vec2 rearLocal(-1.0f * kScale, -0.5f * kScale);
        glm::vec2 frontLocal(1.0f * kScale, -0.5f * kScale);

        kx::Body *wheelRear = World().CreateCircle(rearLocal, wheelRadius, 1.0f);
        kx::Body *wheelFront = World().CreateCircle(frontLocal, wheelRadius, 1.0f);

        mJointRear = new kx::WheelJoint(mChassis, wheelRear, rearLocal, glm::vec2(0.0f, 1.0f), 4.0f, 0.7f);
        mJointFront = new kx::WheelJoint(mChassis, wheelFront, frontLocal, glm::vec2(0.0f, 1.0f), 4.0f, 0.7f);
        World().AddJoint(mJointRear);
        World().AddJoint(mJointFront);
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        float motorSpeed = 0.0f;
        if (input.KeyDown(SCANCODE_RIGHT))
            motorSpeed = -15.0f;
        else if (input.KeyDown(SCANCODE_LEFT))
            motorSpeed = 15.0f;

        mJointRear->SetMotor(true, motorSpeed, mMaxTorque);
        mJointFront->SetMotor(true, motorSpeed, mMaxTorque);
    }

    void UpdateUI() override
    {
        ImGui::SliderFloat("maxTorque", &mMaxTorque, 1.0e5f, 2.0e7f, "%.0f", ImGuiSliderFlags_Logarithmic);
    }

private:
    kx::Body *mChassis;
    kx::WheelJoint *mJointRear;
    kx::WheelJoint *mJointFront;
    float mMaxTorque = 5000000.0f;
};

static Demo *CreateCar(Testbed &tb) { return new Car(tb); }
static int gDemoCar = TestbedRegisterDemo("Joints", "Car", &CreateCar);

class Pendulum : public Demo
{
public:
    explicit Pendulum(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 60.0f);
        T().SetStatus("a mass on a rigid arm swinging about a fixed static base");

        glm::vec2 pin(0.0f, 190.0f);

        kx::Body *base = World().CreateStaticBox(glm::vec2(0.0f, 215.0f), 60.0f, 10.0f);

        kx::Body *rod = World().CreateBody(kx::BodyType::Dynamic, glm::vec2(0.0f, 100.0f));
        rod->AddBox(8.0f, 90.0f, glm::vec2(0.0f, 0.0f), 1.0f);

        kx::RevoluteJoint *joint = new kx::RevoluteJoint(base, rod, pin);
        World().AddJoint(joint);

        kx::Body *bob = World().CreateCircle(glm::vec2(0.0f, -30.0f), 28.0f, 2.0f);
        kx::DistanceJoint *tie = new kx::DistanceJoint(rod, bob,
                                                       glm::vec2(0.0f, 10.0f), glm::vec2(0.0f, -30.0f));
        World().AddJoint(tie);

        rod->SetAngularVelocity(2.0f);
        bob->SetAngularVelocity(2.0f);
    }
};

static Demo *CreatePendulum(Testbed &tb) { return new Pendulum(tb); }
static int gDemoPendulum = TestbedRegisterDemo("Joints", "Pendulum", &CreatePendulum);

class Chain : public Demo
{
public:
    explicit Chain(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -100.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 60.0f);

        kx::Body *left = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(-320.0f, 240.0f));
        left->SetRestitution(1.0f);
        kx::Body *right = World().CreateEdge(glm::vec2(320.0f, -240.0f), glm::vec2(320.0f, 240.0f));
        right->SetRestitution(1.0f);
        kx::Body *floor = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(320.0f, -240.0f));
        floor->SetRestitution(1.0f);

        const int kChains = 5;
        const int kLinks = 10;
        const float kWidth = 20.0f;
        const float kHeight = 30.0f;
        const float kSpacing = kWidth * 0.3f;

        for (int i = 0; i < kChains; ++i)
        {
            float x = 40.0f * (i - (kChains - 1) / 2.0f);
            kx::Body *prev = World().CreateBody(kx::BodyType::Static, glm::vec2(x, 240.0f));

            for (int j = 0; j < kLinks; ++j)
            {
                glm::vec2 pos(x, 240.0f - (j + 0.5f) * kHeight - (j + 1.0f) * kSpacing);
                kx::Body *link = World().CreateBody(kx::BodyType::Dynamic, pos);
                link->AddBox(kWidth * 0.5f, kHeight * 0.5f, glm::vec2(0.0f, 0.0f), 1.0f);
                link->SetFriction(0.8f);

                kx::RevoluteJoint *joint = new kx::RevoluteJoint(prev, link, glm::vec2(pos.x, pos.y + kHeight * 0.5f));
                joint->SetCollideConnected(false);
                World().AddJoint(joint);

                prev = link;
            }
        }

        kx::Body *ball = World().CreateCircle(glm::vec2(0.0f, 160.0f), 15.0f, 1.0f);
        ball->SetFriction(0.9f);
    }
};

static Demo *CreateChain(Testbed &tb) { return new Chain(tb); }
static int gDemoChain = TestbedRegisterDemo("Joints", "Chain", &CreateChain);

class Springies : public Demo
{
public:
    explicit Springies(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -100.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 0.0f);
        T().SetStatus("a network of spring distance joints");

        mStaticBody = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f, 0.0f));

        mBar1 = AddBar(glm::vec2(-240.0f, 160.0f), glm::vec2(-160.0f, 80.0f));
        mBar2 = AddBar(glm::vec2(-160.0f, 80.0f), glm::vec2(-80.0f, 160.0f));
        mBar3 = AddBar(glm::vec2(0.0f, 160.0f), glm::vec2(80.0f, 0.0f));
        mBar4 = AddBar(glm::vec2(160.0f, 160.0f), glm::vec2(240.0f, 160.0f));
        mBar5 = AddBar(glm::vec2(-240.0f, 0.0f), glm::vec2(-160.0f, -80.0f));
        mBar6 = AddBar(glm::vec2(-160.0f, -80.0f), glm::vec2(-80.0f, 0.0f));
        mBar7 = AddBar(glm::vec2(-80.0f, 0.0f), glm::vec2(0.0f, 0.0f));
        mBar8 = AddBar(glm::vec2(0.0f, -80.0f), glm::vec2(80.0f, -80.0f));
        mBar9 = AddBar(glm::vec2(240.0f, 80.0f), glm::vec2(160.0f, 0.0f));
        mBar10 = AddBar(glm::vec2(160.0f, 0.0f), glm::vec2(240.0f, -80.0f));
        mBar11 = AddBar(glm::vec2(-240.0f, -80.0f), glm::vec2(-160.0f, -160.0f));
        mBar12 = AddBar(glm::vec2(-160.0f, -160.0f), glm::vec2(-80.0f, -160.0f));
        mBar13 = AddBar(glm::vec2(0.0f, -160.0f), glm::vec2(80.0f, -160.0f));
        mBar14 = AddBar(glm::vec2(160.0f, -160.0f), glm::vec2(240.0f, -160.0f));

        Pivot(mBar1, mBar2, glm::vec2(-160.0f, 80.0f));
        Pivot(mBar5, mBar6, glm::vec2(-160.0f, -80.0f));
        Pivot(mBar6, mBar7, glm::vec2(-80.0f, 0.0f));
        Pivot(mBar9, mBar10, glm::vec2(160.0f, 0.0f));
        Pivot(mBar11, mBar12, glm::vec2(-160.0f, -160.0f));

        const float kFreq = 3.0f;
        const float kDamp = 0.15f;

        Spring(mStaticBody, mBar1, glm::vec2(-320.0f, 240.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar1, glm::vec2(-320.0f, 80.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar1, glm::vec2(-160.0f, 240.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar2, glm::vec2(-160.0f, 240.0f), glm::vec2(40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar2, glm::vec2(0.0f, 240.0f), glm::vec2(40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar3, glm::vec2(80.0f, 240.0f), glm::vec2(-40.0f, 80.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar4, glm::vec2(80.0f, 240.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar4, glm::vec2(320.0f, 240.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar5, glm::vec2(-320.0f, 80.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar9, glm::vec2(320.0f, 80.0f), glm::vec2(40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar10, glm::vec2(320.0f, 0.0f), glm::vec2(40.0f, -40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar10, glm::vec2(320.0f, -160.0f), glm::vec2(40.0f, -40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar11, glm::vec2(-320.0f, -160.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar12, glm::vec2(-240.0f, -240.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar12, glm::vec2(0.0f, -240.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar13, glm::vec2(0.0f, -240.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar13, glm::vec2(80.0f, -240.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar14, glm::vec2(80.0f, -240.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar14, glm::vec2(240.0f, -240.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mStaticBody, mBar14, glm::vec2(320.0f, -160.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);

        Spring(mBar1, mBar5, glm::vec2(40.0f, -40.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mBar1, mBar6, glm::vec2(40.0f, -40.0f), glm::vec2(40.0f, 40.0f), kFreq, kDamp);
        Spring(mBar2, mBar3, glm::vec2(40.0f, 40.0f), glm::vec2(-40.0f, 80.0f), kFreq, kDamp);
        Spring(mBar3, mBar4, glm::vec2(-40.0f, 80.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar3, mBar4, glm::vec2(40.0f, -80.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar3, mBar7, glm::vec2(40.0f, -80.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar3, mBar7, glm::vec2(-40.0f, 80.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar3, mBar8, glm::vec2(40.0f, -80.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar3, mBar9, glm::vec2(40.0f, -80.0f), glm::vec2(-40.0f, -40.0f), kFreq, kDamp);
        Spring(mBar4, mBar9, glm::vec2(40.0f, 0.0f), glm::vec2(40.0f, 40.0f), kFreq, kDamp);
        Spring(mBar5, mBar11, glm::vec2(-40.0f, 40.0f), glm::vec2(-40.0f, 40.0f), kFreq, kDamp);
        Spring(mBar5, mBar11, glm::vec2(40.0f, -40.0f), glm::vec2(40.0f, -40.0f), kFreq, kDamp);
        Spring(mBar7, mBar8, glm::vec2(40.0f, 0.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar8, mBar12, glm::vec2(-40.0f, 0.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar8, mBar13, glm::vec2(-40.0f, 0.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar8, mBar13, glm::vec2(40.0f, 0.0f), glm::vec2(40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar8, mBar14, glm::vec2(40.0f, 0.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar10, mBar14, glm::vec2(40.0f, -40.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);
        Spring(mBar10, mBar14, glm::vec2(40.0f, -40.0f), glm::vec2(-40.0f, 0.0f), kFreq, kDamp);

        mBar3->SetAngularVelocity(1.5f);
    }

private:
    kx::Body *AddBar(const glm::vec2 &a, const glm::vec2 &b)
    {
        glm::vec2 delta = b - a;
        glm::vec2 center = (a + b) * 0.5f;
        float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
        float angle = atan2f(delta.y, delta.x);

        kx::Body *body = World().CreateBody(kx::BodyType::Dynamic, center, angle);
        body->AddBox(length * 0.5f, 10.0f, glm::vec2(0.0f, 0.0f), 1.0f);
        body->SetFilter(1, 0xFFFF, (int16_t)-5);
        return body;
    }

    void Pivot(kx::Body *a, kx::Body *b, const glm::vec2 &worldAnchor)
    {
        kx::RevoluteJoint *joint = new kx::RevoluteJoint(a, b, worldAnchor);
        World().AddJoint(joint);
    }

    void Spring(kx::Body *a, kx::Body *b, const glm::vec2 &anchorA, const glm::vec2 &anchorB,
                float frequencyHz, float dampingRatio)
    {
        glm::vec2 worldA = a->GetTransform().Transform(anchorA);
        glm::vec2 worldB = b->GetTransform().Transform(anchorB);

        kx::DistanceJoint *joint = new kx::DistanceJoint(a, b, worldA, worldB);
        joint->SetSpring(frequencyHz, dampingRatio);
        joint->SetLengthRange(0.0f, 1000.0f);
        World().AddJoint(joint);
    }

    kx::Body *mStaticBody;
    kx::Body *mBar1;
    kx::Body *mBar2;
    kx::Body *mBar3;
    kx::Body *mBar4;
    kx::Body *mBar5;
    kx::Body *mBar6;
    kx::Body *mBar7;
    kx::Body *mBar8;
    kx::Body *mBar9;
    kx::Body *mBar10;
    kx::Body *mBar11;
    kx::Body *mBar12;
    kx::Body *mBar13;
    kx::Body *mBar14;
};

static Demo *CreateSpringies(Testbed &tb) { return new Springies(tb); }
static int gDemoSpringies = TestbedRegisterDemo("Joints", "Springies", &CreateSpringies);

class Pump : public Demo
{
public:
    explicit Pump(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -600.0f))
    {
        T().Cam().center = glm::vec2(0.0f, -20.0f);
        T().Cam().zoom = 1.4f;
        T().SetStatus("arrow keys to drive the machine");

        World().CreateEdge(glm::vec2(-256.0f, 16.0f), glm::vec2(-256.0f, 300.0f));
        World().CreateEdge(glm::vec2(-256.0f, 16.0f), glm::vec2(-192.0f, 0.0f));
        World().CreateEdge(glm::vec2(-192.0f, 0.0f), glm::vec2(-192.0f, -64.0f));
        World().CreateEdge(glm::vec2(-128.0f, -64.0f), glm::vec2(-128.0f, 144.0f));
        World().CreateEdge(glm::vec2(-192.0f, 80.0f), glm::vec2(-192.0f, 176.0f));
        World().CreateEdge(glm::vec2(-192.0f, 176.0f), glm::vec2(-128.0f, 240.0f));
        World().CreateEdge(glm::vec2(-128.0f, 144.0f), glm::vec2(192.0f, 64.0f));

        for (int i = 0; i < kNumBalls; ++i)
        {
            mBalls[i] = AddBall(glm::vec2(-224.0f + i, 80.0f + 64.0f * i));
        }

        glm::vec2 smallGearPos(-160.0f, -160.0f);
        glm::vec2 bigGearPos(80.0f, -160.0f);

        kx::Body *smallGear = World().CreateCircle(smallGearPos, 80.0f, 10.0f);
        kx::Body *bigGear = World().CreateCircle(bigGearPos, 160.0f, 40.0f);

        kx::Body *anchorSmall = World().CreateBody(kx::BodyType::Static, smallGearPos);
        kx::RevoluteJoint *jointSmall = new kx::RevoluteJoint(anchorSmall, smallGear, smallGearPos);
        World().AddJoint(jointSmall);

        kx::Body *anchorBig = World().CreateBody(kx::BodyType::Static, bigGearPos);
        mMotor = new kx::RevoluteJoint(anchorBig, bigGear, bigGearPos);
        World().AddJoint(mMotor);

        kx::GearJoint *gear = new kx::GearJoint(jointSmall, mMotor, 2.0f);
        World().AddJoint(gear);

        mPlunger = World().CreateBody(kx::BodyType::Dynamic, glm::vec2(-160.0f, -80.0f));
        glm::vec2 plungerVerts[4] = {
            glm::vec2(-30.0f, -80.0f),
            glm::vec2(-30.0f, 80.0f),
            glm::vec2(30.0f, 64.0f),
            glm::vec2(30.0f, -80.0f)};
        mPlunger->AddPolygon(plungerVerts, 4, 1.0f);
        mPlunger->SetRestitution(1.0f);
        mPlunger->SetFriction(0.5f);

        kx::DistanceJoint *rod = new kx::DistanceJoint(smallGear, mPlunger,
                                                       smallGearPos + glm::vec2(80.0f, 0.0f),
                                                       mPlunger->Position());
        World().AddJoint(rod);

        const float bottom = -300.0f;
        const float top = 32.0f;
        glm::vec2 feederCenter(-224.0f, (bottom + top) * 0.5f);
        kx::Body *feeder = World().CreateBody(kx::BodyType::Dynamic, feederCenter, 3.14159265f * 0.5f);
        feeder->AddBox((top - bottom) * 0.5f, 20.0f, glm::vec2(0.0f, 0.0f), 1.0f);

        kx::Body *anchorFeeder = World().CreateBody(kx::BodyType::Static, glm::vec2(-224.0f, bottom));
        kx::RevoluteJoint *feederPivot = new kx::RevoluteJoint(anchorFeeder, feeder,
                                                               glm::vec2(-224.0f, bottom));
        World().AddJoint(feederPivot);

        kx::DistanceJoint *feederRod = new kx::DistanceJoint(feeder, smallGear,
                                                             glm::vec2(-224.0f, -160.0f),
                                                             smallGearPos + glm::vec2(0.0f, 80.0f));
        World().AddJoint(feederRod);
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        float rate = 0.0f;
        if (input.KeyDown(SCANCODE_RIGHT))
            rate = 40.0f;
        else if (input.KeyDown(SCANCODE_LEFT))
            rate = -40.0f;
        mMotor->SetMotor(rate != 0.0f, rate, rate != 0.0f ? 1.0e8f : 0.0f);

        for (int i = 0; i < kNumBalls; ++i)
        {
            kx::Body *ball = mBalls[i];
            glm::vec2 pos = ball->Position();
            if (pos.x > 320.0f)
            {
                ball->SetPosition(glm::vec2(-224.0f, 200.0f));
                ball->SetVelocity(glm::vec2(0.0f, 0.0f));
                ball->SetAngularVelocity(0.0f);
            }
        }
    }

private:
    kx::Body *AddBall(const glm::vec2 &pos)
    {
        kx::Body *body = World().CreateCircle(pos, 30.0f, 1.0f);
        body->SetFriction(0.5f);
        return body;
    }

    static const int kNumBalls = 5;
    kx::Body *mBalls[kNumBalls];
    kx::Body *mPlunger;
    kx::RevoluteJoint *mMotor;
};

static Demo *CreatePump(Testbed &tb) { return new Pump(tb); }
static int gDemoPump = TestbedRegisterDemo("Joints", "Pump", &CreatePump);