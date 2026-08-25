#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    struct SpriteRegion
    {
        ct::String id;
        Math::Vec4 rect = Math::Vec4(0.0f);
    };

    // Named regions inside one texture. Sprite Editor stores these beside the
    // image so reopening it restores the same palette.
    class SpriteAtlas
    {
    public:
        void clear();
        void setTexturePath(const char *path) { mTexturePath = path ? path : ""; }
        const ct::String &texturePath() const { return mTexturePath; }

        size_t regionCount() const { return mRegions.size(); }
        const SpriteRegion *regionAt(size_t index) const
        { return index < mRegions.size() ? &mRegions[index] : nullptr; }
        const SpriteRegion *find(const char *id) const;
        bool add(const char *id, const Math::Vec4 &rect);
        bool set(const char *id, const char *newId, const Math::Vec4 &rect);
        bool remove(const char *id);

        // Finds 8-connected alpha islands. Existing regions are replaced while
        // the atlas texture path is preserved.
        size_t detectTransparentRegions(const class Pixmap &image, unsigned char alphaThreshold = 1,
                                        int minPixels = 4);

        bool load(const char *path);
        bool save(const char *path) const;

    private:
        ct::String mTexturePath;
        ct::Vector<SpriteRegion> mRegions;
    };

}
