//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_SLICE_H
#define TOY_H264_SLICE_H


#include "util/bitreader.h"
#include "common.h"
#include "ps.h"


#define SLICE_P            0
#define SLICE_B            1
#define SLICE_I            2
#define SLICE_SP           3
#define SLICE_SI           4
#define SLICE_P_BIS        5
#define SLICE_B_BIS        6
#define SLICE_I_BIS        7
#define SLICE_SP_BIS       8
#define SLICE_SI_BIS       9

#define IS_P_SLICE(a)  (((a) == SLICE_P) || ((a) == SLICE_P_BIS))
#define IS_B_SLICE(a)  (((a) == SLICE_B) || ((a) == SLICE_B_BIS))
#define IS_I_SLICE(a)  (((a) == SLICE_I) || ((a) == SLICE_I_BIS))
#define IS_SP_SLICE(a) (((a) == SLICE_SP) || ((a) == SLICE_SP_BIS))
#define IS_SI_SLICE(a) (((a) == SLICE_SI) || ((a) == SLICE_SI_BIS))



void decode_slice_header(NalUnit *nal_unit, BitReader *br, ParamSets *ps);


#endif //TOY_H264_SLICE_H