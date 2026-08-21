//
// Created by gmathix on 8/21/26.
//

/*  this file is compiled 7 times, once for every partition shape type :
    16x16, 16x8, 8x16, 8x8, 8x4, 4x8, 4x4
*/



#include "mb.h"
#include "inter.h"


#ifndef WIDTH
#define WIDTH 16
#endif
#ifndef HEIGHT
#define HEIGHT 16
#endif


#define INTER_FUNC3(name, W, H, ...) name ## _ ## W ## x ## H(__VA_ARGS__)
#define INTER_FUNC2(name, W, H, ...) INTER_FUNC3(name, W, H, __VA_ARGS__)
#define INTER_FUNC(name, ...) INTER_FUNC2(name, WIDTH, HEIGHT, __VA_ARGS__)


#include "qpel_template.c"



void INTER_FUNC(inter_pred_single,
                Macroblock *mb, int pos4x4, MotionVector mv, int list,
                uint8_t *restrict scratch_buf, int16_t *restrict qpel_pass_buf, Undo264Context *ctx) {

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

    fetch_ref_block(refPic->luma, scratch_buf, refPic->widthY, refPic->heightY, yBase + yOffInt, xBase + xOffInt, WIDTH, HEIGHT);

    QPEL_FUNCS_ARRAY[(yFrac<<2) | xFrac] (scratch_buf, dst, qpel_pass_buf, stride);

    if (weighted) {
        int logWD = ctx->wpred.logWD[0];
        int w = ctx->wpred.weight[list][0];
        int o = ctx->wpred.offset[list][0];

        dst = &currPic->luma[yBase*stride + xBase];
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int t0 = dst[x];
                if (logWD >= 1) dst[x] = _clip1y(((t0 * w + (1 << (logWD-1))) >> logWD) + o, 8);
                else            dst[x] = _clip1y(t0 * w + o, 8);
            }
            dst += stride;
        }
    }
}


void INTER_FUNC(inter_pred_bi,
                Macroblock *mb, int pos4x4, MotionVector mvL0, MotionVector mvL1,
                uint8_t *restrict scratch_buf, uint8_t *restrict temp_bi_buf, int16_t *restrict qpel_pass_buf,
                Undo264Context *ctx) {

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

    fetch_ref_block(picL0->luma, scratch_buf, picL0->widthY, picL0->heightY, yBase + yOffInt0, xBase + xOffInt0, WIDTH, HEIGHT);
    QPEL_FUNCS_ARRAY[(yFrac0 << 2) | xFrac0] (scratch_buf, &temp_bi_buf[0], qpel_pass_buf, WIDTH);

    fetch_ref_block(picL1->luma, scratch_buf, picL1->widthY, picL1->heightY, yBase + yOffInt1, xBase + xOffInt1, WIDTH, HEIGHT);
    QPEL_FUNCS_ARRAY[(yFrac1 << 2) | xFrac1] (scratch_buf, &temp_bi_buf[dimension], qpel_pass_buf, WIDTH);



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
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int t0 = temp_bi_buf[y*WIDTH + x];
                int t1 = temp_bi_buf[dimension + y*WIDTH + x];
                dst[x] = (uint8_t) _clip1y(((t0 * w0 + t1 * w1 + (1<<logWD)) >>
                                              (logWD + 1)) + ((o0 + o1 + 1) >> 1), 8);
            }
            dst += stride;
        }
    }
}

