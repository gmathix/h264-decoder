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


void deblock_slice(Picture *pic, SliceHeader *sh, const Undo264Context *ctx);
void deblock_macroblock(Picture *pic, SliceHeader *sh, int mbAddr, const Undo264Context *ctx);



#endif //H264_DECODER_DEBLOCK_H