//
// Created by gmathix on 5/2/26.
//

#ifndef H264_DECODER_INTER_H
#define H264_DECODER_INTER_H


#include "global.h"


#include "decoder.h"
#include "slice.h"
#include "picture.h"


static void derive_pred_weights(int refL0, int refL1, bool predFlagL0, bool predFlagL1, Undo264Context *ctx) {
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
        ctx->wpred.logWD[0]      = ctx->wpred.luma_log2_weight_denom;
        ctx->wpred.weight[L0][0] = ctx->wpred.luma_weight[L0][refL0];
        ctx->wpred.weight[L1][0] = ctx->wpred.luma_weight[L1][refL1];
        ctx->wpred.offset[L0][0] = ctx->wpred.luma_offset[L0][refL0];
        ctx->wpred.offset[L1][0] = ctx->wpred.luma_offset[L1][refL1];

        for (int i = 0; i < 2; i++) {
            ctx->wpred.logWD[1+i]      = ctx->wpred.chroma_log2_weight_denom;
            ctx->wpred.weight[L0][1+i] = ctx->wpred.chroma_weight[L0][refL0][i];
            ctx->wpred.weight[L1][1+i] = ctx->wpred.chroma_weight[L1][refL1][i];
            ctx->wpred.offset[L0][1+i] = ctx->wpred.chroma_offset[L0][refL0][i];
            ctx->wpred.offset[L1][1+i] = ctx->wpred.chroma_offset[L1][refL1][i];
        }
    }
}

static ALWAYS_INLINE void fetch_ref_block(const uint8_t * restrict ref, uint8_t * restrict scratch_buf,
    int picW, int picH, int y, int x, int width, int height) {

    width += 5;
    height += 5;

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




#endif //H264_DECODER_INTER_H