#include "testbed.h"

class VerticalStack : public Demo
{
public:
    explicit VerticalStack(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f, -500.0f)), mRows(12), mColumns(1), mShapeType(1)
    {
        T().Cam().center = glm::vec2(-120.0f, 260.0f);
        T().Cam().zoom = 1.0f;
        T().SetStatus("Box2D Vertical Stack; bullet controls deferred until CCD");

        World().CreateEdge(glm::vec2(-700.0f, 0.0f), glm::vec2(700.0f, 0.0f));
        World().CreateEdge(glm::vec2(300.0f, 0.0f), glm::vec2(300.0f, 600.0f));
        CreateStacks();
    }

    void UpdateUI() override
    {
        const char *shapeTypes[] = {"Circle", "Box"};
        bool changed = ImGui::Combo("shape", &mShapeType, shapeTypes, 2);
        changed = ImGui::SliderInt("rows", &mRows, 1, 25) || changed;
        changed = ImGui::SliderInt("columns", &mColumns, 1, 5) || changed;
        changed = ImGui::Button("reset stack") || changed;
        if (changed)
            CreateStacks();
    }

private:
    void CreateStacks()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
            World().Destroy(mBodies[i]);
        mBodies.clear();

        const float size = 15.0f;
        const float dx = -90.0f;
        const float xroot = 240.0f;
        float offset = mShapeType == 0 ? 0.0f : 0.3f;

        for (int column = 0; column < mColumns; ++column)
        {
            float x = xroot + column * dx;
            for (int row = 0; row < mRows; ++row)
            {
                float shift = row % 2 == 0 ? -offset : offset;
                glm::vec2 position(x + shift, size + 2.0f * size * row);
                kx::Body *body = mShapeType == 0
                                     ? World().CreateCircle(position, size, 1.0f)
                                     : World().CreateBox(position, size * 0.9f, size * 0.9f, 1.0f);
                body->SetFriction(0.3f);
                mBodies.push_back(body);
            }
        }
    }

    ct::Vector<kx::Body *> mBodies;
    int mRows;
    int mColumns;
    int mShapeType;
};

static Demo *CreateVerticalStack(Testbed &tb) { return new VerticalStack(tb); }
static int gDemoVerticalStack = TestbedRegisterDemo("Stacking", "Vertical Stack", &CreateVerticalStack);

class CircleStack : public Demo
{
public:
    explicit CircleStack(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -600.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 120.0f);
        T().Cam().zoom = 1.4f;
        T().SetStatus("four circles with increasing density and restitution 0.8");

        World().CreateEdge(glm::vec2(-500.0f, 0.0f), glm::vec2(500.0f, 0.0f));

        float y = 22.5f;
        for (int i = 0; i < 4; ++i)
        {
            kx::Body *circle = World().CreateCircle(glm::vec2(0.0f, y), 15.0f, 1.0f + 4.0f * i);
            circle->SetFriction(0.0f);
            circle->SetRestitution(0.8f);
            y += 37.5f;
        }
    }
};

static Demo *CreateCircleStack(Testbed &tb) { return new CircleStack(tb); }
static int gDemoCircleStack = TestbedRegisterDemo("Stacking", "Circle Stack", &CreateCircleStack);

class DoubleDomino : public Demo
{
public:
    explicit DoubleDomino(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 80.0f);
        T().Cam().zoom = 1.25f;

        World().CreateStaticBox(glm::vec2(0.0f, -25.0f), 700.0f, 25.0f);

        const int count = 15;
        const float spacing = 45.0f;
        float x = -0.5f * count * spacing;
        for (int i = 0; i < count; ++i)
        {
            kx::Body *domino = World().CreateBox(glm::vec2(x, 25.0f), 5.625f, 22.5f, 1.0f);
            domino->SetFriction(0.6f);
            if (i == 0)
            {
                glm::vec2 point(x, 50.0f);
                domino->ApplyImpulse(glm::vec2(40.0f * domino->Mass(), 0.0f), point);
            }
            x += spacing;
        }
    }
};

static Demo *CreateDoubleDomino(Testbed &tb) { return new DoubleDomino(tb); }
static int gDemoDoubleDomino = TestbedRegisterDemo("Stacking", "Double Domino", &CreateDoubleDomino);

class Arch : public Demo
{
public:
    explicit Arch(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(0.0f, 190.0f);
        T().Cam().zoom = 1.15f;

        const float scale = 8.0f;
        glm::vec2 inner[9] = {
            glm::vec2(16.0f, 0.0f),
            glm::vec2(14.9380371f, 5.1336011f),
            glm::vec2(13.7987175f, 10.2492807f),
            glm::vec2(12.5625296f, 15.3410702f),
            glm::vec2(11.2004099f, 20.3985654f),
            glm::vec2(9.6652122f, 25.4036990f),
            glm::vec2(7.8717993f, 30.3179321f),
            glm::vec2(5.6351996f, 35.0382080f),
            glm::vec2(2.4059380f, 39.0955429f)};
        glm::vec2 outer[9] = {
            glm::vec2(24.0f, 0.0f),
            glm::vec2(22.3361950f, 6.0229983f),
            glm::vec2(20.5493698f, 12.0096436f),
            glm::vec2(18.6085453f, 17.9470329f),
            glm::vec2(16.4676933f, 23.8136787f),
            glm::vec2(14.0532503f, 29.5707932f),
            glm::vec2(11.2355108f, 35.1377563f),
            glm::vec2(7.7525682f, 40.3045082f),
            glm::vec2(3.0169315f, 44.2889175f)};

        for (int i = 0; i < 9; ++i)
        {
            inner[i] *= 0.25f * scale;
            outer[i] *= 0.25f * scale;
        }

        World().CreateEdge(glm::vec2(-700.0f, 0.0f), glm::vec2(700.0f, 0.0f))->SetFriction(0.6f);

        for (int i = 0; i < 8; ++i)
        {
            glm::vec2 right[4] = {inner[i], outer[i], outer[i + 1], inner[i + 1]};
            kx::Body *rightBody = World().CreatePolygon(glm::vec2(0.0f), right, 4, 1.0f);
            rightBody->SetFriction(0.6f);

            glm::vec2 left[4] = {
                glm::vec2(-outer[i].x, outer[i].y),
                glm::vec2(-inner[i].x, inner[i].y),
                glm::vec2(-inner[i + 1].x, inner[i + 1].y),
                glm::vec2(-outer[i + 1].x, outer[i + 1].y)};
            kx::Body *leftBody = World().CreatePolygon(glm::vec2(0.0f), left, 4, 1.0f);
            leftBody->SetFriction(0.6f);
        }

        glm::vec2 keystone[4] = {
            inner[8], outer[8], glm::vec2(-outer[8].x, outer[8].y), glm::vec2(-inner[8].x, inner[8].y)};
        World().CreatePolygon(glm::vec2(0.0f), keystone, 4, 1.0f)->SetFriction(0.6f);

        for (int i = 0; i < 4; ++i)
        {
            glm::vec2 position(0.0f, outer[8].y + 4.0f + 8.0f * i);
            World().CreateBox(position, 16.0f, 4.0f, 1.0f)->SetFriction(0.6f);
        }
    }
};

static Demo *CreateArch(Testbed &tb) { return new Arch(tb); }
static int gDemoArch = TestbedRegisterDemo("Stacking", "Arch", &CreateArch);

class CardHouse : public Demo
{
public:
    explicit CardHouse(Testbed &tb) : Demo(tb, glm::vec2(0.0f, -500.0f))
    {
        T().Cam().center = glm::vec2(80.0f, 105.0f);
        T().Cam().zoom = 2.0f;

        World().CreateStaticBox(glm::vec2(0.0f, -20.0f), 600.0f, 20.0f)->SetFriction(0.7f);

        const float scale = 220.0f;
        const float cardHeight = 0.2f * scale;
        const float cardThickness = 1.0f;
        const float angle0 = 25.0f * kx::kPi / 180.0f;
        const float angle1 = -25.0f * kx::kPi / 180.0f;
        const float angle2 = 0.5f * kx::kPi;

        int count = 5;
        float xroot = 0.0f;
        float y = cardHeight - 0.02f * scale;
        while (count > 0)
        {
            float x = xroot;
            for (int i = 0; i < count; ++i)
            {
                if (i != count - 1)
                    CreateCard(glm::vec2(x + 0.25f * scale, y + cardHeight - 0.015f * scale), angle2, cardThickness, cardHeight);

                CreateCard(glm::vec2(x, y), angle1, cardThickness, cardHeight);
                x += 0.175f * scale;
                CreateCard(glm::vec2(x, y), angle0, cardThickness, cardHeight);
                x += 0.175f * scale;
            }
            y += 2.0f * cardHeight - 0.03f * scale;
            xroot += 0.175f * scale;
            --count;
        }
    }

private:
    void CreateCard(const glm::vec2 &position, float angle, float halfWidth, float halfHeight)
    {
        kx::Body *card = World().CreateBody(kx::BodyType::Dynamic, position, angle);
        card->AddBox(halfWidth, halfHeight, glm::vec2(0.0f), 1.0f);
        card->SetFriction(0.7f);
    }
};

static Demo *CreateCardHouse(Testbed &tb) { return new CardHouse(tb); }
static int gDemoCardHouse = TestbedRegisterDemo("Stacking", "Card House", &CreateCardHouse);