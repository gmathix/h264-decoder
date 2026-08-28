//
// Created by gmathix on 8/23/26.
//

#include "dsp_init.h"


#define HEIGHT 16
#include "sse4/weighted_pred_sse4_template.c"
#undef HEIGHT
#define HEIGHT 8
#include "sse4/weighted_pred_sse4_template.c"
#include "sse4/chroma_inter_sse4_template.c"
#undef HEIGHT
#define HEIGHT 4
#include "sse4/weighted_pred_sse4_template.c"
#include "sse4/chroma_inter_sse4_template.c"
#undef HEIGHT
#define HEIGHT 2
#include "sse4/weighted_pred_sse4_template.c"
#include "sse4/chroma_inter_sse4_template.c"
#undef HEIGHT

#include "sse4/transform_sse4.c"


static void dsp_init_x86(DSPContext *dsp) {
    __builtin_cpu_init();

    if (__builtin_cpu_supports("sse4.1")) {
        dsp->weigh_bi_funcs[BLOCK_16x16] = weigh_bi_sse_16x16;
        dsp->weigh_bi_funcs[BLOCK_16x8]  = weigh_bi_sse_16x8;
        dsp->weigh_bi_funcs[BLOCK_8x16]  = weigh_bi_sse_8x16;
        dsp->weigh_bi_funcs[BLOCK_8x8]   = weigh_bi_sse_8x8;
        dsp->weigh_bi_funcs[BLOCK_8x4]   = weigh_bi_sse_8x4;
        dsp->weigh_bi_funcs[BLOCK_4x8]   = weigh_bi_sse_4x8;
        dsp->weigh_bi_funcs[BLOCK_4x4]   = weigh_bi_sse_4x4;
        dsp->weigh_bi_funcs[BLOCK_4x2]   = weigh_bi_sse_4x2;
        dsp->weigh_bi_funcs[BLOCK_2x4]   = weigh_bi_sse_2x4;
        dsp->weigh_bi_funcs[BLOCK_2x2]   = weigh_bi_sse_2x2;

        dsp->weigh_single_funcs[BLOCK_16x16] = weigh_single_sse_16x16;
        dsp->weigh_single_funcs[BLOCK_16x8]  = weigh_single_sse_16x8;
        dsp->weigh_single_funcs[BLOCK_8x16]  = weigh_single_sse_8x16;
        dsp->weigh_single_funcs[BLOCK_8x8]   = weigh_single_sse_8x8;
        dsp->weigh_single_funcs[BLOCK_8x4]   = weigh_single_sse_8x4;
        dsp->weigh_single_funcs[BLOCK_4x8]   = weigh_single_sse_4x8;
        dsp->weigh_single_funcs[BLOCK_4x4]   = weigh_single_sse_4x4;
        dsp->weigh_single_funcs[BLOCK_4x2]   = weigh_single_sse_4x2;
        dsp->weigh_single_funcs[BLOCK_2x4]   = weigh_single_sse_2x4;
        dsp->weigh_single_funcs[BLOCK_2x2]   = weigh_single_sse_2x2;

        dsp->chroma_interpolation_funcs[BLOCK_8x8] = chroma_interpolation_sse_8x8;
        dsp->chroma_interpolation_funcs[BLOCK_8x4] = chroma_interpolation_sse_8x4;
        dsp->chroma_interpolation_funcs[BLOCK_4x8] = chroma_interpolation_sse_4x8;
        dsp->chroma_interpolation_funcs[BLOCK_4x4] = chroma_interpolation_sse_4x4;
        dsp->chroma_interpolation_funcs[BLOCK_4x2] = chroma_interpolation_sse_4x2;
        dsp->chroma_interpolation_funcs[BLOCK_2x4] = chroma_interpolation_sse_2x4;
        dsp->chroma_interpolation_funcs[BLOCK_2x2] = chroma_interpolation_sse_2x2;

        dsp->transform_4x4    = transform_luma_4x4_sse;
        dsp->transform_8x8    = transform_luma_8x8_sse;
        dsp->transform_16x16  = transform_luma_16x16_sse;
        dsp->transform_chroma = transform_chroma_sse;
    }
}