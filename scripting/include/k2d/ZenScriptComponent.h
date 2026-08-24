#pragma once

#include "k2d/ScriptComponent.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstddef>

namespace k2d
{

    class Input;

    struct ZenScriptProperty
    {
        enum class Kind : unsigned char
        {
            Number,
            String,
            Bool
        };

        ct::String name;
        Kind kind = Kind::Number;
        double number = 0.0;
        ct::String text;
        bool flag = false;
        bool integer = false;
    };

    std::size_t ScanZenScriptProperties(const char *source, ct::Vector<ZenScriptProperty> &out);

    class ZenScriptComponent : public ScriptComponent
    {
    public:
        ZenScriptComponent();
        ~ZenScriptComponent() override;

        bool loadSource(const char *source, const char *scriptName = "script");
        bool loadFile(const char *path);
        bool loaded() const;
        bool pendingLoad() const;
        const ct::String &scriptPath() const { return mScriptPath; }

        bool callEvent(const char *event, double value = 0.0);
        bool callCollision(GameObject *other, bool began);
        bool callFunction(const char *name, double value = 0.0);
        bool hasFunction(const char *name) const;

        bool reloadIfChanged();
        long long sourceTimestamp() const { return mSourceTimestamp; }

        std::size_t declaredPropertyCount() const;
        const ZenScriptProperty *declaredPropertyAt(std::size_t index) const;
        const ZenScriptProperty *declaredProperty(const char *name) const;

        std::size_t overrideCount() const;
        const ZenScriptProperty *overrideAt(std::size_t index) const;
        const ZenScriptProperty *findOverride(const char *name) const;
        void setNumberOverride(const char *name, double value, bool integer = false);
        void setStringOverride(const char *name, const char *value);
        void setBoolOverride(const char *name, bool value);
        void clearOverride(const char *name);
        void clearOverrides();
        std::size_t applyOverrides();

        void destroyInstance();

        struct State;

    protected:
        void onUpdate(float deltaTime) override;

    private:
        bool loadFromSource(const char *source, const char *path);
        bool ensureInstance();

        ZenScriptProperty &overrideSlot(const char *name);
        bool writeProperty(const ZenScriptProperty &prop);

        State *mState;
        ct::String mScriptPath;
        ct::Vector<ZenScriptProperty> mOverrides;
        long long mSourceTimestamp = 0;
    };

    std::size_t ReloadChangedZenScripts();

    class Assets;
    class GameObject;

    class PhysicsWorld2D;

    void DispatchZenScriptEvents(GameObject &root);
    void RouteZenScriptCollisions(PhysicsWorld2D &world);
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
