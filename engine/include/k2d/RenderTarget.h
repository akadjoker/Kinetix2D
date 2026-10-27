#pragma once

namespace k2d
{

class RenderTarget
{
  public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool resize(int width, int height);
    void destroy();
    void bind() const;
    unsigned int texture() const { return mTexture; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }

  private:
    unsigned int mFramebuffer = 0;
    unsigned int mTexture = 0;
    unsigned int mDepth = 0;
    int mWidth = 0;
    int mHeight = 0;
};

} // namespace k2d
