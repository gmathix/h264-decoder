//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_TRANSFORM_H
#define TOY_H264_TRANSFORM_H


#include "decoder.h"
#include "mb.h"
#include "util/mbutil.h"

#include <emmintrin.h>




extern const uint8_t blk_scan_8x8[8][8];

void transform_luma_4x4(Macroblock *mb, int qp, int blkIdx, CodecContext *ctx);
void transform_luma_8x8(Macroblock *mb, CodecContext *ctx);
void transform_luma_16x16(Macroblock *mb, int qp, CodecContext *ctx);
void transform_chroma(Macroblock *mb, CodecContext *ctx);



static ALWAYS_INLINE void scaling_residual_4x4_lshift(int shift, int16_t (*scale)[4], int32_t c[4][4], int32_t d[4][4], bool is_luma, CodecContext *ctx) {
    for (int i = 0; i < 4; i++) {
        for (int j =0 ; j < 4; j++) {
            d[i][j] = lshift(c[i][j] * scale[i][j], shift);
        }
    }
}
static ALWAYS_INLINE void scaling_residual_4x4_rshift_min(int shift, int16_t (*scale)[4], int32_t c[4][4], int32_t d[4][4], bool is_luma, CodecContext *ctx) {
    for (int i = 0; i < 4; i++) {
        for (int j =0 ; j < 4; j++) {
            d[i][j] = rshift_min(c[i][j] * scale[i][j], shift);
        }
    }
}



static void idct_4x4(int32_t d[4][4], uint8_t *dst, int stride, int bitDepth) {

    int t0,t1,t2,t3;
    int f[4][4];
    int h[4][4];

    for (int i = 0; i < 4; i++) {
        t0 = d[i][0] + d[i][2];
        t1 = d[i][0] - d[i][2];
        t2 = (d[i][1] >> 1) - d[i][3];
        t3 = d[i][1] + (d[i][3] >> 1);

        f[i][0] = t0 + t3;
        f[i][1] = t1 + t2;
        f[i][2] = t1 - t2;
        f[i][3] = t0 - t3;
    }

    for (int j = 0; j < 4; j++) {
        t0 = f[0][j] + f[2][j];
        t1 = f[0][j] - f[2][j];
        t2 = (f[1][j] >> 1) - f[3][j];
        t3 = f[1][j] + (f[3][j] >> 1);

        h[0][j] = t0 + t3;
        h[1][j] = t1 + t2;
        h[2][j] = t1 - t2;
        h[3][j] = t0 - t3;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            *dst = _clip3(0, (1<<bitDepth)-1, *dst + ((h[i][j] + (1 << 5)) >> 6));
            dst++;
        }
        dst += stride-4;
    }
}


static ALWAYS_INLINE void inverse_4x4_coeff_scaling_scan(int16_t vals[16], int32_t out[4][4]) {
    out[0][0] = vals[0];   out[0][1] = vals[1];   out[0][2] = vals[5];   out[0][3] = vals[6];
    out[1][0] = vals[2];   out[1][1] = vals[4];   out[1][2] = vals[7];   out[1][3] = vals[12];
    out[2][0] = vals[3];   out[2][1] = vals[8];   out[2][2] = vals[11];  out[2][3] = vals[13];
    out[3][0] = vals[9];   out[3][1] = vals[10];  out[3][2] = vals[14];  out[3][3] = vals[15];
}
static ALWAYS_INLINE void inverse_4x4_coeff_scaling_scan_dc(int16_t AC[15], int dc, int32_t out[4][4]) {
    out[0][0] = dc;      out[0][1] = AC[0];   out[0][2] = AC[4];   out[0][3] = AC[5];
    out[1][0] = AC[1];   out[1][1] = AC[3];   out[1][2] = AC[6];   out[1][3] = AC[11];
    out[2][0] = AC[2];   out[2][1] = AC[7];   out[2][2] = AC[10];  out[2][3] = AC[12];
    out[3][0] = AC[8];   out[3][1] = AC[9];   out[3][2] = AC[13];  out[3][3] = AC[14];
}


static ALWAYS_INLINE void inverse_8x8_coeff_scaling_scan(int16_t vals[64], int32_t out[8][8]) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            out[i][j] = vals[blk_scan_8x8[i][j]];
        }
    }
}




#endif //TOY_H264_TRANSFORM_H