#include "testbed.h"

class SensorFunnel : public Demo
{
public:
    explicit SensorFunnel(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f, -500.0f)), mInside(0), mEntered(0), mExited(0)
    {
        T().Cam().center = glm::vec2(0.0f, 110.0f);
        T().Cam().zoom = 1.1f;

        World().CreateEdge(glm::vec2(-420.0f, 300.0f), glm::vec2(-85.0f, 65.0f));
        World().CreateEdge(glm::vec2(420.0f, 300.0f), glm::vec2(85.0f, 65.0f));
        World().CreateEdge(glm::vec2(-500.0f, -100.0f), glm::vec2(500.0f, -100.0f));

        kx::Body *sensor = World().CreateBody(kx::BodyType::Static, glm::vec2(0.0f, 20.0f));
        sensor->AddBox(85.0f, 14.0f, glm::vec2(0.0f), 0.0f);
        sensor->SetSensor(0, true);
        sensor->SetContactCallback(&SensorFunnel::OnContact, this);

        for (int i = 0; i < 16; ++i)
        {
            float x = -180.0f + 120.0f * (i % 4);
            float y = 210.0f + 45.0f * (i / 4);
            kx::Body *ball = World().CreateCircle(glm::vec2(x, y), 11.0f, 1.0f);
            ball->SetFriction(0.1f);
            ball->SetRestitution(0.2f);
        }
    }

    void Step(float) override
    {
        T().SetStatus("inside %d   begin %d   end %d", mInside, mEntered, mExited);
    }

private:
    static void OnContact(const kx::ContactEvent &event, void *context)
    {
        SensorFunnel *demo = static_cast<SensorFunnel *>(context);
        if (!event.sensor)
            return;
        if (event.phase == kx::ContactPhase::Begin)
        {
            ++demo->mInside;
            ++demo->mEntered;
        }
        else if (event.phase == kx::ContactPhase::End)
        {
            --demo->mInside;
            ++demo->mExited;
        }
    }

    int mInside;
    int mEntered;
    int mExited;
};

static Demo *CreateSensorFunnel(Testbed &tb) { return new SensorFunnel(tb); }
static int gDemoSensorFunnel = TestbedRegisterDemo("Events", "Sensor Funnel", &CreateSensorFunnel);
