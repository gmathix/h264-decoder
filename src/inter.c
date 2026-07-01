//
// Created by gmathix on 5/2/26.
//

#include "inter.h"
#include "qpel.h"


/* gets called for every 4x4 sub macroblock every time, even if they belong to a 16x16 inter mb
 * TODO: adaptive inter pred following mb partitioning (avoid calling this 16 times for a 16x16 mb when inter pred could be done in one shot)
 */
void inter_pred_single(Macroblock *mb, int idx, MotionVector *mv, bool l0, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic  = l0 ? ctx->dpb->l0[mv->ref_idx] : ctx->dpb->l1[mv->ref_idx];

    // top-left sample coords relative to picture
    int yBase = mb->mb_y*16 + ((idx>>2) << 2);
    int xBase = mb->mb_x*16 + ((idx&3)  << 2);

    int stride = currPic->widthY;
    uint8_t *dst = &currPic->luma[yBase*stride + xBase];

    // mv offsets
    int xOffInt  = mv->x >> 2;
    int yOffInt  = mv->y >> 2;
    int xFrac    = mv->x & 3;
    int yFrac    = mv->y & 3;


    fetch_9x9_block(refPic, yBase + yOffInt, xBase + xOffInt, ctx->ref_samples, ctx);

    qpel_funcs[(yFrac<<2) | xFrac] (ctx->ref_samples, dst, stride, ctx->ps->sps->bit_depth_luma_minus8 + 8);
    // qpel_00(ref_samples, dst, stride, ctx->ps->sps->bit_depth_luma_minus8 + 8); // fun testing
}


void inter_pred_bi(Macroblock *mb, int idx, MotionVector *mvL0, MotionVector *mvL1, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic0 = ctx->dpb->l0[mvL0->ref_idx];
    Picture *refPic1 = ctx->dpb->l1[mvL1->ref_idx];


    MotionVector *mvList[2] = {mvL0, mvL1};
    Picture *picList[2] = {refPic0, refPic1};

    // temp buffer for sample accumulation
    uint8_t temp_samples[2][4][4];

    // top-left sample coords relative to picture
    int yBase = mb->mb_y*16 + ((idx>>2) << 2);
    int xBase = mb->mb_x*16 + ((idx&3)  << 2);


    for (int i = 0; i < 2; i++) {
        MotionVector *mv = mvList[i];
        Picture *refPic = picList[i];

        uint8_t *dst = &temp_samples[i][0][0];

        // mv offsets
        int xOffInt  = mv->x >> 2;
        int yOffInt  = mv->y >> 2;
        int xFrac    = mv->x & 3;
        int yFrac    = mv->y & 3;


        fetch_9x9_block(refPic, yBase + yOffInt, xBase + xOffInt, ctx->ref_samples, ctx);

        qpel_funcs[(yFrac<<2) | xFrac] (ctx->ref_samples, dst, 4, ctx->ps->sps->bit_depth_luma_minus8 + 8);
        // qpel_00(ref_samples, dst, 4, ctx->ps->sps->bit_depth_luma_minus8 + 8); // fun testing
    }

    int stride = currPic->widthY;
    uint8_t *dst = &currPic->luma[yBase * stride + xBase];
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            dst[x] = (temp_samples[0][y][x] + temp_samples[1][y][x] + 1) >> 1;
        }
        dst += stride;
    }
}


void inter_pred_chroma_single(Macroblock *mb, int y, int x, MotionVector *mv, bool l0, CodecContext *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic  = l0 ? ctx->dpb->l0[mv->ref_idx] : ctx->dpb->l1[mv->ref_idx];

    const int yBase = mb->mb_y*8 + y;
    const int xBase = mb->mb_x*8 + x;

    int stride = currPic->widthC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];

    const int xOffInt  = mv->x >> 3;
    const int yOffInt  = mv->y >> 3;
    const int xFrac    = mv->x & 7;
    const int yFrac    = mv->y & 7;

    uint8_t ref_cb[3][3], ref_cr[3][3];
    fetch_3x3_block_chroma(refPic, yBase + yOffInt, xBase + xOffInt, ref_cb, ref_cr, ctx);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            dstCb[j] = ((8-xFrac) * (8-yFrac) * ref_cb[i][j]     +
                           xFrac  * (8-yFrac) * ref_cb[i][j+1]   +
                        (8-xFrac) *    yFrac  * ref_cb[i+1][j]   +
                           xFrac  *    yFrac  * ref_cb[i+1][j+1] + 32) >> 6;
            dstCr[j] = ((8-xFrac) * (8-yFrac) * ref_cr[i][j]     +
                           xFrac  * (8-yFrac) * ref_cr[i][j+1]   +
                        (8-xFrac) *    yFrac  * ref_cr[i+1][j]   +
                           xFrac  *    yFrac  * ref_cr[i+1][j+1] + 32) >> 6;
        }
        dstCb += stride;
        dstCr += stride;
    }
}


void inter_pred_chroma_bi(Macroblock *mb, int y, int x, MotionVector *mvL0, MotionVector *mvL1, CodecContext *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic0 = ctx->dpb->l0[mvL0->ref_idx];
    Picture *refPic1 = ctx->dpb->l1[mvL1->ref_idx];


    MotionVector *mvList[2] = {mvL0, mvL1};
    Picture *picList[2] = {refPic0, refPic1};

    uint8_t temp_samples_cb[2][2][2];
    uint8_t temp_samples_cr[2][2][2];

    const int yBase = mb->mb_y*8 + y;
    const int xBase = mb->mb_x*8 + x;




    for (int i = 0; i < 2; i++) {
        MotionVector *mv = mvList[i];
        Picture *refPic = picList[i];

        uint8_t *dstCb = &temp_samples_cb[i][0][0];
        uint8_t *dstCr = &temp_samples_cr[i][0][0];

        const int xOffInt  = mv->x >> 3;
        const int yOffInt  = mv->y >> 3;
        const int xFrac    = mv->x & 7;
        const int yFrac    = mv->y & 7;

        uint8_t ref_cb[3][3], ref_cr[3][3];
        fetch_3x3_block_chroma(refPic, yBase + yOffInt, xBase + xOffInt, ref_cb, ref_cr, ctx);

        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                dstCb[x] = ((8-xFrac) * (8-yFrac) * ref_cb[y][x]     +
                               xFrac  * (8-yFrac) * ref_cb[y][x+1]   +
                            (8-xFrac) *    yFrac  * ref_cb[y+1][x]   +
                               xFrac  *    yFrac  * ref_cb[y+1][x+1] + 32) >> 6;
                dstCr[x] = ((8-xFrac) * (8-yFrac) * ref_cr[y][x]     +
                               xFrac  * (8-yFrac) * ref_cr[y][x+1]   +
                            (8-xFrac) *    yFrac  * ref_cr[y+1][x]   +
                               xFrac  *    yFrac  * ref_cr[y+1][x+1] + 32) >> 6;
            }
            dstCb += 2;
            dstCr += 2;
        }
    }

    int stride = currPic->widthC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            dstCb[x] = (uint8_t) ((uint32_t)temp_samples_cb[0][y][x] + (uint32_t)temp_samples_cb[1][y][x] + 1) >> 1;
            dstCr[x] = (uint8_t) ((uint32_t)temp_samples_cr[0][y][x] + (uint32_t)temp_samples_cr[1][y][x] + 1) >> 1;
        }
        dstCb += stride;
        dstCr += stride;
    }
}




