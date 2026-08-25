#include "k2d/UserData.h"

#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"

#include <SDL.h>

#include <cstring>

namespace k2d
{
bool UserData::open(const char *organization, const char *application)
{
    char *path = SDL_GetPrefPath(organization && organization[0] ? organization : "Kinetix2D",
                                 application && application[0] ? application : "App");
    if (!path)
        return false;
    mPath = path;
    SDL_free(path);
    mValues = ct::Json::object();
    return true;
}

bool UserData::validFileName(const char *fileName) const
{
    return fileName && fileName[0] && !mPath.empty() && !std::strstr(fileName, "..") &&
           !std::strchr(fileName, '/') && !std::strchr(fileName, '\\');
}

ct::String UserData::filePath(const char *fileName) const
{
    if (!validFileName(fileName))
        return ct::String();
    ct::String result = mPath;
    result += fileName;
    return result;
}

bool UserData::load(const char *fileName)
{
    const ct::String path = filePath(fileName);
    if (path.empty())
        return false;
    FileBuffer buffer;
    if (!FileSystem::Instance().LoadFile(path.c_str(), buffer, true))
        return false;
    ct::Json::Error error;
    const ct::Json values = ct::Json::parse(buffer.Text(), &error);
    if (error || !values.is_object())
        return false;
    mValues = values;
    return true;
}

bool UserData::save(const char *fileName) const
{
    const ct::String path = filePath(fileName);
    return !path.empty() && FileSystem::Instance().SaveTextFile(path.c_str(), mValues.dump(2));
}

bool UserData::has(const char *key) const { return key && mValues.contains(key); }
void UserData::erase(const char *key) { if (key) mValues.erase(key); }
void UserData::clear() { mValues = ct::Json::object(); }
void UserData::setString(const char *key, const char *value) { if (key && key[0]) mValues.set(key, ct::Json(value ? value : "")); }
void UserData::setInt(const char *key, int value) { if (key && key[0]) mValues.set(key, ct::Json(value)); }
void UserData::setFloat(const char *key, float value) { if (key && key[0]) mValues.set(key, ct::Json(value)); }
void UserData::setBool(const char *key, bool value) { if (key && key[0]) mValues.set(key, ct::Json(value)); }
const char *UserData::getString(const char *key, const char *fallback) const
{ return key ? mValues[key].as_cstr(fallback ? fallback : "") : (fallback ? fallback : ""); }
int UserData::getInt(const char *key, int fallback) const { return key ? (int)mValues[key].as_int(fallback) : fallback; }
float UserData::getFloat(const char *key, float fallback) const
{ return key ? (float)mValues[key].as_double(fallback) : fallback; }
bool UserData::getBool(const char *key, bool fallback) const { return key ? mValues[key].as_bool(fallback) : fallback; }

bool UserData::writeText(const char *fileName, const ct::String &text) const
{
    const ct::String path = filePath(fileName);
    return !path.empty() && FileSystem::Instance().SaveTextFile(path.c_str(), text);
}

bool UserData::readText(const char *fileName, ct::String &text) const
{
    const ct::String path = filePath(fileName);
    FileBuffer buffer;
    if (path.empty() || !FileSystem::Instance().LoadFile(path.c_str(), buffer, true))
        return false;
    text = buffer.Text();
    return true;
}
}
