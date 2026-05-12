//
// Created by gmathix on 5/2/26.
//

#include "inter.h"
#include "qpel.h"


/* gets called for every 4x4 sub macroblock every time, even if they belong to a 16x16 inter mb
 * pos: 4x4 block index within macroblock
 */
void inter_pred(Macroblock *mb, int idx, MotionVector *mv, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic  = ctx->dpb->l0[mv->ref_idx];

    // top-left sample coords relative to picture
    int yBase = mb->mb_y*16 + ((idx>>2) << 2);
    int xBase = mb->mb_x*16 + ((idx&3)  << 2);

    int stride = currPic->strideY;
    uint8_t *dst = &currPic->luma[yBase*stride + xBase];

    // mv offsets
    int xOffInt  = mv->x >> 2;
    int yOffInt  = mv->y >> 2;
    int xFrac    = mv->x & 3;
    int yFrac    = mv->y & 3;

    uint8_t ref_samples[9][9];
    fetch_9x9_block(refPic, yBase + yOffInt, xBase + xOffInt, ref_samples, ctx);

    qpel_funcs[(yFrac<<2) | xFrac] (ref_samples, dst, stride, ctx->ps->sps->bit_depth_luma_minus8 + 8);
    // qpel_00(ref_samples, dst, stride, ctx->ps->sps->bit_depth_luma_minus8 + 8); // fun testing
}

void inter_pred_chroma(Macroblock *mb, int idx, MotionVector *mv, CodecContext *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic  = ctx->dpb->l0[mv->ref_idx];

    int yBase = mb->mb_y*8 + ((idx>>1) << 2);
    int xBase = mb->mb_x*8 + ((idx&1)  << 2);

    int stride = currPic->strideC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];

    int xOffInt  = mv->x >> 3;
    int yOffInt  = mv->y >> 3;
    int xFrac    = mv->x & 7;
    int yFrac    = mv->y & 7;

    uint8_t ref_cb[5][5], ref_cr[5][5];
    fetch_5x5_block_chroma(refPic, yBase + yOffInt, xBase + xOffInt, ref_cb, ref_cr, ctx);

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            *(dstCb++) = ((8-xFrac) * (8-yFrac) * ref_cb[y][x]     +
                             xFrac  * (8-yFrac) * ref_cb[y][x+1]   +
                          (8-xFrac) *    yFrac  * ref_cb[y+1][x]   +
                             xFrac  *    yFrac  * ref_cb[y+1][x+1] + 32) >> 6;
            *(dstCr++) = ((8-xFrac) * (8-yFrac) * ref_cr[y][x]     +
                             xFrac  * (8-yFrac) * ref_cr[y][x+1]   +
                          (8-xFrac) *    yFrac  * ref_cr[y+1][x]   +
                             xFrac  *    yFrac  * ref_cr[y+1][x+1] + 32) >> 6;
        }
        dstCb += stride - 4;
        dstCr += stride - 4;
    }
}


