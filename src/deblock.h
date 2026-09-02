//
// Created by gmathix on 6/26/26.
//

#ifndef H264_DECODER_DEBLOCK_H
#define H264_DECODER_DEBLOCK_H



#include "picture.h"
#include "slice.h"


extern const uint8_t alpha_table[52];
extern const uint8_t beta_table[52];
extern const uint8_t treshold_table[3][52];

/* all 256*26 precomputed tc0 tables (128-bit register size)
 * tc0_tables[(bS0 << 6) + (bS1 << 4) + (bS2 << 2) + bS3][indexA] */
extern int8_t tc0_tables[256][52][16] __attribute__((aligned(16)));
extern int tc0_tables_initialized;

static always_inline int8_t *get_tc0_table(int *bS, int indexA) {
    return tc0_tables[(bS[0]<<6) + (bS[1]<<4) + (bS[2]<<2) + bS[3]][indexA];
}

void deblock_slice(Picture *pic, SliceHeader *sh, const Undo264Context *ctx);
void deblock_macroblock(Picture *pic, SliceHeader *sh, int mbAddr, const Undo264Context *ctx);
void deblock_macroblock_intra(Picture *pic, SliceHeader *sh, int mbAddr, const Undo264Context *ctx);



#endif //H264_DECODER_DEBLOCK_H