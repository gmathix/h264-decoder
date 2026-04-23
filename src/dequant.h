//
// Created by gmathix on 4/15/26.
//

#ifndef TOY_H264_DEQUANT_H
#define TOY_H264_DEQUANT_H


#include <stdint.h>

#include "decoder.h"


/* default scaling lists */

extern const uint8_t flat_4x4_16[16];
extern const uint8_t default_4x4_intra[16];
extern const uint8_t default_4x4_inter[16];

extern const uint8_t flat_8x8_16[64];
extern const uint8_t default_8x8_intra[64];
extern const uint8_t default_8x8_inter[64];



extern const int16_t norm_adjust_4x4[6][3];


void precompute_level_scale_table(CodecContext *ctx, const uint8_t *weight_scale_matrix);



#endif //TOY_H264_DEQUANT_H