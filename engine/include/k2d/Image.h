#pragma once

#include <cstddef>

namespace k2d
{

unsigned char* DecodeImageRgba(const unsigned char* data, size_t size, int& width, int& height);
void FreeImage(unsigned char* pixels);
bool WriteImagePng(const char* path, int width, int height, const unsigned char* rgba, int stride);

} // namespace k2d
