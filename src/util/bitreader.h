//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_BITREADER_H
#define TOY_H264_BITREADER_H

#include "../global.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct BitReader {
    const uint8_t *data; // pointer to the byte buffer
    size_t         size; // total bytes in buffer
    size_t         byte_pos;
    uint64_t       bits_cache;
    size_t         cache_pos;
} BitReader ;




static always_inline void bitreader_init(BitReader *br, const uint8_t *data, size_t size);
static always_inline BitReader make_br(const uint8_t *data, size_t size);


/*
 * load 4 bytes located 4 bytes after the current byte into the left part of bits_cache
 */
static always_inline int bitreader_refill_half(BitReader *br) {
    br->bits_cache <<= 32;

    if (br->size - br->byte_pos >= (size_t)4) {
        uint32_t v;
        memcpy(&v, &br->data[br->byte_pos + 4], 4);
        br->bits_cache |= __builtin_bswap32(v);
        return 4;
    }

    int i;
    for (i = 0; br->byte_pos+4+i < br->size && i < 4; i++) {
        br->bits_cache |= (uint64_t) br->data[br->byte_pos + 4 + i] << ((3-i) * 8);
    }
    return i;
}

/*
 * load 8 bytes into bits_cache
 */
static always_inline int bitreader_refill(BitReader *br) {
    if (br->size - br->byte_pos >= (size_t)8) {
        uint64_t v;
        memcpy(&v, &br->data[br->byte_pos], 8);
        br->bits_cache = __builtin_bswap64(v); // little-endian to big-endian (no-op on big-endian machines)
        return 8;
    }

    br->bits_cache = 0;
    int i;
    for (i = 0; br->byte_pos + i < br->size; i++) {
        int shift = (7-i) * 8;
        br->bits_cache |= (uint64_t) br->data[br->byte_pos + i] << shift;
    }
    return i;
}

/*
 * guaranteed to always have enough bits in the cache, since the cache it 64 bits and peek_bits returns 32 bits max
 */
static always_inline uint32_t bitreader_peek_bits(BitReader *br, size_t n) {
    if (n < 1 || n > 32) return 0;

    // shift left part of bits_cache to the right 32 bits, then mask the number of bits to peek
    return (uint32_t) (br->bits_cache >> (64 - br->cache_pos - n)) & (((uint64_t)1<<n) - 1);
}

static always_inline size_t bitreader_bits_remaining(BitReader *br) {
    if (br->byte_pos >= br->size) return 0;
    return (br->size - br->byte_pos) * 8 + br->cache_pos;
}


static always_inline void bitreader_skip_bits(BitReader *br, size_t n) {
    size_t remaining = bitreader_bits_remaining(br);

    if (n > remaining) {
        printf("bitreader overflow: requested %lu, remaining %lu\n", n, remaining);
        exit(42);
    }


    br->cache_pos += n;

    if (br->cache_pos >= 32) {
        br->byte_pos += 4;
        bitreader_refill_half(br);
        br->cache_pos = br->cache_pos % 32;
    }
}

static always_inline uint32_t bitreader_read_bits(BitReader *br, size_t n) {
    uint32_t res = bitreader_peek_bits(br, n);
    bitreader_skip_bits(br, n);

    return res;
}

static always_inline void bitreader_rewind(BitReader *br, size_t n) {
    size_t before = br->byte_pos * 8 + br->cache_pos;

    if (n > before) n = before;

    size_t new_pos_bits = before - n;
    br->byte_pos = new_pos_bits / 8;

    bitreader_refill(br);

    br->cache_pos = new_pos_bits % 8;
}

static always_inline bool bitreader_byte_aligned(BitReader *br) {
    return br->cache_pos % 8 == 0;
}

static always_inline size_t bitreader_bits_consumed(BitReader *br) {
    return br->byte_pos*8 + br->cache_pos;
}

static always_inline bool rbsp_trailing_bits(BitReader *br) {
    size_t rem = bitreader_bits_remaining(br);

    if (rem <= 0) return false;

    // must start with '1'
    if (bitreader_peek_bits(br, 1) != 1)
        return false;

    // after that, ALL remaining bits must be zero
    for (size_t i = 1; i < rem; i++) {
        if (bitreader_peek_bits(br, i+1) & 1) {
            return false;
        }
    }

    return true;
}

static always_inline bool more_rbsp_data(BitReader *br) {
    return bitreader_bits_remaining(br) > 0 && !rbsp_trailing_bits(br);
}




static always_inline void bitreader_init(struct BitReader *br, const uint8_t *data, size_t size) {
    br->data = data;
    br->size = size;
    br->byte_pos = 0;
    br->cache_pos = 0;
    br->bits_cache = 0;
    bitreader_refill(br);
}

static always_inline BitReader make_br(const uint8_t *data, size_t size) {
    BitReader br;
    bitreader_init(&br, data, size);
    return br;
}


#endif //TOY_H264_BITREADER_H