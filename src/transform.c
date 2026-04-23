//
// Created by gmathix on 4/6/26.
//

#include "transform.h"
#include "util/formulas.h"
#include "util/mbutil.h"

#include <string.h>

#include "dequant.h"
#include "picture.h"


const uint8_t field_scan_4x4[4][4] = {
      { 0,  2,  8, 12},
      { 1,  5,  9, 13},
      { 3,  6, 10, 14},
      { 4,  7, 11, 15}
};

const uint8_t blk_scan_8x8[8][8] = {
      { 0,  1,  5,  6, 14, 15, 27, 28},
      { 2,  4,  7, 13, 16, 26, 29, 42},
      { 3,  8, 12, 17, 25, 30, 41, 43},
      { 9, 11, 18, 24, 31, 40, 44, 53},
      {10, 19, 23, 32, 39, 45, 52, 54},
      {20, 22, 33, 38, 46, 51, 55, 60},
      {21, 34, 37, 47, 50, 56, 59, 61},
      {35, 36, 48, 49, 57, 58, 62, 63}
};

const uint8_t field_scan_8x8[8][8] = {
      { 0,  3,  8, 15, 22, 30, 38, 52},
      { 1,  4, 14, 21, 29, 37, 45, 53},
      { 2,  7, 16, 23, 31, 39, 46, 58},
      { 5,  9, 20, 28, 36, 44, 51, 59},
      { 6, 13, 24, 32, 40, 47, 54, 60},
      {10, 17, 25, 33, 41, 48, 55, 61},
      {11, 18, 26, 34, 42, 49, 56, 62},
      {12, 19, 27, 35, 43, 50, 57, 63}
};

const int16_t hadamard_4x4_mat[4][4] = {
      { 1,  1,  1,  1},
      { 1,  1, -1, -1},
      { 1, -1, -1,  1},
      { 1, -1,  1, -1}
};

const int16_t hadamard_2x2_mat[2][2] = {
      { 1,  1},
      { 1, -1}
};




void transform_luma_4x4(Macroblock *mb, int qp, int blkIdx, CodecContext *ctx) {
      int stride = mb->p_frame->strideY;

      int blkY = (blkIdx>>2)<<2;
      int blkX = (blkIdx&3)<<2;

      int c[4][4];
      inverse_4x4_coeff_scaling_scan(mb->residuals.luma_4x4_coeffs[blkIdx], c);

      int d[4][4];
      if (qp >= 24)   scaling_residual_4x4_lshift(qp/6-4, ctx->levelScaleTable[qp], c, d, true, ctx);
      else            scaling_residual_4x4_rshift_min(qp/6-4, ctx->levelScaleTable[qp], c, d, true, ctx);


      idct_4x4(
            d,
            &mb->p_frame->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
            stride, ctx->ps->sps->bit_depth_luma);
}


void transform_luma_8x8(Macroblock *mb, CodecContext *ctx) {

}


void transform_luma_16x16(Macroblock *mb, int qp,CodecContext *ctx) {
      int32_t c[4][4];
      inverse_4x4_coeff_scaling_scan(mb->residuals.luma_16x16_DC, c);

      int32_t dcY[4][4];
      int32_t temp[4][4];

      /* hadamard on DC */
      for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                  temp[i][j] = c[0][j]*hadamard_4x4_mat[i][0] + c[1][j]*hadamard_4x4_mat[i][1] +
                        c[2][j]*hadamard_4x4_mat[i][2] + c[3][j]*hadamard_4x4_mat[i][3];
      for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                  c[i][j] = temp[i][0]*hadamard_4x4_mat[0][j] + temp[i][1]*hadamard_4x4_mat[1][j] +
                        temp[i][2]*hadamard_4x4_mat[2][j] + temp[i][3]*hadamard_4x4_mat[3][j];



      /* dequant */
      int32_t (*shiftfunc)(int32_t, int16_t) = qp >= 36 ? lshift : rshift_min;
      for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                  dcY[i][j] = shiftfunc(c[i][j] * ctx->levelScaleTable[qp][0][0], qp/6-6);
            }
      }


      /* scaling & transform */
      int d[4][4];
      int stride = mb->p_frame->strideY;
      for (int i = 0; i < 16; i++) {
            inverse_4x4_coeff_scaling_scan_dc(mb->residuals.luma_16x16_AC[i], dcY[i>>2][i&3], c);

            if (qp >= 24)   scaling_residual_4x4_lshift(qp/6-4, ctx->levelScaleTable[qp], c, d, true, ctx);
            else            scaling_residual_4x4_rshift_min(qp/6-4, ctx->levelScaleTable[qp], c, d, true, ctx);
            d[0][0] = c[0][0];

            int blkY = (i>>2) << 2;
            int blkX = (i&3) << 2;
            idct_4x4(
                  d,
                  &mb->p_frame->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
                  stride, ctx->ps->sps->bit_depth_luma);
      }
}


void transform_chroma(Macroblock *mb, CodecContext *ctx) {

      int nbCr4x4 = (mb->mb_height_c/4) * (mb->mb_width_c/4);

      int32_t qp = mb->QPC;

      for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t *dst = iCbCr
                  ? mb->p_frame->cr
                  : mb->p_frame->cb;

            if (mb->residuals.cbp_chroma & 3) {
                  int32_t c[2][2] = {
                        {mb->residuals.chroma_DC[iCbCr][0], mb->residuals.chroma_DC[iCbCr][1]},
                        {mb->residuals.chroma_DC[iCbCr][2], mb->residuals.chroma_DC[iCbCr][3]}
                  };

                  int32_t dcC[2][2];

                  int32_t temp[2][2];
                  int32_t f[2][2];
                  for (int i = 0; i < 2; i++)
                        for (int j = 0; j < 2; j++)
                              temp[i][j] = hadamard_2x2_mat[i][0]*c[0][j] + hadamard_2x2_mat[i][1]*c[1][j];
                  for (int i = 0; i < 2; i++)
                        for (int j = 0; j < 2; j++)
                              f[i][j] = temp[i][0]*hadamard_2x2_mat[0][j] + temp[i][1]*hadamard_2x2_mat[1][j];


                  if (ctx->ps->sps->chroma_format_idc == 1) {
                        for (int i = 0; i < 2; i++)
                              for (int j = 0; j < 2; j++)
                                    dcC[i][j] = ((f[i][j] * ctx->levelScaleTable[qp][0][0]) << (qp/6)) >> 5;
                  }


                  int32_t  d[4][4];
                  int stride = mb->p_frame->strideC;
                  for (int i4x4 = 0; i4x4 < nbCr4x4; i4x4++) {
                        int32_t c[4][4];
                        inverse_4x4_coeff_scaling_scan_dc(mb->residuals.chroma_AC[iCbCr][i4x4], dcC[i4x4>>1][i4x4&1], c);

                        if (qp >= 24)   scaling_residual_4x4_lshift(qp/6-4, ctx->levelScaleTable[qp], c, d, true, ctx);
                        else            scaling_residual_4x4_rshift_min(qp/6-4, ctx->levelScaleTable[qp], c, d, true, ctx);
                        d[0][0] = c[0][0];

                        int blkY = (i4x4>>1) << 2;
                        int blkX = (i4x4&1) << 2;
                        idct_4x4(
                              d,
                              &dst[(mb->mb_y*8 + blkY)*stride + mb->mb_x*8 + blkX],
                              stride, ctx->ps->sps->bit_depth_chroma);
                  }
            }
      }
}










