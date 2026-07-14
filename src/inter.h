//
// Created by gmathix on 5/2/26.
//

#ifndef H264_DECODER_INTER_H
#define H264_DECODER_INTER_H


#include "global.h"


#include "decoder.h"
#include "mb.h"
#include "motion_info.h"
#include "picture.h"




void derive_pred_weights(int refL0, int refL1, bool predFlagL0, bool predFlagL1, CodecContext *ctx);


void inter_pred_single(Macroblock *mb, int pos4x4, MotionVector mv, int list,
    int width, int height, uint8_t *scratch_buf, CodecContext *ctx);
void inter_pred_bi(Macroblock *mb, int pos4x4, MotionVector mvL0, MotionVector mvL1,
    int width, int height, uint8_t *scratch_buf, uint8_t *temp_bi_buf, CodecContext *ctx);
void inter_pred_chroma_single(Macroblock *mb, int pos2x2, MotionVector mv, int list,
    int width, int height, uint8_t *scratch_buf, CodecContext *ctx);
void inter_pred_chroma_bi(Macroblock *mb, int pos2x2, MotionVector mvL0, MotionVector mvL1,
    int width, int height, uint8_t *scratch_buf,  uint8_t *temp_bi_buf, CodecContext *ctx);


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

static ALWAYS_INLINE void fetch_ref_block(const uint8_t * restrict ref, uint8_t * restrict scratch_buf,
    int picW, int picH, int y, int x, int width, int height) {

    width += 5;  // 5 extra samples for qpel
    height += 5;
    int yc, xc;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            yc = _clip3(0, picH - 1, y-2+i);
            xc = _clip3(0, picW - 1, x-2+j);
            scratch_buf[i*width + j] = ref[yc*picW + xc];
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