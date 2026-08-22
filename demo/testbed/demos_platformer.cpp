#include "testbed.h"

#include <cmath>

static const int SCANCODE_RIGHT = 79;
static const int SCANCODE_LEFT = 80;

// Simple platformer demo: a fixed-rotation character (Box2D fixedRotation /
// Chipmunk infinite moment of inertia) running over static platforms and riding
// two kinematic sliding platforms — one lateral, one vertical.
//
// Controls: LEFT/RIGHT move, SPACE jumps (only when grounded).
class Platformer : public Demo
{
public:
    explicit Platformer(Testbed &tb)
        : Demo(tb, glm::vec2(0.0f, -500.0f)),
          mTime(0.0f), mContacts(0), mVerticalContacts(0),
          mCeilingContacts(0), mVerticalDirection(1.0f),
          mGrounded(false), mCrushed(false)
    {
        T().Cam().center = glm::vec2(0.0f, 10.0f);
        T().Cam().zoom = 0.8f;

        // Ground
        World().CreateStaticBox(glm::vec2(0.0f, -80.0f), 400.0f, 5.0f);

        // Static platforms
        World().CreateStaticBox(glm::vec2(-160.0f, -20.0f), 40.0f, 3.0f);
        World().CreateStaticBox(glm::vec2(30.0f, -15.0f), 35.0f, 3.0f);
        World().CreateStaticBox(glm::vec2(190.0f, -40.0f), 35.0f, 3.0f);

        // Lateral sliding platform (left-right, ping-pong)
        mLateral = World().CreateKinematicBox(glm::vec2(-120.0f, -45.0f), 30.0f, 3.0f);
        mLateral->SetFriction(1.0f);

        // Vertical sliding platform (up-down, ping-pong)
        mVertical = World().CreateKinematicBox(glm::vec2(30.0f, 12.0f), 30.0f, 3.0f);
        mVertical->SetFriction(1.0f);

        // Ceiling above the vertical platform. Contact with both the rising
        // platform and this ceiling is interpreted by the demo as crushing.
        mCeiling = World().CreateStaticBox(glm::vec2(30.0f, 60.0f), 40.0f, 3.0f);

        // Character: dynamic body with fixed rotation + contact callback
        // to know when it is grounded (so it can jump).
        mCharacter = World().CreateBox(glm::vec2(0.0f, -60.0f), 5.0f, 8.0f, 1.0f);
        mCharacter->SetFixedRotation(true);
        mCharacter->SetFriction(0.6f);
        mCharacter->SetContactCallback(&Platformer::OnContact, this);
    }

    void Step(float dt) override
    {
        mTime += dt;

        // Contact callbacks correm dentro de World::Step. A remocao fica
        // deferida para aqui, fora do envio de eventos, para nao alterar as
        // colecoes de contactos enquanto estao a ser percorridas.
        if (mCrushed && mCharacter)
        {
            mCharacter->SetContactCallback(nullptr);
            World().Destroy(mCharacter);
            mCharacter = nullptr;
            mContacts = 0;
            mVerticalContacts = 0;
            mCeilingContacts = 0;
        }
        mGrounded = mCharacter && mContacts > 0;

        // Kinematic sliding platforms (velocity-driven ping-pong, no drift).
        float lateralV = 110.0f * std::cos(mTime * 1.2f);
        mLateral->SetVelocity(glm::vec2(lateralV, 0.0f));

        const float verticalMin = -55.0f;
        const float verticalMax = 85.0f;
        if (mVertical->Position().y <= verticalMin)
            mVerticalDirection = 1.0f;
        else if (mVertical->Position().y >= verticalMax)
            mVerticalDirection = -1.0f;

        float verticalV = 70.0f * mVerticalDirection;
        mVertical->SetVelocity(glm::vec2(0.0f, verticalV));

        // Character control: only override horizontal velocity while a
        // direction key is held, so the platforms can carry it when idle.
        k2d::Input &input = T().Input();
        float dir = 0.0f;
        if (input.KeyDown(SCANCODE_RIGHT))
            dir += 1.0f;
        if (input.KeyDown(SCANCODE_LEFT))
            dir -= 1.0f;

        const float moveSpeed = 60.0f;
        const float jumpSpeed = 360.0f;

        if (mCharacter && dir != 0.0f)
        {
            glm::vec2 v = mCharacter->Velocity();
            v.x = dir * moveSpeed;
            mCharacter->SetVelocity(v);
        }
        if (mCharacter && input.KeyPressed(SCANCODE_SPACE) && mGrounded)
        {
            glm::vec2 v = mCharacter->Velocity();
            v.y = jumpSpeed;
            mCharacter->SetVelocity(v);
        }

        if (mCrushed)
            T().SetStatus("DEAD: crushed by platform   F5 restart");
        else
            T().SetStatus("grounded %d   lateral vx %.0f   vertical vy %.0f",
                          mGrounded ? 1 : 0, mLateral->Velocity().x, mVertical->Velocity().y);
    }

private:
    static void OnContact(const kx::ContactEvent &event, void *context)
    {
        Platformer *demo = static_cast<Platformer *>(context);
        if (event.sensor)
            return;
        if (event.phase == kx::ContactPhase::Begin)
        {
            ++demo->mContacts;
            if (event.other == demo->mVertical)
                ++demo->mVerticalContacts;
            if (event.other == demo->mCeiling)
                ++demo->mCeilingContacts;
        }
        else if (event.phase == kx::ContactPhase::End && demo->mContacts > 0)
        {
            --demo->mContacts;
            if (event.other == demo->mVertical && demo->mVerticalContacts > 0)
                --demo->mVerticalContacts;
            if (event.other == demo->mCeiling && demo->mCeilingContacts > 0)
                --demo->mCeilingContacts;
        }

        // O motor apenas comunicou os contactos. A decisao de gameplay e da
        // demo: comprimido por uma plataforma ascendente contra o teto = morte.
        if (demo->mVerticalContacts > 0 && demo->mCeilingContacts > 0 &&
            demo->mVertical->Velocity().y > 0.0f)
            demo->mCrushed = true;
    }

    kx::Body *mCharacter;
    kx::Body *mLateral;
    kx::Body *mVertical;
    kx::Body *mCeiling;
    float mTime;
    int mContacts;
    int mVerticalContacts;
    int mCeilingContacts;
    float mVerticalDirection;
    bool mGrounded;
    bool mCrushed;
};

static Demo *CreatePlatformer(Testbed &tb) { return new Platformer(tb); }
static int gDemoPlatformer = TestbedRegisterDemo("Platformer", "Moving Platforms", &CreatePlatformer);
