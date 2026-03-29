//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_BITREADER_H
#define TOY_H264_BITREADER_H


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    const uint8_t *data; // pointer to the byte buffer
    size_t         size; // total bytes in buffer
    size_t         byte_pos;
    size_t         bit_pos; // 0..7 within current byte
} BitReader ;



void     bitreader_init(BitReader *br, const uint8_t *data, size_t size);
uint32_t bitreader_peek_bits(BitReader *br, int n);
uint32_t bitreader_read_bits(BitReader *br, int n);
void     bitreader_skip_bits(BitReader *br, uint32_t n);
void     bitreader_rewind(BitReader *br, int n);
bool     bitreader_byte_aligned(BitReader *br);
int32_t  bitreader_bits_remaining(BitReader *br);
bool     more_rbsp_data(BitReader *br);
bool     rbsp_trailing_bits(BitReader *br);


static BitReader make_br(const uint8_t *data, size_t size) {
    BitReader br;
    bitreader_init(&br, data, size);
    return br;
}



#endif //TOY_H264_BITREADER_H