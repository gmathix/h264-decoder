//
// Created by gmathix on 8/25/26.
//


//
// Created by gmathix on 4/6/26.
//



#include "global.h"
#include "mb.h"
#include "util/formulas.h"
#include "util/mbutil.h"
#include "transform_common.h"




static always_inline void idct_4x4(int16_t d[4][4], uint8_t *dst, int stride, int bitDepth) {

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

      for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                  dst[x] = _clip3(0, (1<<bitDepth)-1, dst[x] + ((h[y][x] + (1 << 5)) >> 6));
            }
            dst += stride;
      }
}

static always_inline void idct_8x8(int16_t d[8][8], uint8_t *dst, int stride, int bitDepth) {

    int t0,t1,t2,t3,t4,t5,t6,t7;
    int f0,f1,f2,f3,f4,f5,f6,f7;
    int g[8][8];
    int m[8][8];
    for (int i = 0; i < 8; i++) {
        t0 = d[i][0] + d[i][4];
        t1 = -d[i][3] + d[i][5] - d[i][7] - (d[i][7] >> 1);
        t2 = d[i][0] - d[i][4];
        t3 = d[i][1] + d[i][7] - d[i][3] - (d[i][3] >> 1);
        t4 = (d[i][2] >> 1) - d[i][6];
        t5 = -d[i][1] + d[i][7] + d[i][5] + (d[i][5] >> 1);
        t6 = d[i][2] + (d[i][6] >> 1);
        t7 = d[i][3] + d[i][5] + d[i][1] + (d[i][1] >> 1);

        f0 = t0 + t6;
        f1 = t1 + (t7 >> 2);
        f2 = t2 + t4;
        f3 = t3 + (t5 >> 2);
        f4 = t2 - t4;
        f5 = (t3 >> 2) - t5;
        f6 = t0 - t6;
        f7 = t7 - (t1 >> 2);

        g[i][0] = f0 + f7;
        g[i][1] = f2 + f5;
        g[i][2] = f4 + f3;
        g[i][3] = f6 + f1;
        g[i][4] = f6 - f1;
        g[i][5] = f4 - f3;
        g[i][6] = f2 - f5;
        g[i][7] = f0 - f7;
    }

    for (int j = 0; j < 8; j++) {
        t0 = g[0][j] + g[4][j];
        t1 = -g[3][j] + g[5][j] - g[7][j] - (g[7][j] >> 1);
        t2 = g[0][j] - g[4][j];
        t3 = g[1][j] + g[7][j] - g[3][j] - (g[3][j] >> 1);
        t4 = (g[2][j] >> 1) - g[6][j];
        t5 = -g[1][j] + g[7][j] + g[5][j] + (g[5][j] >> 1);
        t6 = g[2][j] + (g[6][j] >> 1);
        t7 = g[3][j] + g[5][j] + g[1][j] + (g[1][j] >> 1);

        f0 = t0 + t6;
        f1 = t1 + (t7 >> 2);
        f2 = t2 + t4;
        f3 = t3 + (t5 >> 2);
        f4 = t2 - t4;
        f5 = (t3 >> 2) - t5;
        f6 = t0 - t6;
        f7 = t7 - (t1 >> 2);

        m[0][j] = f0 + f7;
        m[1][j] = f2 + f5;
        m[2][j] = f4 + f3;
        m[3][j] = f6 + f1;
        m[4][j] = f6 - f1;
        m[5][j] = f4 - f3;
        m[6][j] = f2 - f5;
        m[7][j] = f0 - f7;
    }

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst[x] = _clip3(0, (1<<bitDepth)-1, dst[x] + ((m[y][x] + (1 << 5)) >> 6));
        }
        dst += stride;
    }
}





void transform_luma_4x4(Macroblock *mb, int blkIdx, const Undo264Context *ctx) {
      static int16_t c[4][4];
      static int16_t d[4][4];

      int stride = mb->p_pic->widthY;

      int blkY = (blkIdx>>2)<<2;
      int blkX = (blkIdx&3)<<2;
      int qp = mb->QPY;

      inverse_4x4_coeff_scaling_scan(mb->residuals.luma_4x4_coeffs[blkIdx], c);

      int scaleIndex = 3 * (IS_INTER(mb->mb_type) > 0);
      int16_t (*scale) [4] = ctx->levelScale4x4[scaleIndex][qp];

      if (qp >= 24)   scaling_residual_4x4_lshift(qp/6-4, scale, c, d, true, ctx);
      else            scaling_residual_4x4_rshift_min(qp/6-4, scale, c, d, true, ctx);


      idct_4x4(
            d,
            &mb->p_pic->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
            stride, ctx->ps->sps->bit_depth_luma);
}


void transform_luma_8x8(Macroblock *mb, int i8x8, const Undo264Context *ctx) {
      static int16_t c[8][8];
      static int16_t d[8][8];

      int stride = mb->p_pic->widthY;

      int blkY = (i8x8>>1)<<3;
      int blkX = (i8x8&1)<<3;
      int qp = mb->QPY;

      inverse_8x8_coeff_scaling_scan(mb->residuals.luma_8x8_coeffs[i8x8], c);

      int scaleIndex = IS_INTER(mb->mb_type) > 0;
      int16_t (*scale) [8] = ctx->levelScale8x8[scaleIndex][qp];

      if (qp >= 36)   scaling_residual_8x8_lshift(qp/6-6, scale, c, d, true, ctx);
      else            scaling_residual_8x8_rshift_min(qp/6-6, scale, c, d, true, ctx);

      idct_8x8(
            d,
            &mb->p_pic->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
            stride, ctx->ps->sps->bit_depth_luma);
}

void transform_luma_16x16(Macroblock *mb, const Undo264Context *ctx) {
      static int16_t c[4][4];
      inverse_4x4_coeff_scaling_scan(mb->residuals.luma_16x16_DC, c);

      static int32_t dcY[4][4];
      static int32_t temp[4][4];

      int qp = mb->QPY;

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
      int scaleIndex = 3 * (IS_INTER(mb->mb_type) > 0);
      for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                  dcY[i][j] = shiftfunc(c[i][j] * ctx->levelScale4x4[scaleIndex][qp][0][0], qp/6-6);
            }
      }

      int16_t (*scale) [4] = ctx->levelScale4x4[scaleIndex][qp];

      /* scaling & transform */
      static int16_t d[4][4];
      int stride = mb->p_pic->widthY;
      for (int i = 0; i < 16; i++) {
            inverse_4x4_coeff_scaling_scan_dc(mb->residuals.luma_16x16_AC[i], dcY[i>>2][i&3], c);

            if (qp >= 24)   scaling_residual_4x4_lshift(qp/6-4, scale, c, d, true, ctx);
            else            scaling_residual_4x4_rshift_min(qp/6-4, scale, c, d, true, ctx);
            d[0][0] = c[0][0];

            int blkY = (i>>2) << 2;
            int blkX = (i&3) << 2;
            idct_4x4(
                  d,
                  &mb->p_pic->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
                  stride, ctx->ps->sps->bit_depth_luma);
      }
}


void transform_chroma(Macroblock *mb, const Undo264Context *ctx) {

      int nbCr4x4 = (mb->mb_height_c/4) * (mb->mb_width_c/4);

      for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            int qp = mb->QPC[iCbCr];

            uint8_t *dst = iCbCr
                  ? mb->p_pic->cr
                  : mb->p_pic->cb;

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


                  int scaleIndex = iCbCr + 1 + 3 * (IS_INTER(mb->mb_type) > 0);
                  if (ctx->ps->sps->chroma_format_idc == 1) {
                        for (int i = 0; i < 2; i++)
                              for (int j = 0; j < 2; j++)
                                    dcC[i][j] = ((f[i][j] * ctx->levelScale4x4[scaleIndex][qp][0][0]) * (1 << (qp/6))) >> 5;
                  }


                  static int16_t  d[4][4];
                  int stride = mb->p_pic->widthC;
                  for (int i4x4 = 0; i4x4 < nbCr4x4; i4x4++) {
                        static int16_t c[4][4];
                        inverse_4x4_coeff_scaling_scan_dc(mb->residuals.chroma_AC[iCbCr][i4x4], dcC[i4x4>>1][i4x4&1], c);

                        int16_t (*scale) [4] = ctx->levelScale4x4[scaleIndex][qp];

                        if (qp >= 24)   scaling_residual_4x4_lshift(qp/6-4, scale, c, d, false, ctx);
                        else            scaling_residual_4x4_rshift_min(qp/6-4, scale, c, d, false, ctx);
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










