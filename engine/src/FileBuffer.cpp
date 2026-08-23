#include "k2d/FileBuffer.h"

#include <SDL_rwops.h>
#include <cstdlib>

namespace k2d
{

    FileBuffer::FileBuffer()
        : mData(nullptr), mSize(0)
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

    bool FileBuffer::Load(const char *path, bool nullTerminate)
    {
        Release();

        SDL_RWops *rw = SDL_RWFromFile(path, "rb");
        if (!rw)
            return false;

        Sint64 sizeS64 = SDL_RWsize(rw);
        if (sizeS64 < 0)
        {
            SDL_RWclose(rw);
            return false;
        }

        size_t size = static_cast<size_t>(sizeS64);
        size_t allocSize = nullTerminate ? size + 1 : size;
        unsigned char *buffer = static_cast<unsigned char *>(std::malloc(allocSize > 0 ? allocSize : 1));
        if (!buffer)
        {
            SDL_RWclose(rw);
            return false;
        }

        size_t readTotal = 0;
        while (readTotal < size)
        {
            size_t got = SDL_RWread(rw, buffer + readTotal, 1, size - readTotal);
            if (got == 0)
                break;
            readTotal += got;
        }

        SDL_RWclose(rw);

        if (readTotal != size)
        {
            std::free(buffer);
            return false;
        }

        if (nullTerminate)
            buffer[size] = '\0';

        mData = buffer;
        mSize = size;
        return true;
    }

}