#include "k2d/SpriteBatch.h"

#include <cstdio>

int main()
{
    k2d::SpriteBatch batch;
    int a = batch.add(nullptr, glm::vec2(1.0f, 2.0f), glm::vec2(8.0f, 9.0f));
    int b = batch.add(nullptr, glm::vec2(3.0f, 4.0f), glm::vec2(10.0f, 11.0f), 0xAABBCCDDu);
    batch.setSource(a, glm::vec4(2.0f, 3.0f, 4.0f, 5.0f));
    batch.setFlip(b, true, false);

    const k2d::SpriteBatch::Entry *first = batch.entry(a);
    const k2d::SpriteBatch::Entry *second = batch.entry(b);
    bool ok = batch.count() == 2 && first && second &&
              first->source == glm::vec4(2.0f, 3.0f, 4.0f, 5.0f) &&
              second->flags == 1 && second->color == 0xAABBCCDDu;
    batch.clear();
    ok = ok && batch.count() == 0 && batch.entry(0) == nullptr;
    std::printf("spritebatch=%s\n", ok ? "pass" : "fail");
    return ok ? 0 : 1;
}
