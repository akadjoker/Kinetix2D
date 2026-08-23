#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

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
            glm::vec2 position;
            glm::vec2 size;
            glm::vec4 source;
            unsigned int color;
            unsigned char flags;
        };

        SpriteBatch();

        void clear();
        int add(Texture *texture, const glm::vec2 &position, const glm::vec2 &size,
                unsigned int color = 0xFFFFFFFFu);
        void setSource(int index, const glm::vec4 &source);
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