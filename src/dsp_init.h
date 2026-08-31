//
// Created by gmathix on 8/23/26.
//

#ifndef UNDO264_DSP_INIT_H
#define UNDO264_DSP_INIT_H


#include "global.h"
#include "mb.h"
#include "decoder.h"

typedef void (*weigh_bi_func)(const uint8_t*, uint8_t*, int, int, int, int, int, int);
typedef void (*weigh_single_func)(uint8_t*, int, int, int, int);
typedef void (*chroma_interpolation_func)(const uint8_t*, uint8_t*, int, int, int);
typedef void (*qpel_func_array[16])(const uint8_t*, uint8_t*, int16_t*, int);
typedef void (*transform_4x4_func)(Macroblock*, int, const Undo264Context*);
typedef void (*transform_8x8_func)(Macroblock*, int, const Undo264Context*);
typedef void (*transform_16x16_func)(Macroblock*, const Undo264Context*);
typedef void (*transform_chroma_func)(Macroblock*, const Undo264Context*);
typedef void (*deblock_low_bs_func)(uint8_t*, int, int, int, int, int, int*);
typedef void (*deblock_high_bs_func)(uint8_t*, int, int, int, int);

typedef struct DSPContext {
    weigh_bi_func             weigh_bi_funcs[NUM_BLOCK_SHAPES];
    weigh_single_func         weigh_single_funcs[NUM_BLOCK_SHAPES];
    chroma_interpolation_func chroma_interpolation_funcs[NUM_BLOCK_SHAPES];
    qpel_func_array           qpel_func_arrays[NUM_BLOCK_SHAPES];
    transform_4x4_func        transform_4x4;
    transform_8x8_func        transform_8x8;
    transform_16x16_func      transform_16x16;
    transform_chroma_func     transform_chroma;
    deblock_low_bs_func       deblock_edge_low_bs_luma;
    deblock_low_bs_func       deblock_edge_low_bs_chroma;
    deblock_high_bs_func      deblock_edge_high_bs_luma;
    deblock_high_bs_func      deblock_edge_high_bs_chroma;
} DSPContext ;



void dsp_init(DSPContext *dsp);




#endif //UNDO264_DSP_INIT_H