//
// Created by gmathix on 8/23/26.
//

#ifndef UNDO264_DSP_INIT_H
#define UNDO264_DSP_INIT_H


#include "global.h"

typedef void (*weigh_bi_func)(const uint8_t*, uint8_t*, int, int, int, int, int, int);
typedef void (*weigh_single_func)(uint8_t*, int, int, int, int);
typedef void (*chroma_interpolation_func)(const uint8_t*, uint8_t*, int, int, int);
typedef void (*qpel_func_array[16])(const uint8_t*, uint8_t*, int16_t*, int);

typedef struct DSPContext {
    weigh_bi_func             weigh_bi_funcs[NUM_BLOCK_SHAPES];
    weigh_single_func         weigh_single_funcs[NUM_BLOCK_SHAPES];
    chroma_interpolation_func chroma_interpolation_funcs[NUM_BLOCK_SHAPES];
    qpel_func_array           qpel_func_arrays[NUM_BLOCK_SHAPES];
} DSPContext ;



void dsp_init(DSPContext *dsp);




#endif //UNDO264_DSP_INIT_H