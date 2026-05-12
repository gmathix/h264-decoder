//
// Created by gmathix on 5/2/26.
//

#ifndef H264_DECODER_INTER_H
#define H264_DECODER_INTER_H



#include "decoder.h"
#include "dpb.h"
#include "mb.h"
#include "mv.h"
#include "picture.h"





void inter_pred(Macroblock *mb, int idx, MotionVector *mv, CodecContext *ctx);
void inter_pred_chroma(Macroblock *mb, int idx, MotionVector *mv, CodecContext *ctx);



static ALWAYS_INLINE void fetch_ref_mb(Macroblock *mb, int refIdx, CodecContext *ctx) {
    Picture *ref = ctx->dpb->l0[refIdx];
    for (int i = 0; i < 16; i++) {
        void *a = &ref->luma[mb->mb_y * (16+i) * ref->strideY + mb->mb_x * 16];
    }
}

/* fetches the 9x9 block with the 4x4 prediction block in the center
 * with extra pixels around it for the 6-tap filter
*/
static ALWAYS_INLINE void fetch_9x9_block(Picture *refPic, int y, int x, uint8_t ref_samples[9][9], CodecContext *ctx) {
    int stride = refPic->strideY;
    int yc, xc;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            yc = _clip3(0, refPic->height-1, y-2+i);
            xc = _clip3(0, stride-1, x-2+j);
            ref_samples[i][j] = refPic->luma[yc*stride + xc];
        }
    }
}

static ALWAYS_INLINE void fetch_5x5_block_chroma(Picture *refPic, int y, int x,
    uint8_t ref_samples_cb[5][5], uint8_t ref_samples_cr[5][5], CodecContext *ctx) {

    int stride = refPic->strideC;
    int yc, xc;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            yc = _clip3(0, refPic->height/2 - 1, y+i);
            xc = _clip3(0, stride-1, x+j);
            ref_samples_cb[i][j] = refPic->cb[yc*stride + xc];
            ref_samples_cr[i][j] = refPic->cr[yc*stride + xc];
        }
    }
}





#endif //H264_DECODER_INTER_H