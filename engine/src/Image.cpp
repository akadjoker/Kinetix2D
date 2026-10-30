#include "k2d/Image.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace k2d
{

unsigned char* DecodeImageRgba(const unsigned char* data, size_t size, int& width, int& height)
{
    int channels = 0;
    return stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
}

void FreeImage(unsigned char* pixels)
{
    stbi_image_free(pixels);
}

bool WriteImagePng(const char* path, int width, int height, const unsigned char* rgba, int stride)
{
    return stbi_write_png(path, width, height, 4, rgba, stride) != 0;
}

namespace
{
void appendBytes(void* context, void* data, int size)
{
    ct::Vector<unsigned char>& out = *static_cast<ct::Vector<unsigned char>*>(context);
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (int i = 0; i < size; ++i)
        out.push_back(bytes[i]);
}
} // namespace

bool EncodePng(int width, int height, const unsigned char* rgba, int stride, ct::Vector<unsigned char>& out)
{
    out.clear();
    return stbi_write_png_to_func(appendBytes, &out, width, height, 4, rgba, stride) != 0 && !out.empty();
}

} // namespace k2d
