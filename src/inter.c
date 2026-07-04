//
// Created by gmathix on 5/2/26.
//

#include "inter.h"
#include "qpel.h"



void derive_pred_weights(int refL0, int refL1, bool predFlagL0, bool predFlagL1, CodecContext *ctx) {
    Picture *currPic = ctx->curr_pic;
    SliceHeader *sh  = currPic->sh;
    PPS *pps         = sh->pps;

    bool implicitMode = (pps->weighted_bipred_idc == 2 && IS_B_SLICE(sh->slice_type) && predFlagL0 && predFlagL1);
    bool explicitMode = !implicitMode &&
                        ((pps->weighted_bipred_idc == 1 && IS_B_SLICE(sh->slice_type) && (predFlagL0 || predFlagL1)) ||
                        (pps->weighted_pred_flag == 1 && IS_P_SLICE(sh->slice_type) && predFlagL0));

    ctx->wpred_active = implicitMode || explicitMode;


    if (implicitMode) {
        for (int i = 0; i < 3; i++) {
            ctx->logWD[i] = 5;
            ctx->o0[i] = ctx->o1[i] = 0;
        }

        Picture *pic0 = ctx->dpb->l0[refL0];
        Picture *pic1 = ctx->dpb->l1[refL1];

        int td = _clip3(-128, 127, pic1->poc - pic0->poc);

        if (td == 0 ||
            (pic0->dpb_status == LONG_TERM_REF || pic1->dpb_status == LONG_TERM_REF)) {
            for (int i = 0; i < 3; i++) {
                ctx->w0[i] = ctx->w1[i] = 32;
            }
        } else {
            int tb = _clip3(-128, 127, currPic->poc - pic0->poc);
            int tx = (16384 + _abs(td / 2)) / td;
            int distScaleFactor = _clip3(-1024, 1023, (tb * tx + 32) >> 6);

            if (((distScaleFactor >> 2) < -64) || ((distScaleFactor >> 2) > 128)) {
                for (int i = 0; i < 3; i++) {
                    ctx->w0[i] = ctx->w1[i] = 32;
                }
            } else {
                for (int i = 0; i < 3; i++) {
                    ctx->w0[i] = 64 - (distScaleFactor >> 2);
                    ctx->w1[i] = distScaleFactor >> 2;
                }
            }
        }
    }
    else if (explicitMode) {
        ctx->logWD[0] = ctx->luma_log2_weight_denom;
        ctx->w0[0]    = ctx->luma_weight_l0[refL0];
        ctx->w1[0]    = ctx->luma_weight_l1[refL1];
        ctx->o0[0]    = ctx->luma_offset_l0[refL0];
        ctx->o1[0]    = ctx->luma_offset_l1[refL1];

        for (int i = 0; i < 2; i++) {
            ctx->logWD[1+i] = ctx->chroma_log2_weight_denom;
            ctx->w0[1+i]    = ctx->chroma_weight_l0[refL0][i];
            ctx->w1[1+i]    = ctx->chroma_weight_l1[refL1][i];
            ctx->o0[1+i]    = ctx->chroma_offset_l0[refL0][i];
            ctx->o1[1+i]    = ctx->chroma_offset_l1[refL1][i];
        }
    }

}



/* gets called for every 4x4 sub macroblock every time, even if they belong to a 16x16 inter mb
 * TODO: adaptive inter pred following mb partitioning (avoid calling this 16 times for a 16x16 mb when inter pred could be done in one shot)
 */
void inter_pred_single(Macroblock *mb, int idx, MotionVector *mv, bool l0, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic  = l0 ? ctx->dpb->l0[mv->ref_idx] : ctx->dpb->l1[mv->ref_idx];
    bool weighted = ctx->wpred_active;

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

    if (debugging && idx == 0) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                fprintf(stderr, "%3d ", refPic->luma[(yBase+yOffInt+y)*refPic->widthY + xBase+xOffInt+x]);
            }
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "\n");
    }

    if (weighted) {
        int logWD = ctx->logWD[0];
        int w = l0 ? ctx->w0[0] : ctx->w1[0];
        int o = l0 ? ctx->o0[0] : ctx->o1[0];

        dst = &currPic->luma[yBase*stride + xBase];
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int t0 = dst[x];
                if (logWD >= 1) dst[x] = _clip1y(((t0 * w + (1 << (logWD-1))) >> logWD) + o, 8);
                else            dst[x] = _clip1y(t0 * w + o, 8);
            }
            dst += stride;
        }
    }
}


void inter_pred_bi(Macroblock *mb, int idx, MotionVector *mvL0, MotionVector *mvL1, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic0 = ctx->dpb->l0[mvL0->ref_idx];
    Picture *refPic1 = ctx->dpb->l1[mvL1->ref_idx];
    bool weighted = ctx->wpred_active;


    MotionVector *mvList[2] = {mvL0, mvL1};
    Picture *picList[2] = {refPic0, refPic1};

    // temp buffer for sample accumulation
    uint8_t temp[2][4][4];

    // top-left sample coords relative to picture
    int yBase = mb->mb_y*16 + ((idx>>2) << 2);
    int xBase = mb->mb_x*16 + ((idx&3)  << 2);



    for (int i = 0; i < 2; i++) {
        MotionVector *mv = mvList[i];
        Picture *refPic = picList[i];

        uint8_t *dst = &temp[i][0][0];

        // mv offsets
        int xOffInt  = mv->x >> 2;
        int yOffInt  = mv->y >> 2;
        int xFrac    = mv->x & 3;
        int yFrac    = mv->y & 3;


        fetch_9x9_block(refPic, yBase + yOffInt, xBase + xOffInt, ctx->ref_samples, ctx);

        qpel_funcs[(yFrac<<2) | xFrac] (ctx->ref_samples, dst, 4, ctx->ps->sps->bit_depth_luma_minus8 + 8);
        // qpel_00(ref_samples, dst, 4, ctx->ps->sps->bit_depth_luma_minus8 + 8); // fun testing
    }


    int logWD = ctx->logWD[0];
    int w0 = ctx->w0[0];
    int w1 = ctx->w1[0];
    int o0 = ctx->o0[0];
    int o1 = ctx->o1[0];


    int stride = currPic->widthY;
    uint8_t *dst = &currPic->luma[yBase * stride + xBase];
    if (!weighted) {
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                dst[x] = (uint8_t) (((unsigned)temp[0][y][x] + (unsigned)temp[1][y][x] + 1) >> 1);
            }
            dst += stride;
        }
    } else {
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int t0 = temp[0][y][x];
                int t1 = temp[1][y][x];
                dst[x] = (uint8_t) _clip1y(((t0 * w0 + t1 * w1 + (1<<logWD)) >>
                                              (logWD + 1)) + ((o0 + o1 + 1) >> 1), 8);
            }
            dst += stride;
        }
    }
}


void inter_pred_chroma_single(Macroblock *mb, int idx, MotionVector *mv, bool l0, CodecContext *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic  = l0 ? ctx->dpb->l0[mv->ref_idx] : ctx->dpb->l1[mv->ref_idx];
    bool weighted = ctx->wpred_active;

    const int yBase = mb->mb_y*8 + ((idx>>2) << 1);
    const int xBase = mb->mb_x*8 + ((idx&3)  << 1);

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

    if (weighted) {
        dstCb = &currPic->cb[yBase*stride + xBase];
        dstCr = &currPic->cr[yBase*stride + xBase];

        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t *ptr = iCbCr ? dstCr : dstCb;

            int logWD = ctx->logWD[1+iCbCr];
            int w = l0 ? ctx->w0[1+iCbCr] : ctx->w1[1+iCbCr];
            int o = l0 ? ctx->o0[1+iCbCr] : ctx->o1[1+iCbCr];

            for (int y = 0; y < 2; y++) {
                for (int x = 0; x < 2; x++) {
                    int t = ptr[x];
                    ptr[x] = logWD >= 1
                            ? (uint8_t) _clip1c(((t * w + (1 << (logWD-1))) >> logWD) + o, 8)
                            : (uint8_t) _clip1c(t * w + o, 8);
                }
                ptr += stride;
            }
        }
    }

}


void inter_pred_chroma_bi(Macroblock *mb, int idx, MotionVector *mvL0, MotionVector *mvL1, CodecContext *ctx) {
    Picture *currPic = mb->p_pic;
    Picture *refPic0 = ctx->dpb->l0[mvL0->ref_idx];
    Picture *refPic1 = ctx->dpb->l1[mvL1->ref_idx];
    bool weighted = ctx->wpred_active;


    MotionVector *mvList[2] = {mvL0, mvL1};
    Picture *picList[2] = {refPic0, refPic1};

    uint8_t temp_cb[2][2][2];
    uint8_t temp_cr[2][2][2];

    const int yBase = mb->mb_y*8 + ((idx>>2) << 1);
    const int xBase = mb->mb_x*8 + ((idx&3)  << 1);




    for (int i = 0; i < 2; i++) {
        MotionVector *mv = mvList[i];
        Picture *refPic = picList[i];

        uint8_t *dstCb = &temp_cb[i][0][0];
        uint8_t *dstCr = &temp_cr[i][0][0];

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

    if (!weighted) {
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                dstCb[x] = (uint8_t) (((uint32_t)temp_cb[0][y][x] + (uint32_t)temp_cb[1][y][x] + 1) >> 1);
                dstCr[x] = (uint8_t) (((uint32_t)temp_cr[0][y][x] + (uint32_t)temp_cr[1][y][x] + 1) >> 1);
            }
            dstCb += stride;
            dstCr += stride;
        }
    } else {
        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t *ptr = iCbCr ? dstCr : dstCb;
            uint8_t (*temp)[2][2] = iCbCr ? temp_cr : temp_cb;

            int logWD = ctx->logWD[1+iCbCr];
            int w0 = ctx->w0[1+iCbCr];
            int w1 = ctx->w1[1+iCbCr];
            int o0 = ctx->o0[1+iCbCr];
            int o1 = ctx->o1[1+iCbCr];

            for (int y = 0; y < 2; y++) {
                for (int x = 0; x < 2; x++) {
                    int t0 = temp[0][y][x];
                    int t1 = temp[1][y][x];
                    ptr[x] = (uint8_t) _clip1c(((t0 * w0 + t1 * w1 + (1 << logWD)) >>
                                            (logWD + 1)) + ((o0 + o1 + 1) >> 1), 8);
                }
                ptr += stride;
            }
        }
    }
}