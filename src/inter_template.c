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


void INTER_FUNC(luma_weigh_single_nolog,
                uint8_t *dst, int stride, int w, int o) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = _clip1y(dst[x] * w + o, MAX_U8);
        }
        dst += stride;
    }
}
void INTER_FUNC(luma_weigh_single,
                uint8_t *dst, int stride, int logWD, int w, int o) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = _clip1y(((dst[x] * w + (1 << (logWD-1))) >> logWD) + o, MAX_U8);
        }
        dst += stride;
    }
}

void INTER_FUNC(luma_weigh_bi,
                const uint8_t *restrict temp_bi_buf, uint8_t *restrict dst, int stride,
                int logWD, int w0, int w1, int o0, int o1) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int t0 = temp_bi_buf[y*WIDTH + x];
            int t1 = temp_bi_buf[WIDTH*HEIGHT + y*WIDTH + x];
            dst[x] = (uint8_t) _clip1y(((t0 * w0 + t1 * w1 + (1<<logWD)) >>
                                          (logWD + 1)) + ((o0 + o1 + 1) >> 1), MAX_U8);
        }
        dst += stride;
    }
}

void INTER_FUNC(fetch_ref_block_luma,
                const uint8_t * restrict ref, uint8_t * restrict scratch_buf,
                int picW, int picH, int y, int x) {
    int height = HEIGHT+5;
    int width = WIDTH+5;
    // fast path: entire fetch window is inside the picture
    if (y - 2 >= 0 && y + height-2 < picH && x - 2 >= 0 && x + width-2 < picW) {
        for (int i = 0; i < height; i++) {
            memcpy(&scratch_buf[i*width], &ref[(y-2+i)*picW + (x-2)], width);
        }
        return;
    }

    // slow path: border mb
    int yc, xc;
    for (int i = 0; i < height; i++) {
        yc = _clip3(0, picH - 1, y-2+i);
        const uint8_t *row = &ref[yc*picW];
        for (int j = 0; j < width; j++) {
            xc = _clip3(0, picW - 1, x-2+j);
            uint8_t s = row[xc];
            scratch_buf[i*width + j] = s;
        }
    }
}

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

    INTER_FUNC(fetch_ref_block_luma, refPic->luma, scratch_buf, refPic->widthY, refPic->heightY, yBase + yOffInt, xBase + xOffInt);

    QPEL_FUNCS_ARRAY[(yFrac<<2) | xFrac] (scratch_buf, dst, qpel_pass_buf, stride);

    if (weighted) {
        int logWD = ctx->wpred.logWD[0];
        int w = ctx->wpred.weight[list][0];
        int o = ctx->wpred.offset[list][0];

        dst = &currPic->luma[yBase*stride + xBase];
        if (logWD >= 1) INTER_FUNC(luma_weigh_single, dst, stride, logWD, w, o);
        else            INTER_FUNC(luma_weigh_single_nolog, dst, stride, w, o);
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

    INTER_FUNC(fetch_ref_block_luma, picL0->luma, scratch_buf, picL0->widthY, picL0->heightY, yBase + yOffInt0, xBase + xOffInt0);
    QPEL_FUNCS_ARRAY[(yFrac0 << 2) | xFrac0] (scratch_buf, &temp_bi_buf[0], qpel_pass_buf, WIDTH);

    INTER_FUNC(fetch_ref_block_luma, picL1->luma, scratch_buf, picL1->widthY, picL1->heightY, yBase + yOffInt1, xBase + xOffInt1);
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
        // INTER_FUNC(luma_weigh_bi, temp_bi_buf, dst, stride, logWD, w0, w1, o0, o1);
        // in x86_64/weighted_pred_sse4.c, will get resolved from including in mb.c
        WEIGHTED_SSE_FUNC2(weigh_bi_sse, WIDTH, HEIGHT, temp_bi_buf, dst, stride, logWD, w0, w1, o0, o1);
    }
}

