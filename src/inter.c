//
// Created by gmathix on 5/2/26.
//

#include "inter.h"
#include "qpel.h"


/* gets called for every 4x4 sub macroblock every time, even if they belong to a 16x16 inter mb
 * TODO: adaptive inter pred following mb partitioning (avoid calling this 16 times for a 16x16 mb when inter pred could be done in one shot)
 */
void inter_pred(Macroblock *mb, int idx, MotionVector *mv, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic  = ctx->dpb->l0[mv->ref_idx];

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

void inter_pred_chroma(Macroblock *mb, int y, int x, MotionVector *mv, CodecContext *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic  = ctx->dpb->l0[mv->ref_idx];

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


