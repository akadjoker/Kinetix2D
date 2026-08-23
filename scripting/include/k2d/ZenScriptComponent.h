#pragma once

#include "k2d/ScriptComponent.h"

#include <ct/string.hpp>

#include <cstddef>

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

        bool callEvent(const char *event, double value = 0.0);
        bool callFunction(const char *name, double value = 0.0);
        bool hasFunction(const char *name) const;

        struct State;

    protected:
        void onUpdate(float deltaTime) override;

    private:
        State *mState;
        ct::String mScriptPath;
    };

    class Assets;
    class GameObject;

    void DispatchZenScriptEvents(GameObject &root);
    void BroadcastZenScriptEvent(GameObject &root, const char *event, double value = 0.0);

    void SetZenScriptsEnabled(bool enabled);
    bool ZenScriptsEnabled();

    void SetZenScriptInput(Input *input);
    void SetZenScriptAssets(Assets *assets);
    void SetZenScriptOutput(void (*fn)(const char *text, bool isError, void *user), void *user);
    void RegisterZenScriptSerializer();

    class ZenBlackboard
    {
    public:
        static void setNumber(const char *key, double value);
        static void setString(const char *key, const char *value);
        static void setBool(const char *key, bool value);
        static double getNumber(const char *key, double fallback = 0.0);
        static ct::String getString(const char *key, const char *fallback = "");
        static bool getBool(const char *key, bool fallback = false);
        static bool has(const char *key);
        static void remove(const char *key);
        static void clear();

        enum class Kind
        {
            Number,
            String,
            Bool
        };

        static std::size_t keyCount();
        static ct::String keyAt(std::size_t index);
        static Kind kindOf(const char *key);

        static void emit(const char *event, double value = 0.0);
        static std::size_t pendingEventCount();
        static void clearEvents();

        using Handler = void (*)(const char *event, double value, void *user);
        static void setHostHandler(Handler handler, void *user);
    };

}
