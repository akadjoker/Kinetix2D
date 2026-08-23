
#include <k2d/k2d.h>

#include <cstdio>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D Serializer Diag", 64, 64, true))
        return 1;
    device.Focus();

    k2d::Assets assets;
    unsigned char redPx[4] = {255, 0, 0, 255};
    k2d::Texture *redTexture = assets.CreateTexture("red", 1, 1, redPx);
    if (!redTexture)
    {
        std::printf("FAIL: CreateTexture returned null\n");
        return 1;
    }

    k2d::Scene srcScene;
    k2d::GameObject *root = srcScene.createObject("Sprite");
    k2d::SpriteComponent *sprite = root->addComponent<k2d::SpriteComponent>(redTexture);

    ct::Json written = k2d::Serializer::WriteObject(*root, &assets);
    const ct::Json *texField = written["components"][0]["data"].find("texture");
    bool wroteName = texField && ct::String(texField->as_cstr()) == ct::String("red");

    k2d::Scene dstScene;
    k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written, nullptr, &assets);
    k2d::SpriteComponent *copySprite = copy ? copy->getComponent<k2d::SpriteComponent>() : nullptr;
    bool resolvedSameTexture = copySprite && copySprite->texture() == redTexture;

    (void)sprite;
    bool ok = wroteName && resolvedSameTexture;
    std::printf("Serializer texture round-trip: wrote_name=%s resolved_same_pointer=%s -> %s\n",
                wroteName ? "pass" : "fail", resolvedSameTexture ? "pass" : "fail",
                ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}