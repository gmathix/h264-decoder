//
// Created by gmathix on 4/1/26.
//

#include "vlc.h"
#include "util/formulas.h"

#include <stdlib.h>



void init_vlc(VLCTable *vlc, uint16_t size, uint16_t *lengths, uint16_t *codes, int max_bits) {
    vlc->size = size;
    vlc->codes = codes;
    vlc->lengths = lengths;
    vlc->max_bits = max_bits;
}

void init_multi_vlc(MultiVLC *mv, int num) {
    mv->tables = malloc(num * sizeof(VLCTable));
    mv->num_tables = num;
}


void set_vlc_table(MultiVLC *mv, int idx, uint16_t *lengths, uint16_t *codes, int size, int max_bits) {
    VLCTable vlc = make_vlc(size, codes, lengths, max_bits);
    mv->tables[idx] = vlc;
    build_vlc(&mv->tables[idx]);
}

void build_vlc(VLCTable *vlc) {
    vlc->lookup_size = 1 << vlc->max_bits;

    vlc->lookup_symbol = malloc(vlc->lookup_size * sizeof(uint16_t));
    vlc->lookup_length = malloc(vlc->lookup_size * sizeof(uint16_t));

    for (int i = 0; i < vlc->lookup_size; i++) {
        vlc->lookup_length[i] = 0;
    }

    for (int sym = 0; sym < vlc->size; sym++) {
        int len = vlc->lengths[sym];
        int code = vlc->codes[sym];

        if (len == 0) continue;

        int shit = vlc->max_bits - len; // typo was supposed to be shift but leaving as-is is convenient
        int start = code << shit;
        int end = start | ((1 << shit) - 1);


        for (int i = start; i <= end; i++) {
            vlc->lookup_symbol[i] = sym;
            vlc->lookup_length[i] = len;
        }
    }
}



uint16_t get_vlc_length(MultiVLC *mv, int table_idx, BitReader *br) {
    uint16_t bits = bitreader_peek_bits(br, mv->tables[table_idx].max_bits);
    return mv->tables[table_idx].lookup_length[bits];
}