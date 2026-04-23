//
// Created by gmathix on 4/1/26.
//

#ifndef TOY_H264_VLC_H
#define TOY_H264_VLC_H


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "global.h"
#include "util/bitreader.h"


typedef struct VLCTable {
    uint16_t  size;
    uint16_t  max_bits;
    uint16_t *codes;
    uint16_t *lengths;

    int       lookup_size;
    uint16_t *lookup_symbol;
    uint16_t *lookup_length;
} VLCTable ;


typedef struct MultiVLC {
    VLCTable *tables;
    int num_tables;
} MultiVLC;



void init_vlc(VLCTable *vlc, uint16_t size, uint16_t *lengths, uint16_t *codes, int max_bits);
void init_multi_vlc(MultiVLC *mv, int num);

void set_vlc_table(MultiVLC *mv, int idx, uint16_t *lengths, uint16_t *codes, int size, int max_bits);
void build_vlc(VLCTable *vlc);

uint16_t get_vlc_length(MultiVLC *mv, int table_idx, BitReader *br);


static ALWAYS_INLINE uint16_t get_vlc(const MultiVLC *mv, int table_idx, BitReader *br) {

    uint16_t bits = bitreader_peek_bits(br, mv->tables[table_idx].max_bits);

    if (mv->tables[table_idx].max_bits > bitreader_bits_remaining(br)) {
        bits <<= (mv->tables[table_idx].max_bits - bitreader_bits_remaining(br));
    }

    int sym = mv->tables[table_idx].lookup_symbol[bits];
    int len = mv->tables[table_idx].lookup_length[bits];
    bitreader_skip_bits(br, len);

    return sym;
}


static VLCTable make_vlc(uint16_t size,uint16_t *codes, uint16_t *lengths, int max_bits) {
    VLCTable vlc;
    init_vlc(&vlc, size, lengths, codes, max_bits);
    return vlc;
}

static MultiVLC make_mutli_vlc(int num_tables) {
    MultiVLC mv;
    init_multi_vlc(&mv, num_tables);
    return mv;
}



#endif //TOY_H264_VLC_H