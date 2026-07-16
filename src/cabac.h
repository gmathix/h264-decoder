//
// Created by gmathix on 7/16/26.
//

#ifndef H264_DECODER_CABAC_H
#define H264_DECODER_CABAC_H
#include "mb.h"


void  residual_block_cabac   (Macroblock *mb, int blkIdx, int iCbCr, int pbt, int16_t coeffLevel[], uint8_t (*total_coeffs_table)[16], int startIdx, int endIdx, int maxNumCoeff, bool isLuma, struct SliceHeader *sh, CodecContext *ctx);



#endif //H264_DECODER_CABAC_H