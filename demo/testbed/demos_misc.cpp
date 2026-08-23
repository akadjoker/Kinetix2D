#include "testbed.h"

#include <stb_image.h>

#include <cmath>
#include <cstdio>

class Planet : public Demo
{
public:
    explicit Planet(Testbed &tb) : Demo(tb, Math::Vec2(0.0f, 0.0f))
    {
        T().Cam().center = Math::Vec2(0.0f, 0.0f);

        mPlanet = World().CreateBody(kx::BodyType::Kinematic, Math::Vec2(0.0f, 0.0f));
        mPlanet->AddCircle(Math::Vec2(0.0f, 0.0f), 70.0f, 1.0f);
        mPlanet->SetRestitution(1.0f);
        mPlanet->SetFriction(1.0f);
        mPlanet->SetAngularVelocity(0.2f);

        for (int i = 0; i < 30; ++i)
            AddBox();
    }

    void Step(float dt) override
    {
        const float kGravityStrength = 5.0e6f;
        for (size_t i = 0; i < mBoxes.size(); ++i)
        {
            kx::Body *body = mBoxes[i];
            Math::Vec2 p = body->Position();
            float sqdist = p.x * p.x + p.y * p.y;
            if (sqdist < 1.0f)
                continue;

            Math::Vec2 g = p * (-kGravityStrength / (sqdist * sqrtf(sqdist)));
            body->SetVelocity(body->Velocity() + g * dt);
        }
    }

private:
    void AddBox()
    {
        Math::Vec2 pos;
        do
        {
            pos = Math::Vec2(Rnd() * 640.0f - 320.0f, Rnd() * 480.0f - 240.0f);
        } while (pos.x * pos.x + pos.y * pos.y < 85.0f * 85.0f);

        float r = sqrtf(pos.x * pos.x + pos.y * pos.y);
        float v = sqrtf(5.0e6f / r) / r;

        kx::Body *body = World().CreateBody(kx::BodyType::Dynamic, pos, atan2f(pos.y, pos.x));
        body->AddBox(10.0f, 10.0f, Math::Vec2(0.0f, 0.0f), 1.0f);
        body->SetVelocity(Math::Vec2(-pos.y, pos.x) * v);
        body->SetAngularVelocity(v);
        body->SetFriction(0.7f);
        mBoxes.push_back(body);
    }

    kx::Body *mPlanet;
    ct::Vector<kx::Body *> mBoxes;
};

static Demo *CreatePlanet(Testbed &tb) { return new Planet(tb); }
static int gDemoPlanet = TestbedRegisterDemo("World", "Planet", &CreatePlanet);

class ImageShape : public Demo
{
public:
    explicit ImageShape(Testbed &tb) : Demo(tb, Math::Vec2(0.0f, -500.0f))
    {
        T().Cam().center = Math::Vec2(0.0f, 40.0f);
        T().SetStatus("SPACE spawns a bunny body traced from its sprite silhouette");

        World().CreateStaticBox(Math::Vec2(0.0f, -170.0f), 500.0f, 10.0f);

        k2d::FileBuffer file;
        if (file.Load("assets/wabbit_alpha.png", false))
        {
            int channels = 0;
            mPixels = stbi_load_from_memory(file.Data(), (int)file.Size(), &mWidth, &mHeight, &channels, 4);
        }

        for (int i = 0; i < 12; ++i)
            Spawn(Rnd() * 500.0f - 250.0f);
    }

    ~ImageShape() override
    {
        if (mPixels)
            stbi_image_free(mPixels);
    }

    void Step(float) override
    {
        k2d::Input &input = T().Input();
        if (input.KeyPressed(SCANCODE_SPACE))
            Spawn(Rnd() * 500.0f - 250.0f);
    }

private:
    void Spawn(float x)
    {
        if (!mPixels)
            return;
        World().CreateFromImage(Math::Vec2(x, 240.0f), mPixels, mWidth, mHeight, 4, 128, 1.0f, 2.0f);
    }

    unsigned char *mPixels;
    int mWidth;
    int mHeight;
};

static Demo *CreateImageShape(Testbed &tb) { return new ImageShape(tb); }
static int gDemoImageShape = TestbedRegisterDemo("World", "Image Shape", &CreateImageShape);