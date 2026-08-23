
#ifndef MSF_GIF_H
#define MSF_GIF_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    void * data;
    size_t dataSize;

    size_t allocSize; 
    void * contextPointer; 
} MsfGifResult;

typedef struct { 
    uint32_t * pixels;
    int depth, count, rbits, gbits, bbits;
} MsfCookedFrame;

typedef struct MsfGifBuffer { 
    struct MsfGifBuffer * next;
    size_t size;
    uint8_t data[1];
} MsfGifBuffer;

typedef size_t (* MsfGifFileWriteFunc) (const void * buffer, size_t size, size_t count, void * stream);
typedef struct { 
    MsfGifFileWriteFunc fileWriteFunc;
    void * fileWriteData;
    MsfCookedFrame previousFrame;
    MsfCookedFrame currentFrame;
    int16_t * lzwMem;
    uint8_t * tlbMem;
    uint8_t * usedMem;
    MsfGifBuffer * listHead;
    MsfGifBuffer * listTail;
    int width, height;
    void * customAllocatorContext;
    int framesSubmitted; 
} MsfGifState;

#ifdef __cplusplus
extern "C" {
#endif 

int msf_gif_begin(MsfGifState * handle, int width, int height);

int msf_gif_frame(MsfGifState * handle, uint8_t * pixelData, int centiSecondsPerFrame, int quality, int pitchInBytes);

MsfGifResult msf_gif_end(MsfGifState * handle);

void msf_gif_free(MsfGifResult result);

extern int msf_gif_alpha_threshold;

extern int msf_gif_bgra_flag;

int msf_gif_begin_to_file(MsfGifState * handle, int width, int height, MsfGifFileWriteFunc func, void * filePointer);
int msf_gif_frame_to_file(MsfGifState * handle, uint8_t * pixelData, int centiSecondsPerFrame, int quality, int pitchInBytes);
int msf_gif_end_to_file(MsfGifState * handle); 

#ifdef __cplusplus
}
#endif 

#endif 

#ifdef MSF_GIF_IMPL
#ifndef MSF_GIF_ALREADY_IMPLEMENTED_IN_THIS_TRANSLATION_UNIT
#define MSF_GIF_ALREADY_IMPLEMENTED_IN_THIS_TRANSLATION_UNIT

#if defined(MSF_GIF_MALLOC) && defined(MSF_GIF_REALLOC) && defined(MSF_GIF_FREE) 
#elif !defined(MSF_GIF_MALLOC) && !defined(MSF_GIF_REALLOC) && !defined(MSF_GIF_FREE) 
#else
#error "You must either define all of MSF_GIF_MALLOC, MSF_GIF_REALLOC, and MSF_GIF_FREE, or define none of them"
#endif

#if !defined(MSF_GIF_MALLOC)
#include <stdlib.h> 
#define MSF_GIF_MALLOC(contextPointer, newSize) malloc(newSize)
#define MSF_GIF_REALLOC(contextPointer, oldMemory, oldSize, newSize) realloc(oldMemory, newSize)
#define MSF_GIF_FREE(contextPointer, oldMemory, oldSize) free(oldMemory)
#endif

#ifdef MSF_GIF_ENABLE_TRACING
#define MsfTimeFunc TimeFunc
#define MsfTimeLoop TimeLoop
#define msf_init_profiling_thread init_profiling_thread
#else
#define MsfTimeFunc
#define MsfTimeLoop(name)
#define msf_init_profiling_thread()
#endif 

#include <string.h> 

#if defined(__GNUC__) 
static inline int msf_bit_log(int i) { return 32 - __builtin_clz(i); }
#elif defined(_MSC_VER) 
#include <intrin.h>
static inline int msf_bit_log(int i) { unsigned long idx; _BitScanReverse(&idx, i); return idx + 1; }
#else 

static inline int msf_bit_log(int i) {
    static const int MultiplyDeBruijnBitPosition[32] = {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31,
    };
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    return MultiplyDeBruijnBitPosition[(uint32_t)(i * 0x07C4ACDDU) >> 27] + 1;
}
#endif
static inline int msf_imin(int a, int b) { return a < b? a : b; }
static inline int msf_imax(int a, int b) { return b < a? a : b; }

#if (defined (__SSE2__) || defined (_M_X64) || _M_IX86_FP == 2) && !defined(MSF_GIF_NO_SSE2)
#include <emmintrin.h>
#endif

int msf_gif_alpha_threshold = 0;
int msf_gif_bgra_flag = 0;

static void msf_cook_frame(MsfCookedFrame * frame, uint8_t * raw, uint8_t * used,
                           int width, int height, int pitch, int depth)
{ MsfTimeFunc

    const static int rdepthsArray[17] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5 };
    const static int gdepthsArray[17] = { 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6 };
    const static int bdepthsArray[17] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5 };

    const int * rdepths = msf_gif_bgra_flag? bdepthsArray : rdepthsArray;
    const int * gdepths =                                   gdepthsArray;
    const int * bdepths = msf_gif_bgra_flag? rdepthsArray : bdepthsArray;

    const static int ditherKernel[16] = {
         0 << 12,  8 << 12,  2 << 12, 10 << 12,
        12 << 12,  4 << 12, 14 << 12,  6 << 12,
         3 << 12, 11 << 12,  1 << 12,  9 << 12,
        15 << 12,  7 << 12, 13 << 12,  5 << 12,
    };

    uint32_t * cooked = frame->pixels;
    int count = 0;
    MsfTimeLoop("do") do {
        int rbits = rdepths[depth], gbits = gdepths[depth], bbits = bdepths[depth];
        int paletteSize = (1 << (rbits + gbits + bbits)) + 1;
        memset(used, 0, paletteSize * sizeof(uint8_t));

        int rdiff = (1 << (8 - rbits)) - 1;
        int gdiff = (1 << (8 - gbits)) - 1;
        int bdiff = (1 << (8 - bbits)) - 1;
        short rmul = (short) ((255.0f - rdiff) / 255.0f * 257);
        short gmul = (short) ((255.0f - gdiff) / 255.0f * 257);
        short bmul = (short) ((255.0f - bdiff) / 255.0f * 257);

        int gmask = ((1 << gbits) - 1) << rbits;
        int bmask = ((1 << bbits) - 1) << rbits << gbits;

        MsfTimeLoop("cook") for (int y = 0; y < height; ++y) {
            int x = 0;

            #if (defined (__SSE2__) || defined (_M_X64) || _M_IX86_FP == 2) && !defined(MSF_GIF_NO_SSE2)
                __m128i k = _mm_loadu_si128((__m128i *) &ditherKernel[(y & 3) * 4]);
                __m128i k2 = _mm_or_si128(_mm_srli_epi32(k, rbits), _mm_slli_epi32(_mm_srli_epi32(k, bbits), 16));
                for (; x < width - 3; x += 4) {
                    uint8_t * pixels = &raw[y * pitch + x * 4];
                    __m128i p = _mm_loadu_si128((__m128i *) pixels);

                    __m128i rb = _mm_and_si128(p, _mm_set1_epi32(0x00FF00FF));
                    __m128i rb1 = _mm_mullo_epi16(rb, _mm_set_epi16(bmul, rmul, bmul, rmul, bmul, rmul, bmul, rmul));
                    __m128i rb2 = _mm_adds_epu16(rb1, k2);
                    __m128i r3 = _mm_srli_epi32(_mm_and_si128(rb2, _mm_set1_epi32(0x0000FFFF)), 16 - rbits);
                    __m128i b3 = _mm_and_si128(_mm_srli_epi32(rb2, 32 - rbits - gbits - bbits), _mm_set1_epi32(bmask));

                    __m128i g = _mm_and_si128(_mm_srli_epi32(p, 8), _mm_set1_epi32(0x000000FF));
                    __m128i g1 = _mm_mullo_epi16(g, _mm_set1_epi32(gmul));
                    __m128i g2 = _mm_adds_epu16(g1, _mm_srli_epi32(k, gbits));
                    __m128i g3 = _mm_and_si128(_mm_srli_epi32(g2, 16 - rbits - gbits), _mm_set1_epi32(gmask));

                    __m128i out = _mm_or_si128(_mm_or_si128(r3, g3), b3);

                    __m128i invAlphaMask = _mm_cmplt_epi32(_mm_srli_epi32(p, 24), _mm_set1_epi32(msf_gif_alpha_threshold));
                    out = _mm_or_si128(_mm_and_si128(invAlphaMask, _mm_set1_epi32(paletteSize - 1)), _mm_andnot_si128(invAlphaMask, out));

                    uint32_t * c = &cooked[y * width + x];
                    _mm_storeu_si128((__m128i *) c, out);
                }
            #endif

            for (; x < width; ++x) {
                uint8_t * p = &raw[y * pitch + x * 4];

                if (p[3] < msf_gif_alpha_threshold) {
                    cooked[y * width + x] = paletteSize - 1;
                    continue;
                }

                int dx = x & 3, dy = y & 3;
                int k = ditherKernel[dy * 4 + dx];
                cooked[y * width + x] =
                    (msf_imin(65535, p[2] * bmul + (k >> bbits)) >> (16 - rbits - gbits - bbits) & bmask) |
                    (msf_imin(65535, p[1] * gmul + (k >> gbits)) >> (16 - rbits - gbits        ) & gmask) |
                     msf_imin(65535, p[0] * rmul + (k >> rbits)) >> (16 - rbits                );
            }
        }

        count = 0;
        MsfTimeLoop("mark") for (int i = 0; i < width * height; ++i) {
            used[cooked[i]] = 1;
        }

        MsfTimeLoop("count") for (int j = 0; j < paletteSize - 1; ++j) {
            count += used[j];
        }
    } while (count >= 256 && --depth);

    MsfCookedFrame ret = { cooked, depth, count, rdepths[depth], gdepths[depth], bdepths[depth] };
    *frame = ret;
}

static inline void msf_put_code(uint8_t * * writeHead, uint32_t * blockBits, int len, uint32_t code) {

    int idx = *blockBits / 8;
    int bit = *blockBits % 8;
    (*writeHead)[idx + 0] |= code <<       bit ;
    (*writeHead)[idx + 1] |= code >> ( 8 - bit);
    (*writeHead)[idx + 2] |= code >> (16 - bit);
    *blockBits += len;

    if (*blockBits >= 256 * 8) {
        *blockBits -= 255 * 8;
        (*writeHead) += 256;
        (*writeHead)[2] = (*writeHead)[1];
        (*writeHead)[1] = (*writeHead)[0];
        (*writeHead)[0] = 255;
        memset((*writeHead) + 4, 0, 256);
    }
}

typedef struct {
    int16_t * data;
    int len;
    int stride;
} MsfStridedList;

static inline void msf_lzw_reset(MsfStridedList * lzw, int tableSize, int stride) { MsfTimeFunc
    memset(lzw->data, 0xFF, 4096 * stride * sizeof(int16_t));
    lzw->len = tableSize + 2;
    lzw->stride = stride;
}

static MsfGifBuffer * msf_compress_frame(void * allocContext, int width, int height, int centiSeconds,
                                         MsfCookedFrame frame, MsfGifState * handle, uint8_t * used, uint8_t * tlb, int16_t * lzwMem)
{ MsfTimeFunc

    int maxBufSize = offsetof(MsfGifBuffer, data) + 32 + 256 * 3 + width * height * 3 / 2 + (256 + 4);
    MsfGifBuffer * buffer = (MsfGifBuffer *) MSF_GIF_MALLOC(allocContext, maxBufSize);
    if (!buffer) { return NULL; }
    uint8_t * writeHead = buffer->data;
    MsfStridedList lzw = { lzwMem };

    int totalBits = frame.rbits + frame.gbits + frame.bbits;
    int tlbSize = (1 << totalBits) + 1;

    typedef struct { uint8_t r, g, b; } Color3;
    Color3 table[256] = { {0} };
    int tableIdx = 1; 

    tlb[tlbSize-1] = 0;
    MsfTimeLoop("table") for (int i = 0; i < tlbSize-1; ++i) {
        if (used[i]) {
            tlb[i] = tableIdx;
            int rmask = (1 << frame.rbits) - 1;
            int gmask = (1 << frame.gbits) - 1;

            int r = i & rmask;
            int g = i >> frame.rbits & gmask;
            int b = i >> (frame.rbits + frame.gbits);

            r <<= 8 - frame.rbits;
            g <<= 8 - frame.gbits;
            b <<= 8 - frame.bbits;
            table[tableIdx].r = r | r >> frame.rbits | r >> (frame.rbits * 2) | r >> (frame.rbits * 3);
            table[tableIdx].g = g | g >> frame.gbits | g >> (frame.gbits * 2) | g >> (frame.gbits * 3);
            table[tableIdx].b = b | b >> frame.bbits | b >> (frame.bbits * 2) | b >> (frame.bbits * 3);
            if (msf_gif_bgra_flag) {
                uint8_t temp = table[tableIdx].r;
                table[tableIdx].r = table[tableIdx].b;
                table[tableIdx].b = temp;
            }
            ++tableIdx;
        }
    }
    int hasTransparentPixels = used[tlbSize-1];

    int tableBits = msf_imax(2, msf_bit_log(tableIdx - 1));
    int tableSize = 1 << tableBits;

    MsfCookedFrame previous = handle->previousFrame;
    int hasSamePal = frame.rbits == previous.rbits && frame.gbits == previous.gbits && frame.bbits == previous.bbits;
    int framesCompatible = hasSamePal && !hasTransparentPixels;

    char headerBytes[19] = "\x21\xF9\x04\x05\0\0\0\0" "\x2C\0\0\0\0\0\0\0\0\x80";

    if (hasTransparentPixels && handle->framesSubmitted > 0) {
        handle->listTail->data[3] = 0x09; 
    }
    memcpy(&headerBytes[4], &centiSeconds, 2);
    memcpy(&headerBytes[13], &width, 2);
    memcpy(&headerBytes[15], &height, 2);
    headerBytes[17] |= tableBits - 1;
    memcpy(writeHead, headerBytes, 18);
    writeHead += 18;

    memcpy(writeHead, table, tableSize * sizeof(Color3));
    writeHead += tableSize * sizeof(Color3);
    *writeHead++ = tableBits;

    memset(writeHead, 0, 256 + 4); 
    writeHead[0] = 255;
    uint32_t blockBits = 8; 

    msf_lzw_reset(&lzw, tableSize, tableIdx);
    msf_put_code(&writeHead, &blockBits, msf_bit_log(lzw.len - 1), tableSize);

    int lastCode = framesCompatible && frame.pixels[0] == previous.pixels[0]? 0 : tlb[frame.pixels[0]];
    MsfTimeLoop("compress") for (int i = 1; i < width * height; ++i) {

        int color = framesCompatible && frame.pixels[i] == previous.pixels[i]? 0 : tlb[frame.pixels[i]];
        int code = (&lzw.data[lastCode * lzw.stride])[color];
        if (code < 0) {

            int codeBits = msf_bit_log(lzw.len - 1);
            msf_put_code(&writeHead, &blockBits, codeBits, lastCode);

            if (lzw.len > 4095) {

                msf_put_code(&writeHead, &blockBits, codeBits, tableSize);
                msf_lzw_reset(&lzw, tableSize, tableIdx);
            } else {
                (&lzw.data[lastCode * lzw.stride])[color] = lzw.len;
                ++lzw.len;
            }

            lastCode = color;
        } else {
            lastCode = code;
        }
    }

    msf_put_code(&writeHead, &blockBits, msf_imin(12, msf_bit_log(lzw.len - 1)), lastCode);
    msf_put_code(&writeHead, &blockBits, msf_imin(12, msf_bit_log(lzw.len)), tableSize + 1);

    if (blockBits > 8) {
        int bytes = (blockBits + 7) / 8; 
        writeHead[0] = bytes - 1;
        writeHead += bytes;
    }
    *writeHead++ = 0; 

    buffer->next = NULL;
    buffer->size = writeHead - buffer->data;
    MsfGifBuffer * moved =
        (MsfGifBuffer *) MSF_GIF_REALLOC(allocContext, buffer, maxBufSize, offsetof(MsfGifBuffer, data) + buffer->size);
    if (!moved) { MSF_GIF_FREE(allocContext, buffer, maxBufSize); return NULL; }
    return moved;
}

static const int lzwAllocSize = 4096 * 256 * sizeof(int16_t);
static const int tlbAllocSize = ((1 << 16) + 1) * sizeof(uint8_t);
static const int usedAllocSize = ((1 << 16) + 1) * sizeof(uint8_t);

static void msf_free_gif_state(MsfGifState * handle) {
    if (handle->previousFrame.pixels) MSF_GIF_FREE(handle->customAllocatorContext, handle->previousFrame.pixels,
                                                   handle->width * handle->height * sizeof(uint32_t));
    if (handle->currentFrame.pixels)  MSF_GIF_FREE(handle->customAllocatorContext, handle->currentFrame.pixels,
                                                   handle->width * handle->height * sizeof(uint32_t));
    if (handle->lzwMem) MSF_GIF_FREE(handle->customAllocatorContext, handle->lzwMem, lzwAllocSize);
    if (handle->tlbMem) MSF_GIF_FREE(handle->customAllocatorContext, handle->tlbMem, tlbAllocSize);
    if (handle->usedMem) MSF_GIF_FREE(handle->customAllocatorContext, handle->usedMem, usedAllocSize);
    for (MsfGifBuffer * node = handle->listHead; node;) {
        MsfGifBuffer * next = node->next; 
        MSF_GIF_FREE(handle->customAllocatorContext, node, offsetof(MsfGifBuffer, data) + node->size);
        node = next;
    }
    handle->listHead = NULL; 
}

int msf_gif_begin(MsfGifState * handle, int width, int height) { MsfTimeFunc

    const int MAX_PIXELS = 268435456; 
    if (width < 1 || height < 1 || width > 65535 || height > 65535 || width >= MAX_PIXELS / height) {
        handle->listHead = NULL; 
        return 0;
    }

    MsfCookedFrame empty = {0}; 
    handle->previousFrame = empty;
    handle->currentFrame = empty;
    handle->width = width;
    handle->height = height;
    handle->framesSubmitted = 0;

    handle->lzwMem = (int16_t *) MSF_GIF_MALLOC(handle->customAllocatorContext, lzwAllocSize);
    handle->tlbMem = (uint8_t *) MSF_GIF_MALLOC(handle->customAllocatorContext, tlbAllocSize);
    handle->usedMem = (uint8_t *) MSF_GIF_MALLOC(handle->customAllocatorContext, usedAllocSize);
    handle->previousFrame.pixels =
        (uint32_t *) MSF_GIF_MALLOC(handle->customAllocatorContext, handle->width * handle->height * sizeof(uint32_t));
    handle->currentFrame.pixels =
        (uint32_t *) MSF_GIF_MALLOC(handle->customAllocatorContext, handle->width * handle->height * sizeof(uint32_t));

    handle->listHead = (MsfGifBuffer *) MSF_GIF_MALLOC(handle->customAllocatorContext, offsetof(MsfGifBuffer, data) + 32);
    if (!handle->listHead || !handle->lzwMem || !handle->previousFrame.pixels || !handle->currentFrame.pixels) {
        msf_free_gif_state(handle);
        return 0;
    }
    handle->listTail = handle->listHead;
    handle->listHead->next = NULL;
    handle->listHead->size = 32;

    char headerBytes[33] = "GIF89a\0\0\0\0\x70\0\0" "\x21\xFF\x0BNETSCAPE2.0\x03\x01\0\0\0";
    memcpy(&headerBytes[6], &width, 2);
    memcpy(&headerBytes[8], &height, 2);
    memcpy(handle->listHead->data, headerBytes, 32);
    return 1;
}

int msf_gif_frame(MsfGifState * handle, uint8_t * pixelData, int centiSecondsPerFrame, int quality, int pitchInBytes)
{ MsfTimeFunc
    if (!handle->listHead) { return 0; }

    quality = msf_imax(1, msf_imin(16, quality));
    if (pitchInBytes == 0) pitchInBytes = handle->width * 4;
    if (pitchInBytes < 0) pixelData -= pitchInBytes * (handle->height - 1);

    msf_cook_frame(&handle->currentFrame, pixelData, handle->usedMem, handle->width, handle->height, pitchInBytes,
        msf_imin(quality, handle->previousFrame.depth + 160 / msf_imax(1, handle->previousFrame.count)));

    MsfGifBuffer * buffer = msf_compress_frame(handle->customAllocatorContext, handle->width, handle->height,
        centiSecondsPerFrame, handle->currentFrame, handle, handle->usedMem, handle->tlbMem, handle->lzwMem);
    if (!buffer) { msf_free_gif_state(handle); return 0; }
    handle->listTail->next = buffer;
    handle->listTail = buffer;

    MsfCookedFrame tmp = handle->previousFrame;
    handle->previousFrame = handle->currentFrame;
    handle->currentFrame = tmp;

    handle->framesSubmitted += 1;
    return 1;
}

MsfGifResult msf_gif_end(MsfGifState * handle) { MsfTimeFunc
    if (!handle->listHead) { MsfGifResult empty = {0}; return empty; }

    size_t total = 1; 
    for (MsfGifBuffer * node = handle->listHead; node; node = node->next) { total += node->size; }

    uint8_t * buffer = (uint8_t *) MSF_GIF_MALLOC(handle->customAllocatorContext, total);
    if (buffer) {
        uint8_t * writeHead = buffer;
        for (MsfGifBuffer * node = handle->listHead; node; node = node->next) {
            memcpy(writeHead, node->data, node->size);
            writeHead += node->size;
        }
        *writeHead++ = 0x3B;
    }

    msf_free_gif_state(handle);

    MsfGifResult ret = { buffer, total, total, handle->customAllocatorContext };
    return ret;
}

void msf_gif_free(MsfGifResult result) { MsfTimeFunc
    if (result.data) { MSF_GIF_FREE(result.contextPointer, result.data, result.allocSize); }
}

int msf_gif_begin_to_file(MsfGifState * handle, int width, int height, MsfGifFileWriteFunc func, void * filePointer) {
    handle->fileWriteFunc = func;
    handle->fileWriteData = filePointer;
    return msf_gif_begin(handle, width, height);
}

int msf_gif_frame_to_file(MsfGifState * handle, uint8_t * pixelData, int centiSecondsPerFrame, int quality, int pitchInBytes) {
    if (!msf_gif_frame(handle, pixelData, centiSecondsPerFrame, quality, pitchInBytes)) { return 0; }

    MsfGifBuffer * head = handle->listHead;
    if (!handle->fileWriteFunc(head->data, head->size, 1, handle->fileWriteData)) { msf_free_gif_state(handle); return 0; }
    handle->listHead = head->next;
    MSF_GIF_FREE(handle->customAllocatorContext, head, offsetof(MsfGifBuffer, data) + head->size);
    return 1;
}

int msf_gif_end_to_file(MsfGifState * handle) {

    MsfGifResult result = msf_gif_end(handle);
    int ret = (int) handle->fileWriteFunc(result.data, result.dataSize, 1, handle->fileWriteData);
    msf_gif_free(result);
    return ret;
}

#endif 
#endif 