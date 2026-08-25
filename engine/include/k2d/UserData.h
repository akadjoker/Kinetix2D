#pragma once

#include <ct/json.hpp>
#include <ct/string.hpp>

namespace k2d
{
    // Unity-style persistent user data. SDL chooses a writable per-user path
    // for the supplied organisation/application, never the project folder.
    class UserData
    {
    public:
        bool open(const char *organization, const char *application);
        bool load(const char *fileName = "settings.json");
        bool save(const char *fileName = "settings.json") const;

        const ct::String &path() const { return mPath; }
        ct::String filePath(const char *fileName) const;

        bool has(const char *key) const;
        void erase(const char *key);
        void clear();

        void setString(const char *key, const char *value);
        void setInt(const char *key, int value);
        void setFloat(const char *key, float value);
        void setBool(const char *key, bool value);
        const char *getString(const char *key, const char *fallback = "") const;
        int getInt(const char *key, int fallback = 0) const;
        float getFloat(const char *key, float fallback = 0.0f) const;
        bool getBool(const char *key, bool fallback = false) const;

        bool writeText(const char *fileName, const ct::String &text) const;
        bool readText(const char *fileName, ct::String &text) const;

    private:
        bool validFileName(const char *fileName) const;

        ct::String mPath;
        ct::Json mValues = ct::Json::object();
    };
}
