//
// Created by gmathix on 4/15/26.
//

#ifndef TOY_H264_DEQUANT_H
#define TOY_H264_DEQUANT_H


#include "global.h"

#include "decoder.h"
#include "util/bitreader.h"


/* default scaling lists */

extern const int16_t flat_4x4_16[16];
extern const int16_t default_4x4_intra[16];
extern const int16_t default_4x4_inter[16];

extern const int16_t flat_8x8_16[64];
extern const int16_t default_8x8_intra[64];
extern const int16_t default_8x8_inter[64];

extern const int norm_adjust_4x4[6][3];
extern const int norm_adjust_8x8[6][6];


extern const uint8_t scaling_list_indices[2][2][3];





void parse_scaling_list(int16_t *scaling_list, int size, bool *useDefault, BitReader *br);
void infer_flat_matrices(bool seq, const Undo264Context *ctx);
void scaling_list_fallback(int index, bool is4x4, bool isPPS, const Undo264Context *ctx);

void precompute_4x4_scales(int16_t (*scalingList4x4)[16], const Undo264Context *ctx);
void precompute_8x8_scales(int16_t (*scalingList8x8)[64], const Undo264Context *ctx);



#endif //TOY_H264_DEQUANT_H