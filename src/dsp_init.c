//
// Created by gmathix on 8/23/26.
//

#include "dsp_init.h"

#if ARCH_X86
#include "x86_64/dsp_init_x86.c"
#endif


#define WIDTH 16
#define HEIGHT 16
#include "dsp/weighted_pred_template.c"        // 16x16
#include "dsp/qpel_template.c"
#undef HEIGHT
#define HEIGHT 8
#include "dsp/weighted_pred_template.c"        // 16x8
#include "dsp/qpel_template.c"
#undef WIDTH
#define WIDTH 8
#include "dsp/weighted_pred_template.c"        // 8x8
#include "dsp/qpel_template.c"
#include "dsp/chroma_interpolation_template.c"
#undef HEIGHT
#define HEIGHT 16
#include "dsp/weighted_pred_template.c"        // 8x16
#include "dsp/qpel_template.c"
#undef HEIGHT
#define HEIGHT 4
#include "dsp/weighted_pred_template.c"        // 8x4
#include "dsp/qpel_template.c"
#include "dsp/chroma_interpolation_template.c"
#undef WIDTH
#define WIDTH 4
#include "dsp/weighted_pred_template.c"        // 4x4
#include "dsp/qpel_template.c"
#include "dsp/chroma_interpolation_template.c"
#undef HEIGHT
#define HEIGHT 8
#include "dsp/weighted_pred_template.c"        // 4x8
#include "dsp/qpel_template.c"
#include "dsp/chroma_interpolation_template.c"
#undef HEIGHT
#define HEIGHT 2
#include "dsp/weighted_pred_template.c"        // 4x2
#include "dsp/chroma_interpolation_template.c"
#undef WIDTH
#define WIDTH 2
#include "dsp/weighted_pred_template.c"        // 2x2
#include "dsp/chroma_interpolation_template.c"
#undef HEIGHT
#define HEIGHT 4
#include "dsp/weighted_pred_template.c"        // 2x4
#include "dsp/chroma_interpolation_template.c"


#undef HEIGHT
#undef WIDTH


#include "dsp/transform.c"
#include "dsp/deblock_edge.c"


// scalar versions by default
static void dsp_init_c(DSPContext *dsp) {
    dsp->weigh_bi_funcs[BLOCK_16x16] = weigh_bi_16x16;
    dsp->weigh_bi_funcs[BLOCK_16x8]  = weigh_bi_16x8;
    dsp->weigh_bi_funcs[BLOCK_8x16]  = weigh_bi_8x16;
    dsp->weigh_bi_funcs[BLOCK_8x8]   = weigh_bi_8x8;
    dsp->weigh_bi_funcs[BLOCK_8x4]   = weigh_bi_8x4;
    dsp->weigh_bi_funcs[BLOCK_4x8]   = weigh_bi_4x8;
    dsp->weigh_bi_funcs[BLOCK_4x4]   = weigh_bi_4x4;
    dsp->weigh_bi_funcs[BLOCK_4x2]   = weigh_bi_4x2;
    dsp->weigh_bi_funcs[BLOCK_2x4]   = weigh_bi_2x4;
    dsp->weigh_bi_funcs[BLOCK_2x2]   = weigh_bi_2x2;

    dsp->weigh_single_funcs[BLOCK_16x16] = weigh_single_16x16;
    dsp->weigh_single_funcs[BLOCK_16x8]  = weigh_single_16x8;
    dsp->weigh_single_funcs[BLOCK_8x16]  = weigh_single_8x16;
    dsp->weigh_single_funcs[BLOCK_8x8]   = weigh_single_8x8;
    dsp->weigh_single_funcs[BLOCK_8x4]   = weigh_single_8x4;
    dsp->weigh_single_funcs[BLOCK_4x8]   = weigh_single_4x8;
    dsp->weigh_single_funcs[BLOCK_4x4]   = weigh_single_4x4;
    dsp->weigh_single_funcs[BLOCK_4x2]   = weigh_single_4x2;
    dsp->weigh_single_funcs[BLOCK_2x4]   = weigh_single_2x4;
    dsp->weigh_single_funcs[BLOCK_2x2]   = weigh_single_2x2;

    dsp->chroma_interpolation_funcs[BLOCK_8x8] = chroma_interpolation_8x8;
    dsp->chroma_interpolation_funcs[BLOCK_8x4] = chroma_interpolation_8x4;
    dsp->chroma_interpolation_funcs[BLOCK_4x8] = chroma_interpolation_4x8;
    dsp->chroma_interpolation_funcs[BLOCK_4x4] = chroma_interpolation_4x4;
    dsp->chroma_interpolation_funcs[BLOCK_4x2] = chroma_interpolation_4x2;
    dsp->chroma_interpolation_funcs[BLOCK_2x4] = chroma_interpolation_2x4;
    dsp->chroma_interpolation_funcs[BLOCK_2x2] = chroma_interpolation_2x2;

    memcpy(dsp->qpel_func_arrays[BLOCK_16x16], qpel_funcs_16x16, sizeof dsp->qpel_func_arrays[BLOCK_16x16]);
    memcpy(dsp->qpel_func_arrays[BLOCK_16x8],  qpel_funcs_16x8,  sizeof dsp->qpel_func_arrays[BLOCK_16x8]);
    memcpy(dsp->qpel_func_arrays[BLOCK_8x16],  qpel_funcs_8x16,  sizeof dsp->qpel_func_arrays[BLOCK_8x16]);
    memcpy(dsp->qpel_func_arrays[BLOCK_8x8],   qpel_funcs_8x8,   sizeof dsp->qpel_func_arrays[BLOCK_8x8]);
    memcpy(dsp->qpel_func_arrays[BLOCK_8x4],   qpel_funcs_8x4,   sizeof dsp->qpel_func_arrays[BLOCK_8x4]);
    memcpy(dsp->qpel_func_arrays[BLOCK_4x8],   qpel_funcs_4x8,   sizeof dsp->qpel_func_arrays[BLOCK_4x8]);
    memcpy(dsp->qpel_func_arrays[BLOCK_4x4],   qpel_funcs_4x4,   sizeof dsp->qpel_func_arrays[BLOCK_4x4]);

    dsp->transform_4x4    = transform_luma_4x4;
    dsp->transform_8x8    = transform_luma_8x8;
    dsp->transform_16x16  = transform_luma_16x16;
    dsp->transform_chroma = transform_chroma;

    dsp->deblock_edge_high_bs_luma   = deblock_edge_high_bs_luma;
    dsp->deblock_edge_low_bs_luma    = deblock_edge_low_bs_luma;
    dsp->deblock_edge_high_bs_chroma = deblock_edge_high_bs_chroma;
    dsp->deblock_edge_low_bs_chroma  = deblock_edge_low_bs_chroma;
}



void dsp_init(DSPContext *dsp) {
    dsp_init_c(dsp);
#if ARCH_X86
    dsp_init_x86(dsp);
#endif
}