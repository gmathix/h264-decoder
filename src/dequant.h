//
// Created by gmathix on 4/15/26.
//

#ifndef TOY_H264_DEQUANT_H
#define TOY_H264_DEQUANT_H


#include "global.h"

#include "decoder.h"
#include "util/bitreader.h"


/* default scaling lists */

extern const int flat_4x4_16[16];
extern const int default_4x4_intra[16];
extern const int default_4x4_inter[16];

extern const int flat_8x8_16[64];
extern const int default_8x8_intra[64];
extern const int default_8x8_inter[64];

extern const int norm_adjust_4x4[6][3];
extern const int norm_adjust_8x8[6][6];


extern const uint8_t scaling_list_indices[2][2][3];





void parse_scaling_list(int *scaling_list, int size, bool *useDefault, BitReader *br);
void infer_flat_matrices(CodecContext *ctx);
void scaling_list_fallback(int index, bool is4x4, bool isPPS, CodecContext *ctx);

void precompute_4x4_scales(CodecContext *ctx);
void precompute_8x8_scales(CodecContext *ctx);



#endif //TOY_H264_DEQUANT_H