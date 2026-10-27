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

} // namespace k2d
