//
// Created by gmathix on 3/12/26.
//
#include "bitreader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>




void bitreader_init(struct BitReader *br, const uint8_t *data, size_t size) {
    br->data = data;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
}

uint32_t bitreader_peek_bits(BitReader *br, int n) {

    if (n < 1 || n > 32) return 0;

    uint32_t res = 0;

    int32_t total = br->size * 8;
    int32_t start = br->byte_pos*8 + br->bit_pos;


    for (int i = 0; i < n && start + i < total; i++) {
        uint32_t bit_index = start + i;
        uint32_t byte = bit_index >> 3;
        uint32_t bit = bit_index & 7;

        res <<= 1;
        res |= (br->data[byte] >> (7-bit)) & 1;
    }

    return res;
}

uint32_t bitreader_read_bits(BitReader *br, int n) {
    uint32_t res = bitreader_peek_bits(br, n);
    bitreader_skip_bits(br, n);

    return res;
}

void bitreader_skip_bits(BitReader *br, uint32_t n) {
    size_t remaining = bitreader_bits_remaining(br);

    if (n > remaining) {
        // printf("%lu\n", remaining);
        printf("bitreader overflow: requested %u, remaining %u\n", n, remaining);
        exit(1);
    }

    size_t totalBits = br->byte_pos * 8 + br->bit_pos + n;
    br->byte_pos = totalBits / 8;
    br->bit_pos = totalBits % 8;
}

void bitreader_rewind(BitReader *br, int n) {
    size_t before = br->byte_pos * 8 + br->bit_pos;

    if ((size_t)n > before) n = before;

    size_t new_pos_bits = before - n;
    br->byte_pos = new_pos_bits / 8;
    br->bit_pos = new_pos_bits % 8;
}

bool bitreader_byte_aligned(BitReader *br) {
    return br->bit_pos == 0;
}

size_t bitreader_bits_remaining(BitReader *br) {
    if (br->byte_pos >= br->size) return 0;
    return (br->size - br->byte_pos) * 8 - br->bit_pos;
}

size_t bitreader_bits_consumed(BitReader *br) {
    return br->byte_pos*8 + br->bit_pos;
}

bool more_rbsp_data(BitReader *br) {
    return bitreader_bits_remaining(br) > 0 && !rbsp_trailing_bits(br);
}

bool rbsp_trailing_bits(BitReader *br) {
    int32_t rem = bitreader_bits_remaining(br);
    // printf("\n   %d remaining\n", rem);
    // printf("%d\n", bitreader_peek_bits(br, 10));

    if (rem <= 0) return false;

    // must start with '1'
    if (bitreader_peek_bits(br, 1) != 1)
        return false;

    // after that, ALL remaining bits must be zero
    for (int i = 1; i < rem; i++) {
        if (bitreader_peek_bits(br, i+1) & 1) {
            return false;
        }
    }

    return true;
}