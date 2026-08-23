#include "k2d/SpriteBatch.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

namespace k2d
{

    SpriteBatch::SpriteBatch()
        : Component(Type, ComponentEventRender), mEntries(), mBlendMode(BLEND_MIX)
    {
    }

    void SpriteBatch::clear()
    {
        mEntries.clear();
    }

    int SpriteBatch::add(Texture *texture, const Math::Vec2 &position, const Math::Vec2 &size,
                         const Color &color)
    {
        Entry entry;
        entry.texture = texture;
        entry.position = position;
        entry.size = size;
        entry.source = Math::Vec4(0.0f);
        entry.color = color;
        entry.flags = 0;
        mEntries.push_back(entry);
        return (int)mEntries.size() - 1;
    }

    void SpriteBatch::setSource(int index, const Math::Vec4 &source)
    {
        if (index >= 0 && index < (int)mEntries.size())
            mEntries[index].source = source;
    }

    void SpriteBatch::setFlip(int index, bool flipX, bool flipY)
    {
        if (index >= 0 && index < (int)mEntries.size())
            mEntries[index].flags = (unsigned char)((flipX ? 1 : 0) | (flipY ? 2 : 0));
    }

    const SpriteBatch::Entry *SpriteBatch::entry(int index) const
    {
        if (index < 0 || index >= (int)mEntries.size())
            return nullptr;
        return &mEntries[index];
    }

    SpriteBatch::Entry *SpriteBatch::entryAt(int index)
    {
        if (index < 0 || index >= (int)mEntries.size())
            return nullptr;
        return &mEntries[index];
    }

    void SpriteBatch::remove(int index)
    {
        if (index >= 0 && index < (int)mEntries.size())
            mEntries.erase(mEntries.begin() + index);
    }

    void SpriteBatch::onRender(RenderQueue &queue)
    {
        RenderItem &item = queue.AddItem(owner()->zIndex());
        item.xform = owner()->globalTransform();
        item.blendMode = mBlendMode;

        for (size_t i = 0; i < mEntries.size(); ++i)
        {
            const Entry &entry = mEntries[i];
            if (!entry.texture)
                continue;

            RenderCommand command = RenderCommand::MakeRect(entry.texture->Id(),
                                                             entry.position.x, entry.position.y,
                                                             entry.size.x, entry.size.y);
            command.texWidth = entry.texture->Width();
            command.texHeight = entry.texture->Height();
            command.srcX = entry.source.x;
            command.srcY = entry.source.y;
            command.srcW = entry.source.z;
            command.srcH = entry.source.w;
            command.pivotX = 0.0f;
            command.pivotY = 0.0f;
            command.color = entry.color;
            command.flags = entry.flags;
            item.commands.push_back(command);
        }
    }

} 