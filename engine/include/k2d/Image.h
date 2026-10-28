#pragma once

#include <ct/vector.hpp>

#include <cstddef>

namespace k2d
{

unsigned char* DecodeImageRgba(const unsigned char* data, size_t size, int& width, int& height);
void FreeImage(unsigned char* pixels);
bool WriteImagePng(const char* path, int width, int height, const unsigned char* rgba, int stride);
// The same PNG, into memory: for sending an image rather than saving it.
bool EncodePng(int width, int height, const unsigned char* rgba, int stride, ct::Vector<unsigned char>& out);

} // namespace k2d
