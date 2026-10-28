#include "k2d/Compress.h"

#include <miniz.h>

namespace k2d
{

bool Compress(const void* data, std::size_t size, ct::Vector<unsigned char>& out, int level)
{
    mz_ulong length = mz_compressBound(static_cast<mz_ulong>(size));
    out.resize(static_cast<std::size_t>(length));
    if (mz_compress2(out.data(), &length, static_cast<const unsigned char*>(data), static_cast<mz_ulong>(size), level) !=
        MZ_OK)
        return false;
    out.resize(static_cast<std::size_t>(length));
    return true;
}

bool Decompress(const unsigned char* data, std::size_t compressedSize, std::size_t size, ct::Vector<unsigned char>& out)
{
    out.resize(size);
    mz_ulong length = static_cast<mz_ulong>(size);
    return mz_uncompress(out.data(), &length, data, static_cast<mz_ulong>(compressedSize)) == MZ_OK &&
           length == static_cast<mz_ulong>(size);
}

} // namespace k2d
