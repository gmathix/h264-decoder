//
// Created by gmathix on 8/21/26.
//

/*  this file is compiled 7 times, once for every partition shape type :
    16x16, 16x8, 8x16, 8x8, 8x4, 4x8, 4x4
*/



#include "mb.h"
#include "inter.h"
#include "dsp_init.h"


#ifndef WIDTH
#define WIDTH 16
#endif
#ifndef HEIGHT
#define HEIGHT 16
#endif


#define INTER_FUNC3(name, W, H, ...) name ## _ ## W ## x ## H(__VA_ARGS__)
#define INTER_FUNC2(name, W, H, ...) INTER_FUNC3(name, W, H, __VA_ARGS__)
#define INTER_FUNC(name, ...) INTER_FUNC2(name, WIDTH, HEIGHT, __VA_ARGS__)

#define BLOCK_TYPE3(w, h) BLOCK_ ## w ## x ## h
#define BLOCK_TYPE2(w, h) BLOCK_TYPE3(w, h)
#define BLOCK_TYPE BLOCK_TYPE2(WIDTH, HEIGHT)





void INTER_FUNC(fetch_ref_block_luma,
                const uint8_t * restrict ref, uint8_t * restrict scratch_buf,
                int picW, int picH, int y, int x) {

    // fast path: entire fetch window is inside the picture
    if (y >= 2 && y + HEIGHT+5 < picH && x >= 2 && x + WIDTH+3 < picW) {
        for (int i = 0; i < HEIGHT+5; i++) {
            memcpy(&scratch_buf[i*(WIDTH+5)], &ref[(y-2+i)*picW + (x-2)], WIDTH+5);
        }
        return;
    }

    // slow path: border mb
    int yc, xc;
    for (int i = 0; i < HEIGHT+5; i++) {
        yc = _clip3(0, picH - 1, y-2+i);
        const uint8_t *row = &ref[yc*picW];
        for (int j = 0; j < WIDTH+5; j++) {
            xc = _clip3(0, picW - 1, x-2+j);
            uint8_t s = row[xc];
            scratch_buf[i*(WIDTH+5)+ j] = s;
        }
    }
}

void INTER_FUNC(inter_pred_single,
                Macroblock *mb, int pos4x4, MotionVector mv, int list,
                uint8_t *restrict scratch_buf, int16_t *restrict qpel_pass_buf, const Undo264Context *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic = ctx->dpb->lists[list][1+mv.ref_idx];
    bool weighted = ctx->wpred.is_active;

    int yBase = mb->mb_y*16 + ((pos4x4>>2) << 2);
    int xBase = mb->mb_x*16 + ((pos4x4&3)  << 2);

    int stride = currPic->widthY;
    uint8_t *dst = &currPic->luma[yBase*stride + xBase];

    // mv offsets
    int xOffInt  = mv.x >> 2;
    int yOffInt  = mv.y >> 2;
    int xFrac    = mv.x & 3;
    int yFrac    = mv.y & 3;

    INTER_FUNC(fetch_ref_block_luma, refPic->luma, scratch_buf, refPic->widthY, refPic->heightY, yBase + yOffInt, xBase + xOffInt);

    ctx->dsp->qpel_func_arrays[BLOCK_TYPE][(yFrac << 2) | xFrac] (scratch_buf, dst, qpel_pass_buf, stride);

    if (weighted) {
        int logWD = ctx->wpred.logWD[0];
        int w = ctx->wpred.weight[list][0];
        int o = ctx->wpred.offset[list][0];

        dst = &currPic->luma[yBase*stride + xBase];
        ctx->dsp->weigh_single_funcs[BLOCK_TYPE](dst, stride, logWD, w, o);
    }
}


void INTER_FUNC(inter_pred_bi,
                Macroblock *mb, int pos4x4, MotionVector mvL0, MotionVector mvL1,
                uint8_t *restrict scratch_buf, uint8_t *restrict temp_bi_buf, int16_t *restrict qpel_pass_buf,
                const Undo264Context *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *picL0 = ctx->dpb->lists[L0][1+mvL0.ref_idx];
    Picture *picL1 = ctx->dpb->lists[L1][1+mvL1.ref_idx];
    bool weighted = ctx->wpred.is_active;

    int yBase = mb->mb_y*16 + ((pos4x4>>2) << 2);
    int xBase = mb->mb_x*16 + ((pos4x4&3)  << 2);

    int dimension = WIDTH * HEIGHT;

    int xOffInt0  = mvL0.x >> 2;
    int yOffInt0  = mvL0.y >> 2;
    int xFrac0    = mvL0.x & 3;
    int yFrac0    = mvL0.y & 3;

    int xOffInt1  = mvL1.x >> 2;
    int yOffInt1  = mvL1.y >> 2;
    int xFrac1    = mvL1.x & 3;
    int yFrac1    = mvL1.y & 3;

    INTER_FUNC(fetch_ref_block_luma, picL0->luma, scratch_buf, picL0->widthY, picL0->heightY, yBase + yOffInt0, xBase + xOffInt0);
    ctx->dsp->qpel_func_arrays[BLOCK_TYPE][(yFrac0 << 2) | xFrac0] (scratch_buf, &temp_bi_buf[0], qpel_pass_buf, WIDTH);

    INTER_FUNC(fetch_ref_block_luma, picL1->luma, scratch_buf, picL1->widthY, picL1->heightY, yBase + yOffInt1, xBase + xOffInt1);
    ctx->dsp->qpel_func_arrays[BLOCK_TYPE][(yFrac1 << 2) | xFrac1] (scratch_buf, &temp_bi_buf[dimension], qpel_pass_buf, WIDTH);



    int logWD = ctx->wpred.logWD[0];
    int w0 = ctx->wpred.weight[L0][0];
    int w1 = ctx->wpred.weight[L1][0];
    int o0 = ctx->wpred.offset[L0][0];
    int o1 = ctx->wpred.offset[L1][0];


    int stride = currPic->widthY;
    uint8_t *dst = &currPic->luma[yBase * stride + xBase];
    if (!weighted) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                dst[x] = (uint8_t) (((unsigned)temp_bi_buf[y*WIDTH + x] + (unsigned)temp_bi_buf[dimension + y*WIDTH + x] + 1) >> 1);
            }
            dst += stride;
        }
    } else {
        ctx->dsp->weigh_bi_funcs[BLOCK_TYPE](temp_bi_buf, dst, stride, logWD, w0, w1, o0, o1);
    }
}

