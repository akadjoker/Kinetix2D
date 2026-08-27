#include "k2d/SpriteAtlas.h"

#include "k2d/Pixmap.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"

#include <ct/json.hpp>
#include <ct/vector.hpp>

#include <cstring>
#include <cstdio>

namespace k2d
{

namespace
{
Math::Vec4 readRect(const ct::Json &value)
{
    if (!value.is_array() || value.size() < 4)
        return Math::Vec4(0.0f);
    return Math::Vec4((float)value[0].as_double(0.0), (float)value[1].as_double(0.0),
                      (float)value[2].as_double(0.0), (float)value[3].as_double(0.0));
}

ct::Json writeRect(const Math::Vec4 &value)
{
    ct::Json result = ct::Json::array();
    result.push_back(value.x);
    result.push_back(value.y);
    result.push_back(value.z);
    result.push_back(value.w);
    return result;
}
}

void SpriteAtlas::clear()
{
    mTexturePath.clear();
    mRegions.clear();
}

const SpriteRegion *SpriteAtlas::find(const char *id) const
{
    if (!id)
        return nullptr;
    for (size_t i = 0; i < mRegions.size(); ++i)
        if (std::strcmp(mRegions[i].id.c_str(), id) == 0)
            return &mRegions[i];
    return nullptr;
}

bool SpriteAtlas::add(const char *id, const Math::Vec4 &rect)
{
    if (!id || !id[0] || rect.z <= 0.0f || rect.w <= 0.0f || find(id))
        return false;
    SpriteRegion region;
    region.id = id;
    region.rect = rect;
    mRegions.push_back(region);
    return true;
}

bool SpriteAtlas::set(const char *id, const char *newId, const Math::Vec4 &rect)
{
    if (!id || !newId || !newId[0] || rect.z <= 0.0f || rect.w <= 0.0f)
        return false;
    for (size_t i = 0; i < mRegions.size(); ++i)
    {
        if (mRegions[i].id != id)
            continue;
        const SpriteRegion *duplicate = find(newId);
        if (duplicate && duplicate != &mRegions[i])
            return false;
        mRegions[i].id = newId;
        mRegions[i].rect = rect;
        return true;
    }
    return false;
}

bool SpriteAtlas::remove(const char *id)
{
    if (!id)
        return false;
    for (size_t i = 0; i < mRegions.size(); ++i)
        if (mRegions[i].id == id)
        {
            mRegions.erase(mRegions.begin() + i);
            return true;
        }
    return false;
}

size_t SpriteAtlas::detectTransparentRegions(const Pixmap &image, unsigned char alphaThreshold,
                                             int minPixels)
{
    const int width = image.Width();
    const int height = image.Height();
    const unsigned char *pixels = image.Pixels();
    if (!pixels || width <= 0 || height <= 0)
        return 0;

    struct Pixel { int x; int y; };
    ct::Vector<unsigned char> visited((size_t)width * (size_t)height, 0);
    ct::Vector<Pixel> pending;
    mRegions.clear();
    minPixels = minPixels > 0 ? minPixels : 1;
    int regionIndex = 0;

    for (int startY = 0; startY < height; ++startY)
    {
        for (int startX = 0; startX < width; ++startX)
        {
            const size_t start = (size_t)startY * (size_t)width + (size_t)startX;
            if (visited[start] || pixels[start * 4u + 3u] < alphaThreshold)
                continue;

            visited[start] = 1;
            pending.clear();
            pending.push_back({startX, startY});
            int left = startX, right = startX, top = startY, bottom = startY, count = 0;
            while (!pending.empty())
            {
                const Pixel pixel = pending.back();
                pending.pop_back();
                ++count;
                if (pixel.x < left) left = pixel.x;
                if (pixel.x > right) right = pixel.x;
                if (pixel.y < top) top = pixel.y;
                if (pixel.y > bottom) bottom = pixel.y;

                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dx == 0 && dy == 0)
                            continue;
                        const int x = pixel.x + dx;
                        const int y = pixel.y + dy;
                        if (x < 0 || x >= width || y < 0 || y >= height)
                            continue;
                        const size_t index = (size_t)y * (size_t)width + (size_t)x;
                        if (visited[index] || pixels[index * 4u + 3u] < alphaThreshold)
                            continue;
                        visited[index] = 1;
                        pending.push_back({x, y});
                    }
            }
            if (count < minPixels)
                continue;
            char id[64];
            std::snprintf(id, sizeof(id), "sprite_%03d", ++regionIndex);
            add(id, Math::Vec4((float)left, (float)top, (float)(right - left + 1),
                                (float)(bottom - top + 1)));
        }
    }
    return mRegions.size();
}

bool SpriteAtlas::load(const char *path)
{
    FileBuffer buffer;
    if (!path || !FileSystem::Instance().LoadFile(path, buffer, true))
        return false;
    ct::Json::Error error;
    const ct::Json document = ct::Json::parse(buffer.Text(), &error);
    if (error || !document.is_object())
        return false;
    clear();
    mTexturePath = document["texture"].as_cstr("");
    const ct::Json &regions = document["regions"];
    if (regions.is_array())
        for (size_t i = 0; i < regions.size(); ++i)
            add(regions[i]["id"].as_cstr(""), readRect(regions[i]["rect"]));
    return true;
}

bool SpriteAtlas::save(const char *path) const
{
    if (!path || !path[0] || mTexturePath.empty())
        return false;
    ct::Json document = ct::Json::object();
    document.set("texture", ct::Json(mTexturePath.c_str()));
    ct::Json regions = ct::Json::array();
    for (size_t i = 0; i < mRegions.size(); ++i)
    {
        ct::Json region = ct::Json::object();
        region.set("id", ct::Json(mRegions[i].id.c_str()));
        region.set("rect", writeRect(mRegions[i].rect));
        regions.push_back(region);
    }
    document.set("regions", regions);
    return FileSystem::Instance().SaveTextFile(path, document.dump(2));
}

}
