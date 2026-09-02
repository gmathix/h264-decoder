//
// Created by gmathix on 8/23/26.
//

#ifndef UNDO264_DSP_INIT_H
#define UNDO264_DSP_INIT_H


#include "global.h"
#include "mb.h"
#include "decoder.h"


typedef void (*qpel_func_array[16])(const uint8_t*, uint8_t*, int16_t*, int);


typedef struct DSPContext {
    /* deblocking filter */
    void (*deblock_edge_weak_luma_h)(uint8_t *dst, int stride, int alpha, int beta, int indexA, int *bS);
    void (*deblock_edge_weak_luma_v)(uint8_t *dst, int stride, int alpha, int beta, int indexA, int *bS);
    void (*deblock_edge_strong_luma_h)(uint8_t *dst, int stride, int alpha, int beta);
    void (*deblock_edge_strong_luma_v)(uint8_t *dst, int stride, int alpha, int beta);

    void (*deblock_edge_weak_chroma_h)(uint8_t *dst, int stride, int alpha, int beta, int indexA, int *bS);
    void (*deblock_edge_weak_chroma_v)(uint8_t *dst, int stride, int alpha, int beta, int indexA, int *bS);
    void (*deblock_edge_strong_chroma_h)(uint8_t *dst, int stride, int alpha, int beta);
    void (*deblock_edge_strong_chroma_v)(uint8_t *dst, int stride, int alpha, int beta);


    /* weighted pred */
    void (*weigh_bi_funcs[NUM_BLOCK_SHAPES])(const uint8_t *temp_bi_buf, uint8_t *dst, int stride,
                                             int logWD, int w0, int w1, int o0, int o1);
    void (*weigh_single_funcs[NUM_BLOCK_SHAPES])(uint8_t *dst, int stride, int logWD, int w, int o);


    /* chroma interpolation */
    void (*chroma_interpolation_funcs[NUM_BLOCK_SHAPES])(const uint8_t *temp_buf, uint8_t *dst, int stride, int xFrac, int yFrac);


    /* qpel */
    qpel_func_array qpel_func_arrays[NUM_BLOCK_SHAPES];


    /* transform */
    void (*transform_4x4)(Macroblock *mb, int blkIdx, const Undo264Context *ctx);
    void (*transform_8x8)(Macroblock *mb, int i8x8, const Undo264Context *ctx);
    void (*transform_16x16)(Macroblock *mb, const Undo264Context *ctx);
    void (*transform_chroma)(Macroblock *mb, const Undo264Context *ctx);
} DSPContext ;



void dsp_init(DSPContext *dsp);




#endif //UNDO264_DSP_INIT_H