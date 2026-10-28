#pragma once

#include <ct/vector.hpp>

#include <cstddef>

namespace k2d
{

// Deflate (zlib) from miniz: level 1 is fast, 9 is small.
bool Compress(const void* data, std::size_t size, ct::Vector<unsigned char>& out, int level = 1);
// size is the length the data had before it was compressed.
bool Decompress(const unsigned char* data, std::size_t compressedSize, std::size_t size, ct::Vector<unsigned char>& out);

} // namespace k2d
