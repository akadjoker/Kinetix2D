#include "testbed.h"

#include <cstdio>

class SingleBox : public Demo
{
public:
    explicit SingleBox(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -300.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 60.0f);

        World().CreateEdge(glm::vec2(-600.0f, 0.0f), glm::vec2(600.0f, 0.0f));

        kx::Body *box = World().CreateBox(glm::vec2(0.0f, 40.0f), 20.0f, 20.0f, 1.0f);
        box->SetVelocity(glm::vec2(150.0f, 0.0f));
        mBox = box;
    }

    void Step(float) override
    {
        T().SetStatus("(x, y) = (%.1f, %.1f)", mBox->Position().x, mBox->Position().y);
    }

private:
    kx::Body *mBox;
};

static Demo *CreateSingleBox(Testbed &tb) { return new SingleBox(tb); }
static int gDemoSingleBox = TestbedRegisterDemo("Basic", "Single Box", &CreateSingleBox);

class PyramidStack : public Demo
{
public:
    explicit PyramidStack(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -100.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 10.0f);

        kx::Body *left = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(-320.0f, 240.0f));
        left->SetRestitution(1.0f);
        left->SetFriction(1.0f);

        kx::Body *right = World().CreateEdge(glm::vec2(320.0f, -240.0f), glm::vec2(320.0f, 240.0f));
        right->SetRestitution(1.0f);
        right->SetFriction(1.0f);

        kx::Body *floor = World().CreateEdge(glm::vec2(-320.0f, -240.0f), glm::vec2(320.0f, -240.0f));
        floor->SetRestitution(1.0f);
        floor->SetFriction(1.0f);

        for (int i = 0; i < 14; ++i)
        {
            for (int j = 0; j <= i; ++j)
            {
                kx::Body *box = World().CreateBox(glm::vec2(j * 32.0f - i * 16.0f, 300.0f - i * 32.0f), 15.0f, 15.0f, 1.0f);
                box->SetFriction(0.8f);
            }
        }

        kx::Body *ball = World().CreateCircle(glm::vec2(0.0f, -220.0f), 15.0f, 1.0f);
        ball->SetFriction(0.9f);
    }
};

static Demo *CreatePyramidStack(Testbed &tb) { return new PyramidStack(tb); }
static int gDemoPyramidStack = TestbedRegisterDemo("Basic", "Pyramid Stack", &CreatePyramidStack);

class TiltedStack : public Demo
{
public:
    explicit TiltedStack(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -300.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 250.0f);
        T().Cam().zoom = 1.0f;

        World().CreateStaticBox(glm::vec2(0.0f, -20.0f), 400.0f, 20.0f);

        const int kColumns = 10;
        const int kRows = 10;
        float dx = 150.0f;
        float offset = 6.0f;
        float xroot = -0.5f * dx * (kColumns - 1.0f);

        for (int j = 0; j < kColumns; ++j)
        {
            float x = xroot + j * dx;
            for (int i = 0; i < kRows; ++i)
            {
                kx::Body *box = World().CreateBox(glm::vec2(x + offset * i, 15.0f + 30.0f * i), 13.5f, 13.5f, 1.0f);
                box->SetFriction(0.3f);
            }
        }
    }
};

static Demo *CreateTiltedStack(Testbed &tb) { return new TiltedStack(tb); }
static int gDemoTiltedStack = TestbedRegisterDemo("Basic", "Tilted Stack", &CreateTiltedStack);

class Plink : public Demo
{
public:
    explicit Plink(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -100.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 0.0f);
        T().SetStatus("pentagons respawn when they fall off screen");

        World().SetVelocityIterations(5);

        glm::vec2 tris[3] = {
            glm::vec2(-15.0f, -15.0f),
            glm::vec2(0.0f, 10.0f),
            glm::vec2(15.0f, -15.0f)};

        for (int i = 0; i < 9; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                float stagger = (j % 2) * 40.0f;
                glm::vec2 pos(i * 80.0f - 320.0f + stagger, j * 70.0f - 240.0f);
                kx::Body *pin = World().CreateBody(kx::BodyType::Static, pos);
                pin->AddPolygon(tris, 3, 1.0f);
                pin->SetRestitution(1.0f);
                pin->SetFriction(1.0f);
            }
        }

        glm::vec2 verts[5];
        for (int i = 0; i < 5; ++i)
        {
            float angle = -2.0f * 3.14159265f * i / 5.0f;
            verts[i] = glm::vec2(10.0f * cosf(angle), 10.0f * sinf(angle));
        }

        for (int i = 0; i < 300; ++i)
        {
            float x = Rnd() * 640.0f - 320.0f;
            kx::Body *penta = World().CreateBody(kx::BodyType::Dynamic, glm::vec2(x, 350.0f));
            penta->AddPolygon(verts, 5, 1.0f);
            penta->SetFriction(0.4f);
            mPentagons.push_back(penta);
        }
    }

    void Step(float) override
    {
        for (size_t i = 0; i < mPentagons.size(); ++i)
        {
            kx::Body *body = mPentagons[i];
            glm::vec2 pos = body->Position();
            if (pos.y < -260.0f || pos.x > 340.0f || pos.x < -340.0f)
            {
                body->SetPosition(glm::vec2(Rnd() * 640.0f - 320.0f, 260.0f));
                body->SetVelocity(glm::vec2(0.0f, 0.0f));
                body->SetAngularVelocity(0.0f);
            }
        }
    }

private:
    ct::Vector<kx::Body *> mPentagons;
};

static Demo *CreatePlink(Testbed &tb) { return new Plink(tb); }
static int gDemoPlink = TestbedRegisterDemo("Basic", "Plink", &CreatePlink);

class Tumble : public Demo
{
public:
    explicit Tumble(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -600.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 0.0f);

        mBox = World().CreateBody(kx::BodyType::Kinematic, glm::vec2(0.0f, 0.0f));
        mBox->AddEdge(glm::vec2(-200.0f, -200.0f), glm::vec2(-200.0f, 200.0f));
        mBox->AddEdge(glm::vec2(-200.0f, 200.0f), glm::vec2(200.0f, 200.0f));
        mBox->AddEdge(glm::vec2(200.0f, 200.0f), glm::vec2(200.0f, -200.0f));
        mBox->AddEdge(glm::vec2(200.0f, -200.0f), glm::vec2(-200.0f, -200.0f));
        mBox->SetRestitution(1.0f);
        mBox->SetFriction(1.0f);
        mBox->SetAngularVelocity(0.4f);

        for (int i = 0; i < 7; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                glm::vec2 pos(i * 30.0f - 150.0f, j * 60.0f - 150.0f);
                int type = ((int)(Rnd() * 3000.0f)) / 1000;

                if (type == 0)
                {
                    kx::Body *box = World().CreateBox(pos, 15.0f, 30.0f, 1.0f);
                    box->SetFriction(0.7f);
                }
                else if (type == 1)
                {
                    kx::Body *bar = World().CreateBox(pos, 15.0f, 29.0f, 1.0f);
                    bar->SetFriction(0.7f);
                }
                else
                {
                    kx::Body *c1 = World().CreateCircle(pos + glm::vec2(0.0f, 15.0f), 15.0f, 1.0f);
                    c1->SetFriction(0.7f);
                    kx::Body *c2 = World().CreateCircle(pos + glm::vec2(0.0f, -15.0f), 15.0f, 1.0f);
                    c2->SetFriction(0.7f);
                }
            }
        }
    }

private:
    kx::Body *mBox;
};

static Demo *CreateTumble(Testbed &tb) { return new Tumble(tb); }
static int gDemoTumble = TestbedRegisterDemo("Basic", "Tumble", &CreateTumble);