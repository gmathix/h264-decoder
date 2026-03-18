//
// Created by gmathix on 3/18/26.
//

#ifndef TOY_H264_EXPGOLOMB_H
#define TOY_H264_EXPGOLOMB_H


#include <stdint.h>

#include "bitreader.h"

uint32_t read_u(BitReader *br, int n);
uint32_t read_ue(BitReader *br);
uint32_t read_te(BitReader *br, int max);
uint32_t map_coded_block_pattern(uint32_t codeNum, int chromaArrayType, int isIntra);
int32_t  read_se(BitReader *br);





#endif //TOY_H264_EXPGOLOMB_H