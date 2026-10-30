#include "k2d/FileBuffer.h"

#include <platform.h>

#include <cstdlib>
#include <cstring>

namespace k2d
{

FileBuffer::FileBuffer() : mData(nullptr), mSize(0)
{
}

FileBuffer::~FileBuffer()
{
    Release();
}

void FileBuffer::Release()
{
    if (mData)
    {
        std::free(mData);
        mData = nullptr;
    }
    mSize = 0;
}

bool FileBuffer::Load(const char* path, bool nullTerminate)
{
    Release();
    size_t size = 0;
    uint8_t* data = file_read(path, &size);
    if (!data)
        return false;
    const bool ok = AssignCopy(data, size, nullTerminate);
    fs_free(data);
    return ok;
}

bool FileBuffer::AssignCopy(const void* data, size_t size, bool nullTerminate)
{
    Release();
    if (!data && size != 0)
        return false;
    const size_t allocationSize = size + (nullTerminate ? 1u : 0u);
    unsigned char* copy = static_cast<unsigned char*>(std::malloc(allocationSize > 0 ? allocationSize : 1));
    if (!copy)
        return false;
    if (size > 0)
        std::memcpy(copy, data, size);
    if (nullTerminate)
        copy[size] = 0;
    mData = copy;
    mSize = size;
    return true;
}

} // namespace k2d
