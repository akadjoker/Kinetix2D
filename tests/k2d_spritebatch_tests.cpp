#include "k2d/SpriteBatch.h"

#include <cstdio>

int main()
{
    k2d::SpriteBatch batch;
    int a = batch.add(nullptr, Math::Vec2(1.0f, 2.0f), Math::Vec2(8.0f, 9.0f));
    int b = batch.add(nullptr, Math::Vec2(3.0f, 4.0f), Math::Vec2(10.0f, 11.0f), 0xAABBCCDDu);
    batch.setSource(a, Math::Vec4(2.0f, 3.0f, 4.0f, 5.0f));
    batch.setFlip(b, true, false);

    const k2d::SpriteBatch::Entry *first = batch.entry(a);
    const k2d::SpriteBatch::Entry *second = batch.entry(b);
    bool ok = batch.count() == 2 && first && second &&
              first->source == Math::Vec4(2.0f, 3.0f, 4.0f, 5.0f) &&
              second->flags == 1 && second->color.Packed() == 0xAABBCCDDu;

    k2d::SpriteBatch::Entry *mutableFirst = batch.entryAt(a);
    ok = ok && mutableFirst;
    if (mutableFirst)
        mutableFirst->position = Math::Vec2(7.0f, 8.0f);
    ok = ok && batch.entry(a)->position == Math::Vec2(7.0f, 8.0f);
    ok = ok && batch.entryAt(-1) == nullptr && batch.entryAt(2) == nullptr;

    batch.remove(a);
    ok = ok && batch.count() == 1 && batch.entry(0)->flags == 1;
    batch.remove(5);
    ok = ok && batch.count() == 1;

    batch.clear();
    ok = ok && batch.count() == 0 && batch.entry(0) == nullptr;
    std::printf("spritebatch=%s\n", ok ? "pass" : "fail");
    return ok ? 0 : 1;
}