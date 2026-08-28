//
// Created by gmathix on 8/25/26.
//


#include "global.h"

#include "mb.h"
#include "immintrin.h"
#include "transform_common.h"
#include "tests/profiler.h"
#include "util/formulas.h"
#include "util/mbutil.h"





#define TRANSPOSE4x4(l0, l1, l2, l3)                              \
      temp0 = _mm_unpacklo_epi16(l0, l1);                         \
      temp1 = _mm_unpacklo_epi16(l2, l3);                         \
                                                                  \
      temp2 = _mm_unpacklo_epi32(temp0, temp1);                   \
      temp3 = _mm_unpackhi_epi32(temp0, temp1);                   \
                                                                  \
      l0 = _mm_unpacklo_epi64(temp2, temp2);                      \
      l1 = _mm_unpackhi_epi64(temp2, temp2);                      \
      l2 = _mm_unpacklo_epi64(temp3, temp3);                      \
      l3 = _mm_unpackhi_epi64(temp3, temp3);                      \


static always_inline void idct_4x4_sse(__m128i *r01, __m128i *r23, uint8_t *dst, int stride, int bitDepth) {
    const uint64_t *p01 = (const uint64_t*)r01;
    const uint64_t *p23 = (const uint64_t*)r23;

    __m128i r0 = _mm_loadu_si64(&p01[0]);
    __m128i r1 = _mm_loadu_si64(&p01[1]);
    __m128i r2 = _mm_loadu_si64(&p23[0]);
    __m128i r3 = _mm_loadu_si64(&p23[1]);

    __m128i add = _mm_set1_epi16(1 << 5);

    __m128i temp0, temp1, temp2, temp3;
    TRANSPOSE4x4(r0, r1, r2, r3);

    __m128i t0 = _mm_add_epi16(r0, r2);
    __m128i t1 = _mm_sub_epi16(r0, r2);
    __m128i t2 = _mm_sub_epi16(_mm_srai_epi16(r1, 1), r3);
    __m128i t3 = _mm_add_epi16(r1, _mm_srai_epi16(r3, 1));

    __m128i f0 = _mm_add_epi16(t0, t3);
    __m128i f1 = _mm_add_epi16(t1, t2);
    __m128i f2 = _mm_sub_epi16(t1, t2);
    __m128i f3 = _mm_sub_epi16(t0, t3);

    TRANSPOSE4x4(f0, f1, f2, f3);

    t0 = _mm_add_epi16(f0, f2);
    t1 = _mm_sub_epi16(f0, f2);
    t2 = _mm_sub_epi16(_mm_srai_epi16(f1, 1), f3);
    t3 = _mm_add_epi16(f1, _mm_srai_epi16(f3, 1));

    f0 = _mm_add_epi16(t0, t3);
    f1 = _mm_add_epi16(t1, t2);
    f2 = _mm_sub_epi16(t1, t2);
    f3 = _mm_sub_epi16(t0, t3);

    __m128i dst0 = _mm_cvtepu8_epi16(_mm_loadu_si32(dst));
    __m128i dst1 = _mm_cvtepu8_epi16(_mm_loadu_si32(dst + stride));
    __m128i dst2 = _mm_cvtepu8_epi16(_mm_loadu_si32(dst + 2*stride));
    __m128i dst3 = _mm_cvtepu8_epi16(_mm_loadu_si32(dst + 3*stride));

    __m128i sum0 = _mm_srai_epi16(_mm_add_epi16(f0, add), 6);
    __m128i sum1 = _mm_srai_epi16(_mm_add_epi16(f1, add), 6);
    __m128i sum2 = _mm_srai_epi16(_mm_add_epi16(f2, add), 6);
    __m128i sum3 = _mm_srai_epi16(_mm_add_epi16(f3, add), 6);

    dst0 = _mm_packus_epi16(_mm_add_epi16(dst0, sum0), add);
    dst1 = _mm_packus_epi16(_mm_add_epi16(dst1, sum1), add);
    dst2 = _mm_packus_epi16(_mm_add_epi16(dst2, sum2), add);
    dst3 = _mm_packus_epi16(_mm_add_epi16(dst3, sum3), add);


    _mm_storeu_si32(dst, dst0);
    _mm_storeu_si32(dst+stride, dst1);
    _mm_storeu_si32(dst+2*stride, dst2);
    _mm_storeu_si32(dst+3*stride, dst3);
}



#define TRANSPOSE8x8(l0, l1, l2, l3, l4, l5, l6, l7) do {                           \
      __m128i _a0 = _mm_unpacklo_epi16(l0,l1), _a1 = _mm_unpackhi_epi16(l0,l1);     \
      __m128i _a2 = _mm_unpacklo_epi16(l2,l3), _a3 = _mm_unpackhi_epi16(l2,l3);     \
      __m128i _a4 = _mm_unpacklo_epi16(l4,l5), _a5 = _mm_unpackhi_epi16(l4,l5);     \
      __m128i _a6 = _mm_unpacklo_epi16(l6,l7), _a7 = _mm_unpackhi_epi16(l6,l7);     \
                                                                                    \
      __m128i _b0 = _mm_unpacklo_epi32(_a0,_a2), _b1 = _mm_unpackhi_epi32(_a0,_a2); \
      __m128i _b2 = _mm_unpacklo_epi32(_a1,_a3), _b3 = _mm_unpackhi_epi32(_a1,_a3); \
      __m128i _b4 = _mm_unpacklo_epi32(_a4,_a6), _b5 = _mm_unpackhi_epi32(_a4,_a6); \
      __m128i _b6 = _mm_unpacklo_epi32(_a5,_a7), _b7 = _mm_unpackhi_epi32(_a5,_a7); \
                                                                                    \
      l0 = _mm_unpacklo_epi64(_b0,_b4); l1 = _mm_unpackhi_epi64(_b0,_b4);           \
      l2 = _mm_unpacklo_epi64(_b1,_b5); l3 = _mm_unpackhi_epi64(_b1,_b5);           \
      l4 = _mm_unpacklo_epi64(_b2,_b6); l5 = _mm_unpackhi_epi64(_b2,_b6);           \
      l6 = _mm_unpacklo_epi64(_b3,_b7); l7 = _mm_unpackhi_epi64(_b3,_b7);           \
} while (0)                                                                         \



static always_inline void idct_8x8_sse(int16_t d[8][8], uint8_t *dst, int stride, int bitDepth) {
      __m128i r0 = _mm_loadu_si128((__m128i*)d[0]);
      __m128i r1 = _mm_loadu_si128((__m128i*)d[1]);
      __m128i r2 = _mm_loadu_si128((__m128i*)d[2]);
      __m128i r3 = _mm_loadu_si128((__m128i*)d[3]);
      __m128i r4 = _mm_loadu_si128((__m128i*)d[4]);
      __m128i r5 = _mm_loadu_si128((__m128i*)d[5]);
      __m128i r6 = _mm_loadu_si128((__m128i*)d[6]);
      __m128i r7 = _mm_loadu_si128((__m128i*)d[7]);

      TRANSPOSE8x8(r0, r1, r2, r3, r4, r5, r6, r7);

      __m128i t0 = _mm_add_epi16(r0, r4);
      __m128i t1 = _mm_sub_epi16(_mm_sub_epi16(r5, _mm_add_epi16(r7, _mm_srai_epi16(r7, 1))),r3);
      __m128i t2 = _mm_sub_epi16(r0, r4);
      __m128i t3 = _mm_add_epi16(_mm_sub_epi16(r7, _mm_add_epi16(r3, _mm_srai_epi16(r3, 1))),r1);
      __m128i t4 = _mm_sub_epi16(_mm_srai_epi16(r2, 1), r6);
      __m128i t5 = _mm_sub_epi16(_mm_add_epi16(r7, _mm_add_epi16(r5, _mm_srai_epi16(r5, 1))),r1);
      __m128i t6 = _mm_add_epi16(r2, _mm_srai_epi16(r6, 1));
      __m128i t7 = _mm_add_epi16(_mm_add_epi16(r5, _mm_add_epi16(r1, _mm_srai_epi16(r1, 1))),r3);

      __m128i f0 = _mm_add_epi16(t0, t6);
      __m128i f1 = _mm_add_epi16(t1, _mm_srai_epi16(t7, 2));
      __m128i f2 = _mm_add_epi16(t2, t4);
      __m128i f3 = _mm_add_epi16(t3, _mm_srai_epi16(t5, 2));
      __m128i f4 = _mm_sub_epi16(t2, t4);
      __m128i f5 = _mm_sub_epi16(_mm_srai_epi16(t3, 2), t5);
      __m128i f6 = _mm_sub_epi16(t0, t6);
      __m128i f7 = _mm_sub_epi16(t7, _mm_srai_epi16(t1, 2));

      __m128i g0 = _mm_add_epi16(f0, f7);
      __m128i g1 = _mm_add_epi16(f2, f5);
      __m128i g2 = _mm_add_epi16(f4, f3);
      __m128i g3 = _mm_add_epi16(f6, f1);
      __m128i g4 = _mm_sub_epi16(f6, f1);
      __m128i g5 = _mm_sub_epi16(f4, f3);
      __m128i g6 = _mm_sub_epi16(f2, f5);
      __m128i g7 = _mm_sub_epi16(f0, f7);

      TRANSPOSE8x8(g0, g1, g2, g3, g4, g5, g6, g7);

      t0 = _mm_add_epi16(g0, g4);
      t1 = _mm_sub_epi16(_mm_sub_epi16(g5, _mm_add_epi16(g7, _mm_srai_epi16(g7, 1))), g3);
      t2 = _mm_sub_epi16(g0, g4);
      t3 = _mm_add_epi16(g1, _mm_sub_epi16(g7, _mm_add_epi16(g3, _mm_srai_epi16(g3, 1))));
      t4 = _mm_sub_epi16(_mm_srai_epi16(g2, 1), g6);
      t5 = _mm_sub_epi16(_mm_add_epi16(g7, _mm_add_epi16(g5, _mm_srai_epi16(g5, 1))), g1);
      t6 = _mm_add_epi16(g2, _mm_srai_epi16(g6, 1));
      t7 = _mm_add_epi16(g3, _mm_add_epi16(g5, _mm_add_epi16(g1, _mm_srai_epi16(g1, 1))));

      f0 = _mm_add_epi16(t0, t6);
      f1 = _mm_add_epi16(t1, _mm_srai_epi16(t7, 2));
      f2 = _mm_add_epi16(t2, t4);
      f3 = _mm_add_epi16(t3, _mm_srai_epi16(t5, 2));
      f4 = _mm_sub_epi16(t2, t4);
      f5 = _mm_sub_epi16(_mm_srai_epi16(t3, 2), t5);
      f6 = _mm_sub_epi16(t0, t6);
      f7 = _mm_sub_epi16(t7, _mm_srai_epi16(t1, 2));

      g0 = _mm_add_epi16(f0, f7);
      g1 = _mm_add_epi16(f2, f5);
      g2 = _mm_add_epi16(f4, f3);
      g3 = _mm_add_epi16(f6, f1);
      g4 = _mm_sub_epi16(f6, f1);
      g5 = _mm_sub_epi16(f4, f3);
      g6 = _mm_sub_epi16(f2, f5);
      g7 = _mm_sub_epi16(f0, f7);

      __m128i dst0 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[0]));
      __m128i dst1 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[1*stride]));
      __m128i dst2 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[2*stride]));
      __m128i dst3 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[3*stride]));
      __m128i dst4 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[4*stride]));
      __m128i dst5 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[5*stride]));
      __m128i dst6 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[6*stride]));
      __m128i dst7 = _mm_cvtepu8_epi16(_mm_loadu_si64(&dst[7*stride]));

      __m128i add = _mm_set1_epi16(1 << 5);

      f0 = _mm_srai_epi16(_mm_add_epi16(g0, add), 6);
      f1 = _mm_srai_epi16(_mm_add_epi16(g1, add), 6);
      f2 = _mm_srai_epi16(_mm_add_epi16(g2, add), 6);
      f3 = _mm_srai_epi16(_mm_add_epi16(g3, add), 6);
      f4 = _mm_srai_epi16(_mm_add_epi16(g4, add), 6);
      f5 = _mm_srai_epi16(_mm_add_epi16(g5, add), 6);
      f6 = _mm_srai_epi16(_mm_add_epi16(g6, add), 6);
      f7 = _mm_srai_epi16(_mm_add_epi16(g7, add), 6);

      dst0 = _mm_packus_epi16(_mm_add_epi16(dst0, f0), add);
      dst1 = _mm_packus_epi16(_mm_add_epi16(dst1, f1), add);
      dst2 = _mm_packus_epi16(_mm_add_epi16(dst2, f2), add);
      dst3 = _mm_packus_epi16(_mm_add_epi16(dst3, f3), add);
      dst4 = _mm_packus_epi16(_mm_add_epi16(dst4, f4), add);
      dst5 = _mm_packus_epi16(_mm_add_epi16(dst5, f5), add);
      dst6 = _mm_packus_epi16(_mm_add_epi16(dst6, f6), add);
      dst7 = _mm_packus_epi16(_mm_add_epi16(dst7, f7), add);

      _mm_storeu_si64(&dst[0*stride], dst0);
      _mm_storeu_si64(&dst[1*stride], dst1);
      _mm_storeu_si64(&dst[2*stride], dst2);
      _mm_storeu_si64(&dst[3*stride], dst3);
      _mm_storeu_si64(&dst[4*stride], dst4);
      _mm_storeu_si64(&dst[5*stride], dst5);
      _mm_storeu_si64(&dst[6*stride], dst6);
      _mm_storeu_si64(&dst[7*stride], dst7);
}


static void scaling_residual_4x4_lshift_sse(int qp, __m128i *r01, __m128i *r23, __m128i scale01, __m128i scale23) {
      int16_t shift = qp/6-4;

      __m128i shift_reg = _mm_loadu_si16(&shift);

      *r01 = _mm_mullo_epi16(*r01, scale01);
      *r23 = _mm_mullo_epi16(*r23, scale23);

      *r01 = _mm_sll_epi16(*r01, shift_reg);
      *r23 = _mm_sll_epi16(*r23, shift_reg);
}


static void scaling_residual_4x4_rshift_min_sse(int qp, __m128i *r01, __m128i *r23, __m128i scale01, __m128i scale23) {
      int32_t shift = 4-qp/6;

      __m128i r0 = _mm_cvtepi16_epi32(_mm_loadu_si64(r01));
      __m128i r1 = _mm_cvtepi16_epi32(_mm_loadu_si64((uint64_t*)r01 + 1));
      __m128i r2 = _mm_cvtepi16_epi32(_mm_loadu_si64(r23));
      __m128i r3 = _mm_cvtepi16_epi32(_mm_loadu_si64((uint64_t*)r23 + 1));

      __m128i scale0 = _mm_cvtepi16_epi32(_mm_loadu_si64(&scale01));
      __m128i scale1 = _mm_cvtepi16_epi32(_mm_loadu_si64((uint64_t*)&scale01 + 1));
      __m128i scale2 = _mm_cvtepi16_epi32(_mm_loadu_si64(&scale23));
      __m128i scale3 = _mm_cvtepi16_epi32(_mm_loadu_si64((uint64_t*)&scale23 + 1));

      __m128i add_reg = _mm_set1_epi32(1 << (3-qp/6));
      __m128i shift_reg = _mm_loadu_si32(&shift);

      r0 = _mm_mullo_epi32(r0, scale0);
      r1 = _mm_mullo_epi32(r1, scale1);
      r2 = _mm_mullo_epi32(r2, scale2);
      r3 = _mm_mullo_epi32(r3, scale3);

      r0 = _mm_add_epi32(r0, add_reg);
      r1 = _mm_add_epi32(r1, add_reg);
      r2 = _mm_add_epi32(r2, add_reg);
      r3 = _mm_add_epi32(r3, add_reg);

      r0 = _mm_sra_epi32(r0, shift_reg);
      r1 = _mm_sra_epi32(r1, shift_reg);
      r2 = _mm_sra_epi32(r2, shift_reg);
      r3 = _mm_sra_epi32(r3, shift_reg);

      *r01 = _mm_packs_epi32(r0, r1);
      *r23 = _mm_packs_epi32(r2, r3);
}

void transform_luma_4x4_sse(Macroblock *mb, int blkIdx, Undo264Context *ctx) {
      static int16_t c[4][4];

      int stride = mb->p_pic->widthY;

      int blkY = (blkIdx>>2)<<2;
      int blkX = (blkIdx&3)<<2;
      int qp = mb->QPY;

      inverse_4x4_coeff_scaling_scan(mb->residuals.luma_4x4_coeffs[blkIdx], c);



      int scaleIndex = 3 * (IS_INTER(mb->mb_type) > 0);
      int16_t (*scale) [4] = ctx->levelScale4x4[scaleIndex][qp];

      __m128i r01 = _mm_loadu_si128((__m128i*)&c[0][0]);
      __m128i r23 = _mm_loadu_si128((__m128i*)&c[2][0]);
      __m128i scale01 = _mm_loadu_si128((__m128i*)&scale[0][0]);
      __m128i scale23 = _mm_loadu_si128((__m128i*)&scale[2][0]);

      if (qp >= 24) scaling_residual_4x4_lshift_sse(qp, &r01, &r23, scale01, scale23);
      else          scaling_residual_4x4_rshift_min_sse(qp, &r01, &r23, scale01, scale23);

      idct_4x4_sse(
            &r01, &r23,
            &mb->p_pic->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
            stride, ctx->ps->sps->bit_depth_luma);
}


void transform_luma_8x8_sse(Macroblock *mb, int i8x8, Undo264Context *ctx) {
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

      idct_8x8_sse(d, &mb->p_pic->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX], stride, ctx->ps->sps->bit_depth_luma);
}




void transform_luma_16x16_sse(Macroblock *mb, Undo264Context *ctx) {
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
      __m128i scale01 = _mm_loadu_si128((__m128i*)&scale[0][0]);
      __m128i scale23 = _mm_loadu_si128((__m128i*)&scale[2][0]);
      __m128i r01 = _mm_set1_epi16(0);
      __m128i r23 = _mm_set1_epi16(0);

      /* scaling & transform */
      int stride = mb->p_pic->widthY;
      for (int i = 0; i < 16; i++) {
            c[0][0] = dcY[i>>2][i&3];
            if (mb->residuals.cbp_luma != 0) {
                  inverse_4x4_coeff_scaling_scan_dc(mb->residuals.luma_16x16_AC[i], dcY[i>>2][i&3], c);

                  r01 = _mm_loadu_si128((__m128i*)&c[0][0]);
                  r23 = _mm_loadu_si128((__m128i*)&c[2][0]);

                  if (qp >= 24) scaling_residual_4x4_lshift_sse(qp, &r01, &r23, scale01, scale23);
                  else          scaling_residual_4x4_rshift_min_sse(qp, &r01, &r23, scale01, scale23);
            }
            ((int16_t*)&r01)[0] = c[0][0];

            int blkY = (i>>2) << 2;
            int blkX = (i&3) << 2;
            idct_4x4_sse(
                  &r01,&r23,
                  &mb->p_pic->luma[(mb->mb_y*16 + blkY)*stride + mb->mb_x*16 + blkX],
                  stride, ctx->ps->sps->bit_depth_luma);
      }
}


void transform_chroma_sse(Macroblock *mb, Undo264Context *ctx) {

      int nbCr4x4 = (mb->mb_height_c/4) * (mb->mb_width_c/4);

      int cbp = mb->residuals.cbp_chroma;

      for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            int qp = mb->QPC[iCbCr];

            uint8_t *dst = iCbCr
                  ? mb->p_pic->cr
                  : mb->p_pic->cb;

            if (cbp & 3) {
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


                  static int16_t coeffs[4][4];
                  memset(coeffs, 0, sizeof coeffs);

                  int stride = mb->p_pic->widthC;
                  int16_t (*scale) [4] = ctx->levelScale4x4[scaleIndex][qp];

                  __m128i scale01 = _mm_loadu_si128((__m128i*)&scale[0][0]);
                  __m128i scale23 = _mm_loadu_si128((__m128i*)&scale[2][0]);
                  __m128i r01 = _mm_set1_epi16(0);
                  __m128i r23 = _mm_set1_epi16(0);

                  for (int i4x4 = 0; i4x4 < nbCr4x4; i4x4++) {
                        coeffs[0][0] = dcC[i4x4>>1][i4x4&1];
                        if (cbp & 2) {
                              inverse_4x4_coeff_scaling_scan_dc(mb->residuals.chroma_AC[iCbCr][i4x4], dcC[i4x4>>1][i4x4&1], coeffs);

                              r01 = _mm_loadu_si128((__m128i*)&coeffs[0][0]);
                              r23 = _mm_loadu_si128((__m128i*)&coeffs[2][0]);

                              if (qp >= 24) scaling_residual_4x4_lshift_sse(qp, &r01, &r23, scale01, scale23);
                              else          scaling_residual_4x4_rshift_min_sse(qp, &r01, &r23, scale01, scale23);
                        }
                        ((int16_t*)&r01)[0] = coeffs[0][0];

                        int blkY = (i4x4>>1) << 2;
                        int blkX = (i4x4&1) << 2;
                        idct_4x4_sse(
                              &r01, &r23,
                              &dst[(mb->mb_y*8 + blkY)*stride + mb->mb_x*8 + blkX],
                              stride, ctx->ps->sps->bit_depth_chroma);
                  }
            }
      }
}