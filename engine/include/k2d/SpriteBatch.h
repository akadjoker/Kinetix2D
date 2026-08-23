#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    class Texture;

    class SpriteBatch : public Component
    {
    public:
        static const ComponentType Type = ComponentType::SpriteBatch;

        struct Entry
        {
            Texture *texture;
            Math::Vec2 position;
            Math::Vec2 size;
            Math::Vec4 source;
            Color color;
            unsigned char flags;
        };

        SpriteBatch();

        void clear();
        int add(Texture *texture, const Math::Vec2 &position, const Math::Vec2 &size,
                const Color &color = Color(0xFFFFFFFFu));
        void setSource(int index, const Math::Vec4 &source);
        void setFlip(int index, bool flipX, bool flipY);
        int count() const { return (int)mEntries.size(); }
        const Entry *entry(int index) const;
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        ct::Vector<Entry> mEntries;
        BlendMode mBlendMode;
    };

}