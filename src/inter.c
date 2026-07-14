//
// Created by gmathix on 5/2/26.
//

#include "inter.h"

#include "dpb.h"
#include "qpel.h"
#include "util/sliceutil.h"


void derive_pred_weights(int refL0, int refL1, bool predFlagL0, bool predFlagL1, CodecContext *ctx) {
    Picture *currPic = ctx->curr_pic;
    SliceHeader *sh  = currPic->sh;
    PPS *pps         = sh->pps;

    bool implicitMode = (pps->weighted_bipred_idc == 2 && IS_B_SLICE(sh->slice_type) && predFlagL0 && predFlagL1);
    bool explicitMode = !implicitMode &&
                        ((pps->weighted_bipred_idc == 1 && IS_B_SLICE(sh->slice_type) && (predFlagL0 || predFlagL1)) ||
                        (pps->weighted_pred_flag == 1 && IS_P_SLICE(sh->slice_type) && predFlagL0));

    ctx->wpred.is_active = implicitMode || explicitMode;


    if (implicitMode) {
        for (int i = 0; i < 3; i++) {
            ctx->wpred.logWD[i] = 5;
            ctx->wpred.offset[L0][i] = ctx->wpred.offset[L1][i] = 0;
        }

        Picture *pic0 = ctx->dpb->lists[L0][1+refL0];
        Picture *pic1 = ctx->dpb->lists[L1][1+refL1];

        int td = _clip3(-128, 127, pic1->poc - pic0->poc);

        if (td == 0 ||
            (pic0->dpb_status == LONG_TERM_REF || pic1->dpb_status == LONG_TERM_REF)) {
            for (int i = 0; i < 3; i++) {
                ctx->wpred.weight[L0][i] = ctx->wpred.weight[L1][i] = 32;
            }
        } else {
            int tb = _clip3(-128, 127, currPic->poc - pic0->poc);
            int tx = (16384 + _abs(td / 2)) / td;
            int distScaleFactor = _clip3(-1024, 1023, (tb * tx + 32) >> 6);

            if (((distScaleFactor >> 2) < -64) || ((distScaleFactor >> 2) > 128)) {
                for (int i = 0; i < 3; i++) {
                    ctx->wpred.weight[L0][i] = ctx->wpred.weight[L1][i] = 32;
                }
            } else {
                for (int i = 0; i < 3; i++) {
                    ctx->wpred.weight[L0][i] = 64 - (distScaleFactor >> 2);
                    ctx->wpred.weight[L1][i] = distScaleFactor >> 2;
                }
            }
        }
    }
    else if (explicitMode) {
        ctx->wpred.logWD[0] = ctx->wpred.luma_log2_weight_denom;
        ctx->wpred.weight[L0][0]    = ctx->wpred.luma_weight[L0][refL0];
        ctx->wpred.weight[L1][0]    = ctx->wpred.luma_weight[L1][refL1];
        ctx->wpred.offset[L0][0]    = ctx->wpred.luma_offset[L0][refL0];
        ctx->wpred.offset[L1][0]    = ctx->wpred.luma_offset[L1][refL1];

        for (int i = 0; i < 2; i++) {
            ctx->wpred.logWD[1+i] = ctx->wpred.chroma_log2_weight_denom;
            ctx->wpred.weight[L0][1+i]    = ctx->wpred.chroma_weight[L0][refL0][i];
            ctx->wpred.weight[L1][1+i]    = ctx->wpred.chroma_weight[L1][refL1][i];
            ctx->wpred.offset[L0][1+i]    = ctx->wpred.chroma_offset[L0][refL0][i];
            ctx->wpred.offset[L1][1+i]    = ctx->wpred.chroma_offset[L1][refL1][i];
        }
    }

}




void inter_pred_single(Macroblock *mb, int pos4x4, MotionVector mv, int list,
    int width, int height, uint8_t * restrict scratch_buf, CodecContext *ctx) {

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

    fetch_ref_block(refPic->luma, scratch_buf, refPic->widthY, refPic->heightY, yBase + yOffInt, xBase + xOffInt, width, height);

    qpel_funcs[(yFrac<<2) | xFrac] (scratch_buf, dst, stride, width, height);

    if (weighted) {
        int logWD = ctx->wpred.logWD[0];
        int w = ctx->wpred.weight[list][0];
        int o = ctx->wpred.offset[list][0];

        dst = &currPic->luma[yBase*stride + xBase];
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int t0 = dst[x];
                if (logWD >= 1) dst[x] = _clip1y(((t0 * w + (1 << (logWD-1))) >> logWD) + o, 8);
                else            dst[x] = _clip1y(t0 * w + o, 8);
            }
            dst += stride;
        }
    }
}


void inter_pred_bi(Macroblock *mb, int pos4x4, MotionVector mvL0, MotionVector mvL1,
    int width, int height, uint8_t *scratch_buf, uint8_t *temp_bi_buf, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *picL0 = ctx->dpb->lists[L0][1+mvL0.ref_idx];
    Picture *picL1 = ctx->dpb->lists[L1][1+mvL1.ref_idx];
    bool weighted = ctx->wpred.is_active;

    MotionVector mvList[2] = {mvL0, mvL1};
    Picture *picList[2] = {picL0, picL1};

    int yBase = mb->mb_y*16 + ((pos4x4>>2) << 2);
    int xBase = mb->mb_x*16 + ((pos4x4&3)  << 2);

    int dimension = width * height;

    for (int i = 0; i < 2; i++) {
        MotionVector mv = mvList[i];
        Picture *refPic = picList[i];

        uint8_t *dst = &temp_bi_buf[i * dimension];

        int xOffInt  = mv.x >> 2;
        int yOffInt  = mv.y >> 2;
        int xFrac    = mv.x & 3;
        int yFrac    = mv.y & 3;

        fetch_ref_block(refPic->luma, scratch_buf, refPic->widthY, refPic->heightY, yBase + yOffInt, xBase + xOffInt, width, height);

        qpel_funcs[(yFrac << 2) | xFrac] (scratch_buf, dst, width, width, height);
    }

    int logWD = ctx->wpred.logWD[0];
    int w0 = ctx->wpred.weight[L0][0];
    int w1 = ctx->wpred.weight[L1][0];
    int o0 = ctx->wpred.offset[L0][0];
    int o1 = ctx->wpred.offset[L1][0];


    int stride = currPic->widthY;
    uint8_t *dst = &currPic->luma[yBase * stride + xBase];
    if (!weighted) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                dst[x] = (uint8_t) (((unsigned)temp_bi_buf[y*width + x] + (unsigned)temp_bi_buf[dimension + y*width + x] + 1) >> 1);
            }
            dst += stride;
        }
    } else {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int t0 = temp_bi_buf[y*width + x];
                int t1 = temp_bi_buf[dimension + y*width + x];
                dst[x] = (uint8_t) _clip1y(((t0 * w0 + t1 * w1 + (1<<logWD)) >>
                                              (logWD + 1)) + ((o0 + o1 + 1) >> 1), 8);
            }
            dst += stride;
        }
    }
}

void inter_pred_chroma_single(Macroblock *mb, int pos4x4, MotionVector mv, int list,
    int width, int height, uint8_t *scratch_buf, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *refPic  = ctx->dpb->lists[list][1+mv.ref_idx];
    bool weighted = ctx->wpred.is_active;

    const int yBase = mb->mb_y*8 + ((pos4x4>>2) << 1);
    const int xBase = mb->mb_x*8 + ((pos4x4&3)  << 1);

    int stride = currPic->widthC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];

    const int xOffInt  = mv.x >> 3;
    const int yOffInt  = mv.y >> 3;
    const int xFrac    = mv.x & 7;
    const int yFrac    = mv.y & 7;

    uint8_t *refs[2] = {refPic->cb, refPic->cr};
    uint8_t *dests[2] = {dstCb, dstCr};
    for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
        fetch_ref_block(refs[iCbCr], scratch_buf, refPic->widthC, refPic->heightC, yBase + yOffInt, xBase + xOffInt, width, height);
        uint8_t *dst = dests[iCbCr];

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                dst[j] =   ((8-xFrac) * (8-yFrac) * scratch_buf[(i+2)*(width+5) + 2+j]     +
                               xFrac  * (8-yFrac) * scratch_buf[(i+2)*(width+5) + 2+j+1]   +
                            (8-xFrac) *    yFrac  * scratch_buf[(i+3)*(width+5) + 2+j]   +
                               xFrac  *    yFrac  * scratch_buf[(i+3)*(width+5) + 2+j+1] + 32) >> 6;
            }
            dst += stride;
        }
    }



    if (weighted) {
        dstCb = &currPic->cb[yBase*stride + xBase];
        dstCr = &currPic->cr[yBase*stride + xBase];

        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t *ptr = iCbCr ? dstCr : dstCb;

            int logWD = ctx->wpred.logWD[1+iCbCr];
            int w = ctx->wpred.weight[list][1+iCbCr];
            int o = ctx->wpred.offset[list][1+iCbCr];

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
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


void inter_pred_chroma_bi(Macroblock *mb, int pos4x4, MotionVector mvL0, MotionVector mvL1,
    int width, int height, uint8_t *scratch_buf, uint8_t *temp_bi_buf, CodecContext *ctx) {

    Picture *currPic = mb->p_pic;
    Picture *ref0  = ctx->dpb->lists[L0][1+mvL0.ref_idx];
    Picture *ref1  = ctx->dpb->lists[L1][1+mvL1.ref_idx];
    bool weighted = ctx->wpred.is_active;

    const int yBase = mb->mb_y*8 + ((pos4x4>>2) << 1);
    const int xBase = mb->mb_x*8 + ((pos4x4&3)  << 1);

    int stride = currPic->widthC;
    uint8_t *dstCb = &currPic->cb[yBase*stride + xBase];
    uint8_t *dstCr = &currPic->cr[yBase*stride + xBase];

    MotionVector mvs[2] = {mvL0, mvL1};
    Picture *refs[2] = {ref0, ref1};
    uint8_t *refsCbCr[2][2] = { {refs[0]->cb, refs[1]->cb}, {refs[0]->cr, refs[1]->cr} };
    uint8_t *dests[2] = {dstCb, dstCr};

    int dimension = width * height;

    for (int iCbCr = 0; iCbCr < 2; iCbCr++) {

        for (int i = 0; i < 2; i++) {

            const int xOffInt  = mvs[i].x >> 3;
            const int yOffInt  = mvs[i].y >> 3;
            const int xFrac    = mvs[i].x & 7;
            const int yFrac    = mvs[i].y & 7;

            Picture *ref = refs[i];

            fetch_ref_block(refsCbCr[iCbCr][i], scratch_buf, ref->widthC, ref->heightC, yBase + yOffInt, xBase + xOffInt, width, height);
            uint8_t *dst = &temp_bi_buf[i * dimension];

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    dst[x] =   ((8-xFrac) * (8-yFrac) * scratch_buf[(y+2)*(width+5) + 2+x]     +
                                   xFrac  * (8-yFrac) * scratch_buf[(y+2)*(width+5) + 2+x+1]   +
                                (8-xFrac) *    yFrac  * scratch_buf[(y+3)*(width+5) + 2+x]   +
                                   xFrac  *    yFrac  * scratch_buf[(y+3)*(width+5) + 2+x+1] + 32) >> 6;
                }
                dst += width;
            }
        }

        uint8_t *dst = dests[iCbCr];
        if (!weighted) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    dst[x] = (uint8_t) (((uint32_t)temp_bi_buf[y*width + x] + (uint32_t)temp_bi_buf[dimension + y*width + x] + 1) >> 1);
                }
                dst += stride;
            }
        } else {
            int logWD = ctx->wpred.logWD[1+iCbCr];
            int w0 = ctx->wpred.weight[L0][1+iCbCr];
            int w1 = ctx->wpred.weight[L1][1+iCbCr];
            int o0 = ctx->wpred.offset[L0][1+iCbCr];
            int o1 = ctx->wpred.offset[L1][1+iCbCr];

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int t0 = temp_bi_buf[y*width + x];
                    int t1 = temp_bi_buf[dimension + y*width + x];
                    dst[x] = (uint8_t) _clip1c(((t0 * w0 + t1 * w1 + (1 << logWD)) >>
                                            (logWD + 1)) + ((o0 + o1 + 1) >> 1), 8);
                }
                dst += stride;
            }
        }
    }
}