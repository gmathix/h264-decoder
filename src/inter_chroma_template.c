//
// Created by gmathix on 8/21/26.
//

#include "mb.h"
#include "inter.h"


/*  this file is compiled 7 times, once for every chroma partition shape type :
    8x8, 8x4, 4x8, 4x4, 4x2, 2x4, 2x2
*/


#ifndef WIDTH
#define WIDTH 8
#endif
#ifndef HEIGHT
#define HEIGHT 8
#endif


#define INTER_CHROMA_FUNC3(name, W, H, ...) name ## _ ## W ## x ## H(__VA_ARGS__)
#define INTER_CHROMA_FUNC2(name, W, H, ...) INTER_CHROMA_FUNC3(name, W, H, __VA_ARGS__)
#define INTER_CHROMA_FUNC(name, ...) INTER_CHROMA_FUNC2(name, WIDTH, HEIGHT, __VA_ARGS__)




void INTER_CHROMA_FUNC(fetch_ref_block_chroma,
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


void INTER_CHROMA_FUNC(chroma_interpolation,
                       int xFrac, int yFrac, int stride,
                       const uint8_t * restrict temp_buf, uint8_t * restrict dst) {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            dst[j] =   ((8-xFrac) * (8-yFrac) * temp_buf[(i+2)*(WIDTH+5) + 2+j]     +
                           xFrac  * (8-yFrac) * temp_buf[(i+2)*(WIDTH+5) + 2+j+1]   +
                        (8-xFrac) *    yFrac  * temp_buf[(i+3)*(WIDTH+5) + 2+j]   +
                           xFrac  *    yFrac  * temp_buf[(i+3)*(WIDTH+5) + 2+j+1] + 32) >> 6;
        }
        dst += stride;
    }
}

void INTER_CHROMA_FUNC(chroma_weigh,
                       uint8_t *dst, int logWD, int w, int o, int stride) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int t = dst[x];
            dst[x] = logWD >= 1
                    ? (uint8_t) _clip1c(((t * w + (1 << (logWD-1))) >> logWD) + o, 8)
                    : (uint8_t) _clip1c(t * w + o, 8);
        }
        dst += stride;
    }
}

void INTER_CHROMA_FUNC(chroma_weigh_bi,
                       uint8_t * restrict dst, const uint8_t * restrict temp_bi_buf,
                       int logWD, int w0, int w1, int o0, int o1, int stride) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int t0 = temp_bi_buf[y*WIDTH + x];
            int t1 = temp_bi_buf[HEIGHT*WIDTH + y*WIDTH + x];
            dst[x] = (uint8_t) _clip1c(((t0 * w0 + t1 * w1 + (1 << logWD)) >>
                                    (logWD + 1)) + ((o0 + o1 + 1) >> 1), 8);
        }
        dst += stride;
    }
}

void INTER_CHROMA_FUNC(copy_from_temp_bi_buf,
                       uint8_t * restrict dst, const uint8_t * restrict temp_bi_buf, int stride) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = (uint8_t) (((uint32_t)temp_bi_buf[y*WIDTH + x] + (uint32_t)temp_bi_buf[HEIGHT*WIDTH + y*WIDTH + x] + 1) >> 1);
        }
        dst += stride;
    }
}

static ALWAYS_INLINE void INTER_CHROMA_FUNC(derive_offsets,
                                            MotionVector mv, int *yOffInt, int *xOffInt, int *yFrac, int *xFrac) {
    *xOffInt = mv.x >> 3;
    *yOffInt = mv.y >> 3;
    *xFrac   = mv.x & 7;
    *yFrac   = mv.y & 7;
}

void INTER_CHROMA_FUNC(inter_pred_chroma_single,
                       Macroblock *mb, int pos4x4, MotionVector mv, int list,
                       uint8_t *scratch_buf, Undo264Context *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic  = ctx->dpb->lists[list][1+mv.ref_idx];
    bool weighted = ctx->wpred.is_active;

    const int yBase = mb->mb_y*8 + ((pos4x4>>2) << 1);
    const int xBase = mb->mb_x*8 + ((pos4x4&3)  << 1);

    int stride = currPic->widthC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];

    int yOffInt, xOffInt, yFrac, xFrac;
    INTER_CHROMA_FUNC(derive_offsets, mv, &yOffInt, &xOffInt, &yFrac, &xFrac);


    INTER_CHROMA_FUNC(fetch_ref_block_chroma, refPic->cb, scratch_buf, refPic->widthC, refPic->heightC, yBase + yOffInt, xBase + xOffInt);
    INTER_CHROMA_FUNC(chroma_interpolation, xFrac, yFrac, stride, scratch_buf, dstCb);

    INTER_CHROMA_FUNC(fetch_ref_block_chroma, refPic->cr, scratch_buf, refPic->widthC, refPic->heightC, yBase + yOffInt, xBase + xOffInt);
    INTER_CHROMA_FUNC(chroma_interpolation, xFrac, yFrac, stride, scratch_buf, dstCr);


    if (weighted) {
        INTER_CHROMA_FUNC(chroma_weigh, dstCb,
            ctx->wpred.logWD[1], ctx->wpred.weight[list][1], ctx->wpred.offset[list][1], stride);
        INTER_CHROMA_FUNC(chroma_weigh, dstCr,
            ctx->wpred.logWD[2], ctx->wpred.weight[list][2], ctx->wpred.offset[list][2], stride);
    }
}


void INTER_CHROMA_FUNC(inter_pred_chroma_bi,
                       Macroblock *mb, int pos4x4, MotionVector mvL0, MotionVector mvL1,
                       uint8_t *scratch_buf, uint8_t *temp_bi_buf, Undo264Context *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *ref0  = ctx->dpb->lists[L0][1+mvL0.ref_idx];
    Picture *ref1  = ctx->dpb->lists[L1][1+mvL1.ref_idx];
    bool weighted = ctx->wpred.is_active;

    const int yBase = mb->mb_y*8 + ((pos4x4>>2) << 1);
    const int xBase = mb->mb_x*8 + ((pos4x4&3)  << 1);

    int stride = currPic->widthC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];


    int yOffInt, xOffInt, yFrac, xFrac;


    // Cb / U

    INTER_CHROMA_FUNC(derive_offsets, mvL0, &yOffInt, &xOffInt, &yFrac, &xFrac);
    INTER_CHROMA_FUNC(fetch_ref_block_chroma, ref0->cb, scratch_buf, ref0->widthC, ref0->heightC, yBase + yOffInt, xBase + xOffInt);
    INTER_CHROMA_FUNC(chroma_interpolation, xFrac, yFrac, WIDTH, scratch_buf, &temp_bi_buf[0]);

    INTER_CHROMA_FUNC(derive_offsets, mvL1, &yOffInt, &xOffInt, &yFrac, &xFrac);
    INTER_CHROMA_FUNC(fetch_ref_block_chroma, ref1->cb, scratch_buf, ref1->widthC, ref1->heightC, yBase + yOffInt, xBase + xOffInt);
    INTER_CHROMA_FUNC(chroma_interpolation, xFrac, yFrac, WIDTH, scratch_buf, &temp_bi_buf[WIDTH*HEIGHT]);

    if (!weighted) {
        INTER_CHROMA_FUNC(copy_from_temp_bi_buf, dstCb, temp_bi_buf, stride);
    } else {
        INTER_CHROMA_FUNC(chroma_weigh_bi, dstCb, temp_bi_buf,
            ctx->wpred.logWD[1], ctx->wpred.weight[L0][1], ctx->wpred.weight[L1][1],
            ctx->wpred.offset[L0][1], ctx->wpred.offset[L1][1], stride);
    }


    // Cr / V
    INTER_CHROMA_FUNC(derive_offsets, mvL0, &yOffInt, &xOffInt, &yFrac, &xFrac);
    INTER_CHROMA_FUNC(fetch_ref_block_chroma, ref0->cr, scratch_buf, ref0->widthC, ref0->heightC, yBase + yOffInt, xBase + xOffInt);
    INTER_CHROMA_FUNC(chroma_interpolation, xFrac, yFrac, WIDTH, scratch_buf, &temp_bi_buf[0]);

    INTER_CHROMA_FUNC(derive_offsets, mvL1, &yOffInt, &xOffInt, &yFrac, &xFrac);
    INTER_CHROMA_FUNC(fetch_ref_block_chroma, ref1->cr, scratch_buf, ref1->widthC, ref1->heightC, yBase + yOffInt, xBase + xOffInt);
    INTER_CHROMA_FUNC(chroma_interpolation, xFrac, yFrac, WIDTH, scratch_buf, &temp_bi_buf[WIDTH*HEIGHT]);

    if (!weighted) {
        INTER_CHROMA_FUNC(copy_from_temp_bi_buf, dstCr, temp_bi_buf, stride);
    } else {
        INTER_CHROMA_FUNC(chroma_weigh_bi, dstCr, temp_bi_buf,
            ctx->wpred.logWD[2], ctx->wpred.weight[L0][2], ctx->wpred.weight[L1][2],
            ctx->wpred.offset[L0][2], ctx->wpred.offset[L1][2], stride);
    }
}