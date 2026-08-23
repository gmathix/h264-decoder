//
// Created by gmathix on 8/23/26.
//

#include "dsp_init.h"

#if ARCH_X86
#include "x86_64/dsp_init_x86.c"
#endif


#define WIDTH 16
#define HEIGHT 16
#include "templates/weighted_pred.c"        // 16x16
#include "templates/qpel.c"
#undef HEIGHT
#define HEIGHT 8
#include "templates/weighted_pred.c"        // 16x8
#include "templates/qpel.c"
#undef WIDTH
#define WIDTH 8
#include "templates/weighted_pred.c"        // 8x8
#include "templates/qpel.c"
#include "templates/chroma_interpolation.c"
#undef HEIGHT
#define HEIGHT 16
#include "templates/weighted_pred.c"        // 8x16
#include "templates/qpel.c"
#undef HEIGHT
#define HEIGHT 4
#include "templates/weighted_pred.c"        // 8x4
#include "templates/qpel.c"
#include "templates/chroma_interpolation.c"
#undef WIDTH
#define WIDTH 4
#include "templates/weighted_pred.c"        // 4x4
#include "templates/qpel.c"
#include "templates/chroma_interpolation.c"
#undef HEIGHT
#define HEIGHT 8
#include "templates/weighted_pred.c"        // 4x8
#include "templates/qpel.c"
#include "templates/chroma_interpolation.c"
#undef HEIGHT
#define HEIGHT 2
#include "templates/weighted_pred.c"        // 4x2
#include "templates/chroma_interpolation.c"
#undef WIDTH
#define WIDTH 2
#include "templates/weighted_pred.c"        // 2x2
#include "templates/chroma_interpolation.c"
#undef HEIGHT
#define HEIGHT 4
#include "templates/weighted_pred.c"        // 2x4
#include "templates/chroma_interpolation.c"


#undef HEIGHT
#undef WIDTH




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
}



void dsp_init(DSPContext *dsp) {
    dsp_init_c(dsp);
#if ARCH_X86
    dsp_init_x86(dsp);
#endif
}