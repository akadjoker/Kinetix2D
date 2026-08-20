#pragma once

#include <cstddef>

namespace k2d
{

    class FileBuffer
    {
    public:
        FileBuffer();
        ~FileBuffer();

        FileBuffer(const FileBuffer &) = delete;
        FileBuffer &operator=(const FileBuffer &) = delete;

        bool Load(const char *path, bool nullTerminate = false);
        void Release();

        const unsigned char *Data() const { return mData; }
        size_t Size() const { return mSize; }
        const char *Text() const { return reinterpret_cast<const char *>(mData); }

    private:
        unsigned char *mData;
        size_t mSize;
    };

}
