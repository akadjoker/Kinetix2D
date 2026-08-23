#pragma once

#include "k2d/ScriptComponent.h"

#include <ct/string.hpp>

namespace k2d
{

    class Input;

    class ZenScriptComponent : public ScriptComponent
    {
    public:
        ZenScriptComponent();
        ~ZenScriptComponent() override;

        bool loadSource(const char *source, const char *scriptName = "script");
        bool loadFile(const char *path);
        bool loaded() const;
        const ct::String &scriptPath() const { return mScriptPath; }

        struct State;

    protected:
        void onUpdate(float deltaTime) override;

    private:
        State *mState;
        ct::String mScriptPath;
    };

    void SetZenScriptInput(Input *input);
    void RegisterZenScriptSerializer();

}
