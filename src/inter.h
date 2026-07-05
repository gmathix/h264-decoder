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



void derive_pred_weights(int refL0, int refL1, bool predFlagL0, bool predFlagL1, CodecContext *ctx);

void inter_pred_single(Macroblock *mb, int idx, MotionVector *mv, bool l0, CodecContext *ctx);
void inter_pred_bi(Macroblock *mb, int idx, MotionVector *mvL0, MotionVector *mvL1, CodecContext *ctx);
void inter_pred_chroma_single(Macroblock *mb, int idx, MotionVector *mv, bool l0, CodecContext *ctx);
void inter_pred_chroma_bi(Macroblock *mb, int idx, MotionVector *mvL0, MotionVector *mvL1, CodecContext *ctx);

void weight_pred_single(Macroblock *mb, int idx, bool l0, CodecContext *ctx);
void weight_pred_bi(Macroblock *mb, int idx, CodecContext *ctx);



static ALWAYS_INLINE void fetch_ref_mb(Macroblock *mb, int refIdx, CodecContext *ctx) {
    Picture *ref = ctx->dpb->l0[1+refIdx];
    for (int i = 0; i < 16; i++) {
        void *a = &ref->luma[mb->mb_y * (16+i) * ref->widthY + mb->mb_x * 16];
    }
}

/* fetches the 9x9 block with the 4x4 prediction block in the center
 * with extra pixels around it for the 6-tap filter
*/
static ALWAYS_INLINE void fetch_9x9_block(Picture *refPic, int y, int x, uint8_t ref_samples[9][9], CodecContext *ctx) {
	int width  = refPic->widthY;
	int height = refPic->heightY;
    int yc, xc;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            yc = _clip3(0, height - 1, y-2+i);
            xc = _clip3(0, width - 1, x-2+j);
            ref_samples[i][j] = refPic->luma[yc*width + xc];
        }
    }
}

static ALWAYS_INLINE void fetch_3x3_block_chroma(Picture *refPic, int y, int x,
    uint8_t ref_samples_cb[3][3], uint8_t ref_samples_cr[3][3], CodecContext *ctx) {

	int width  = refPic->widthC;
    int height = refPic->heightC;
    int yc, xc;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            yc = _clip3(0, height - 1, y+i);
            xc = _clip3(0, width - 1, x+j);
            ref_samples_cb[i][j] = refPic->cb[yc*width + xc];
            ref_samples_cr[i][j] = refPic->cr[yc*width + xc];
        }
    }
}





#endif //H264_DECODER_INTER_H