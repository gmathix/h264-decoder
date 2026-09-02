//
// Created by gmathix on 8/31/26.
//



// good luck for reading that! i'm planning to write documentation comments in the definitive version of this file


#include <immintrin.h>

#include "deblock.h"
#include "emmintrin.h"
#include "global.h"


// _mm_subs_epu8 saturates to 0 instead of wrapping, so whatever direction is positive is retained
#define ABS_DIFF_epi8(r0, r1) _mm_or_si128(_mm_subs_epu8(r0, r1), _mm_subs_epu8(r1, r0))

// 0xFF where r < thresh, else 0
// #define THRESH_MASK_epu8(r, thresh) _mm_cmpeq_epi8(_mm_subs_epu8(r, _mm_set1_epi8(thresh-1)), _mm_setzero_si128())
#define THRESH_MASK_epu8(r, thresh) _mm_cmplt_epi8(_mm_xor_si128(r, _mm_set1_epi8(0x80)), _mm_xor_si128(thresh, _mm_set1_epi8(0x80)))

// 0x01 where r < thresh, else 0
#define THRESH_MASK_SET1_epi8(r, thresh) _mm_subs_epu8(THRESH_MASK_epu8(r, thresh), _mm_set1_epi8(0xFE))

#define CLIP3_epi16(min, max, r) _mm_max_epi16(min, _mm_min_epi16(max, r))



#define TRANSPOSE8x8(l0, l1, l2, l3) do {                                                                \
    __m128i shuffle = _mm_set_epi8(15,11,7,3, 14,10,6,2, 13,9,5,1, 12,8,4,0);                            \
                                                                                                         \
    __m128i t0 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(l0),_mm_castsi128_ps(l1),0b10001000)); \
    __m128i t1 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(l2),_mm_castsi128_ps(l3),0b10001000)); \
    __m128i t2 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(l0),_mm_castsi128_ps(l1),0b11011101)); \
    __m128i t3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(l2),_mm_castsi128_ps(l3),0b11011101)); \
                                                                                                         \
    l0 = _mm_shuffle_epi8(t0, shuffle);                                                                  \
    l1 = _mm_shuffle_epi8(t1, shuffle);                                                                  \
    l2 = _mm_shuffle_epi8(t2, shuffle);                                                                  \
    l3 = _mm_shuffle_epi8(t3, shuffle);                                                                  \
                                                                                                         \
    t0 = _mm_unpacklo_epi32(l0, l1);                                                                     \
    t1 = _mm_unpackhi_epi32(l0, l1);                                                                     \
    t2 = _mm_unpacklo_epi32(l2, l3);                                                                     \
    t3 = _mm_unpackhi_epi32(l2, l3);                                                                     \
                                                                                                         \
    _mm_storeu_si128(&l0, t0);                                                                           \
    _mm_storeu_si128(&l1, t1);                                                                           \
    _mm_storeu_si128(&l2, t2);                                                                           \
    _mm_storeu_si128(&l3, t3);                                                                           \
} while (0);




typedef struct {
    __m128i c0, c1, c2, c3, c4, c5, c6, c7;
} strided_load_t ;

/** load p0,p1,p2,q0,q1,q2 columns in registers
 *
 *  instead of loading the 16x16 block to then do a 16x16 transpose and get p0,p1,p2,q0,q1,q2 as rows,
 *  we instead load the two neighboring 8x8 blocks and transpose them. this should be faster than a 16x16 transform,
 *  which is probably slow here (64 simd ops with avx512, can't imagine with sse)
 */
static always_inline strided_load_t load_strided_16(uint8_t *src, int stride) {
    __m128i l0 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[0*stride]), _mm_loadu_si64(&src[1*stride]));
    __m128i l1 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[2*stride]), _mm_loadu_si64(&src[3*stride]));
    __m128i l2 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[4*stride]), _mm_loadu_si64(&src[5*stride]));
    __m128i l3 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[6*stride]), _mm_loadu_si64(&src[7*stride]));

    TRANSPOSE8x8(l0, l1, l2, l3)


    __m128i l4 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[ 8*stride]), _mm_loadu_si64(&src[ 9*stride]));
    __m128i l5 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[10*stride]), _mm_loadu_si64(&src[11*stride]));
    __m128i l6 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[12*stride]), _mm_loadu_si64(&src[13*stride]));
    __m128i l7 = _mm_unpacklo_epi64(_mm_loadu_si64(&src[14*stride]), _mm_loadu_si64(&src[15*stride]));

    TRANSPOSE8x8(l4, l5, l6, l7)

    return (strided_load_t) {
        /*p2*/_mm_unpacklo_epi64(l0, l4),
        /*p1*/_mm_unpackhi_epi64(l0, l4),
        /*p0*/_mm_unpacklo_epi64(l1, l5),
        /*q0*/_mm_unpackhi_epi64(l1, l5),
        /*q1*/_mm_unpacklo_epi64(l2, l6),
        /*q2*/_mm_unpackhi_epi64(l2, l6),
        /*l3*/_mm_unpacklo_epi64(l3, l7),
        /*q3*/_mm_unpackhi_epi64(l3, l7),
    };
}

static always_inline void store_strided_16(uint8_t *dst, int stride, strided_load_t rows) {
    __m128i l0 = _mm_unpacklo_epi64(rows.c0, rows.c1);
    __m128i l1 = _mm_unpacklo_epi64(rows.c2, rows.c3);
    __m128i l2 = _mm_unpacklo_epi64(rows.c4, rows.c5);
    __m128i l3 = _mm_unpacklo_epi64(rows.c6, rows.c7);

    TRANSPOSE8x8(l0, l1, l2, l3)
    _mm_storeu_si64(&dst[0*stride], _mm_unpacklo_epi64(l0, l0));
    _mm_storeu_si64(&dst[1*stride], _mm_unpackhi_epi64(l0, l0));
    _mm_storeu_si64(&dst[2*stride], _mm_unpacklo_epi64(l1, l1));
    _mm_storeu_si64(&dst[3*stride], _mm_unpackhi_epi64(l1, l1));
    _mm_storeu_si64(&dst[4*stride], _mm_unpacklo_epi64(l2, l2));
    _mm_storeu_si64(&dst[5*stride], _mm_unpackhi_epi64(l2, l2));
    _mm_storeu_si64(&dst[6*stride], _mm_unpacklo_epi64(l3, l3));
    _mm_storeu_si64(&dst[7*stride], _mm_unpackhi_epi64(l3, l3));

    l0 = _mm_unpackhi_epi64(rows.c0, rows.c1);
    l1 = _mm_unpackhi_epi64(rows.c2, rows.c3);
    l2 = _mm_unpackhi_epi64(rows.c4, rows.c5);
    l3 = _mm_unpackhi_epi64(rows.c6, rows.c7);

    TRANSPOSE8x8(l0, l1, l2, l3);
    _mm_storeu_si64(&dst[ 8*stride], _mm_unpacklo_epi64(l0, l0));
    _mm_storeu_si64(&dst[ 9*stride], _mm_unpackhi_epi64(l0, l0));
    _mm_storeu_si64(&dst[10*stride], _mm_unpacklo_epi64(l1, l1));
    _mm_storeu_si64(&dst[11*stride], _mm_unpackhi_epi64(l1, l1));
    _mm_storeu_si64(&dst[12*stride], _mm_unpacklo_epi64(l2, l2));
    _mm_storeu_si64(&dst[13*stride], _mm_unpackhi_epi64(l2, l2));
    _mm_storeu_si64(&dst[14*stride], _mm_unpacklo_epi64(l3, l3));
    _mm_storeu_si64(&dst[15*stride], _mm_unpackhi_epi64(l3, l3));
}





#define HELPER_DEBLOCK_8_PIX_WEAK_LUMA(p0, p1, p2, q0, q1, q2, aP, aQ, threshold, threshold_neg, tc0, tc0_neg) \
    delta = _mm_mullo_epi16(_mm_sub_epi16(q0, p0), _mm_set1_epi16(1 << 2));                                    \
    delta = _mm_add_epi16(delta, _mm_sub_epi16(p1, q1));                                                       \
    delta = _mm_add_epi16(delta, _mm_set1_epi16(4));                                                           \
    delta = _mm_srai_epi16(delta, 3);                                                                          \
    delta = CLIP3_epi16(threshold_neg, threshold, delta);                                                      \
                                                                                                               \
    add_common = _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(p0, q0), _mm_set1_epi16(1)), 1);                   \
                                                                                                               \
    p1_add = _mm_sub_epi16(add_common, _mm_slli_epi16(p1, 1));                                                 \
    p1_add = _mm_srai_epi16(_mm_add_epi16(p2, p1_add), 1);                                                     \
    p1_add = CLIP3_epi16(tc0_neg, tc0, p1_add);                                                                \
                                                                                                               \
    p1 = _mm_blendv_epi8(p1, _mm_add_epi16(p1, p1_add), _mm_cmplt_epi16(aP, beta_reg));                        \
                                                                                                               \
    q1_add = _mm_sub_epi16(add_common, _mm_slli_epi16(q1, 1));                                                 \
    q1_add = _mm_srai_epi16(_mm_add_epi16(q2, q1_add), 1);                                                     \
    q1_add = CLIP3_epi16(tc0_neg, tc0, q1_add);                                                                \
    q1 = _mm_blendv_epi8(q1, _mm_add_epi16(q1, q1_add), _mm_cmplt_epi16(aQ, beta_reg));                        \
                                                                                                               \
    p0 = CLIP3_epi16(_mm_setzero_si128(), _mm_set1_epi16(255), _mm_add_epi16(p0, delta));                      \
    q0 = CLIP3_epi16(_mm_setzero_si128(), _mm_set1_epi16(255), _mm_sub_epi16(q0, delta));



void deblock_edge_weak_luma_h_sse4(uint8_t *dst, int stride, int alpha, int beta, int indexA, int *bS) {
    __m128i zero_reg = _mm_setzero_si128();

    __m128i p0 = _mm_loadu_si128((__m128i*)&dst[-1*stride]);
    __m128i p1 = _mm_loadu_si128((__m128i*)&dst[-2*stride]);
    __m128i p2 = _mm_loadu_si128((__m128i*)&dst[-3*stride]);
    __m128i q0 = _mm_loadu_si128((__m128i*)&dst[ 0*stride]);
    __m128i q1 = _mm_loadu_si128((__m128i*)&dst[ 1*stride]);
    __m128i q2 = _mm_loadu_si128((__m128i*)&dst[ 2*stride]);

    __m128i tc0 = _mm_load_si128((__m128i*)get_tc0_table(bS, indexA));
    __m128i skip = _mm_cmpgt_epi8(tc0, _mm_set1_epi8(-1));

    __m128i alpha_reg = _mm_set1_epi8(alpha);
    __m128i beta_reg  = _mm_set1_epi8(beta);

    __m128i p0q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), alpha_reg);
    __m128i p1p0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p1, p0), beta_reg);
    __m128i q1q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(q1, q0), beta_reg);
    __m128i filter_cond = _mm_and_si128(
        _mm_and_si128(
            _mm_and_si128(
                p0q0_mask,
                p1p0_mask),
            q1q0_mask),
        skip
    );

    __m128i aP = ABS_DIFF_epi8(p2, p0);
    __m128i aQ = ABS_DIFF_epi8(q2, q0);

    __m128i threshold = _mm_add_epi8(tc0, _mm_add_epi8(THRESH_MASK_SET1_epi8(aP, beta_reg), THRESH_MASK_SET1_epi8(aQ, beta_reg)));



    // from now we'll need 16-bit precision, so we do 2 passes

    // we don't need beta packed in 8 bits anymore so we can extend to 16 for these operations
    beta_reg  = _mm_cvtepu8_epi16(beta_reg);

    __m128i add_common, p1_add, q1_add, delta;


    // lower half
    __m128i p0_lo = _mm_unpacklo_epi8(p0, zero_reg);
    __m128i p1_lo = _mm_unpacklo_epi8(p1, zero_reg);
    __m128i p2_lo = _mm_unpacklo_epi8(p2, zero_reg);
    __m128i q0_lo = _mm_unpacklo_epi8(q0, zero_reg);
    __m128i q1_lo = _mm_unpacklo_epi8(q1, zero_reg);
    __m128i q2_lo = _mm_unpacklo_epi8(q2, zero_reg);

    __m128i aP_lo = _mm_unpacklo_epi8(aP, zero_reg);
    __m128i aQ_lo = _mm_unpacklo_epi8(aQ, zero_reg);

    __m128i threshold_lo     = _mm_unpacklo_epi8(threshold, zero_reg);
    __m128i threshold_lo_neg = _mm_sub_epi16(zero_reg, threshold_lo);
    __m128i tc0_lo           = _mm_unpacklo_epi8(tc0, zero_reg);
    __m128i tc0_lo_neg       = _mm_sub_epi16(zero_reg, tc0_lo);

    HELPER_DEBLOCK_8_PIX_WEAK_LUMA(p0_lo, p1_lo, p2_lo, q0_lo, q1_lo, q2_lo, aP_lo, aQ_lo,
        threshold_lo, threshold_lo_neg, tc0_lo, tc0_lo_neg);



    // upper half
    __m128i p0_hi = _mm_unpackhi_epi8(p0, zero_reg);
    __m128i p1_hi = _mm_unpackhi_epi8(p1, zero_reg);
    __m128i p2_hi = _mm_unpackhi_epi8(p2, zero_reg);
    __m128i q0_hi = _mm_unpackhi_epi8(q0, zero_reg);
    __m128i q1_hi = _mm_unpackhi_epi8(q1, zero_reg);
    __m128i q2_hi = _mm_unpackhi_epi8(q2, zero_reg);

    __m128i aP_hi = _mm_unpackhi_epi8(aP, zero_reg);
    __m128i aQ_hi = _mm_unpackhi_epi8(aQ, zero_reg);

    __m128i threshold_hi     = _mm_unpackhi_epi8(threshold, zero_reg);
    __m128i threshold_hi_neg = _mm_sub_epi16(zero_reg, threshold_hi);
    __m128i tc0_hi           = _mm_unpackhi_epi8(tc0, zero_reg);
    __m128i tc0_hi_neg       = _mm_sub_epi16(zero_reg, tc0_hi);

    HELPER_DEBLOCK_8_PIX_WEAK_LUMA(p0_hi, p1_hi, p2_hi, q0_hi, q1_hi, q2_hi, aP_hi, aQ_hi,
        threshold_hi, threshold_hi_neg, tc0_hi, tc0_hi_neg);



    p0 = _mm_blendv_epi8(p0, _mm_packus_epi16(p0_lo, p0_hi), filter_cond);
    p1 = _mm_blendv_epi8(p1, _mm_packus_epi16(p1_lo, p1_hi), filter_cond);
    q0 = _mm_blendv_epi8(q0, _mm_packus_epi16(q0_lo, q0_hi), filter_cond);
    q1 = _mm_blendv_epi8(q1, _mm_packus_epi16(q1_lo, q1_hi), filter_cond);


    _mm_storeu_si128((__m128i*)&dst[-1*stride], p0);
    _mm_storeu_si128((__m128i*)&dst[-2*stride], p1);
    _mm_storeu_si128((__m128i*)&dst[ 0*stride], q0);
    _mm_storeu_si128((__m128i*)&dst[ 1*stride], q1);
}


void deblock_edge_weak_luma_v_sse4(uint8_t *dst, int stride, int alpha, int beta, int indexA, int *bS) {
    __m128i zero_reg = _mm_setzero_si128();


    __m128i p0, p1, p2, q0, q1, q2;
    strided_load_t load = load_strided_16(&dst[-3], stride);
    p2 = load.c0; p1 = load.c1; p0 = load.c2;
    q0 = load.c3; q1 = load.c4; q2 = load.c5;

    __m128i tc0 = _mm_load_si128((__m128i*)get_tc0_table(bS, indexA));
    __m128i skip = _mm_cmpgt_epi8(tc0, _mm_set1_epi8(-1));

    __m128i alpha_reg = _mm_set1_epi8(alpha);
    __m128i beta_reg  = _mm_set1_epi8(beta);

    __m128i p0q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), alpha_reg);
    __m128i p1p0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p1, p0), beta_reg);
    __m128i q1q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(q1, q0), beta_reg);
    __m128i filter_cond = _mm_and_si128(
        _mm_and_si128(
            _mm_and_si128(
                p0q0_mask,
                p1p0_mask),
            q1q0_mask),
        skip
    );

    __m128i aP = ABS_DIFF_epi8(p2, p0);
    __m128i aQ = ABS_DIFF_epi8(q2, q0);

    __m128i threshold = _mm_add_epi8(tc0, _mm_add_epi8(THRESH_MASK_SET1_epi8(aP, beta_reg), THRESH_MASK_SET1_epi8(aQ, beta_reg)));



    // from now we'll need 16-bit precision, so we do 2 passes

    // we don't need beta packed in 8 bits anymore so we can extend to 16 for these operations
    beta_reg  = _mm_cvtepu8_epi16(beta_reg);

    __m128i add_common, p1_add, q1_add, delta;


    // lower half
    __m128i p0_lo = _mm_unpacklo_epi8(p0, zero_reg);
    __m128i p1_lo = _mm_unpacklo_epi8(p1, zero_reg);
    __m128i p2_lo = _mm_unpacklo_epi8(p2, zero_reg);
    __m128i q0_lo = _mm_unpacklo_epi8(q0, zero_reg);
    __m128i q1_lo = _mm_unpacklo_epi8(q1, zero_reg);
    __m128i q2_lo = _mm_unpacklo_epi8(q2, zero_reg);

    __m128i aP_lo = _mm_unpacklo_epi8(aP, zero_reg);
    __m128i aQ_lo = _mm_unpacklo_epi8(aQ, zero_reg);

    __m128i threshold_lo     = _mm_unpacklo_epi8(threshold, zero_reg);
    __m128i threshold_lo_neg = _mm_sub_epi16(zero_reg, threshold_lo);
    __m128i tc0_lo           = _mm_unpacklo_epi8(tc0, zero_reg);
    __m128i tc0_lo_neg       = _mm_sub_epi16(zero_reg, tc0_lo);

    HELPER_DEBLOCK_8_PIX_WEAK_LUMA(p0_lo, p1_lo, p2_lo, q0_lo, q1_lo, q2_lo, aP_lo, aQ_lo,
        threshold_lo, threshold_lo_neg, tc0_lo, tc0_lo_neg);



    // upper half
    __m128i p0_hi = _mm_unpackhi_epi8(p0, zero_reg);
    __m128i p1_hi = _mm_unpackhi_epi8(p1, zero_reg);
    __m128i p2_hi = _mm_unpackhi_epi8(p2, zero_reg);
    __m128i q0_hi = _mm_unpackhi_epi8(q0, zero_reg);
    __m128i q1_hi = _mm_unpackhi_epi8(q1, zero_reg);
    __m128i q2_hi = _mm_unpackhi_epi8(q2, zero_reg);

    __m128i aP_hi = _mm_unpackhi_epi8(aP, zero_reg);
    __m128i aQ_hi = _mm_unpackhi_epi8(aQ, zero_reg);

    __m128i threshold_hi     = _mm_unpackhi_epi8(threshold, zero_reg);
    __m128i threshold_hi_neg = _mm_sub_epi16(zero_reg, threshold_hi);
    __m128i tc0_hi           = _mm_unpackhi_epi8(tc0, zero_reg);
    __m128i tc0_hi_neg       = _mm_sub_epi16(zero_reg, tc0_hi);

    HELPER_DEBLOCK_8_PIX_WEAK_LUMA(p0_hi, p1_hi, p2_hi, q0_hi, q1_hi, q2_hi, aP_hi, aQ_hi,
        threshold_hi, threshold_hi_neg, tc0_hi, tc0_hi_neg);



    p0 = _mm_blendv_epi8(p0, _mm_packus_epi16(p0_lo, p0_hi), filter_cond);
    p1 = _mm_blendv_epi8(p1, _mm_packus_epi16(p1_lo, p1_hi), filter_cond);
    q0 = _mm_blendv_epi8(q0, _mm_packus_epi16(q0_lo, q0_hi), filter_cond);
    q1 = _mm_blendv_epi8(q1, _mm_packus_epi16(q1_lo, q1_hi), filter_cond);

    strided_load_t store = (strided_load_t) {p2, p1, p0, q0, q1, q2, load.c6, load.c7};
    store_strided_16(&dst[-3], stride, store);
}





#define add16(a, b) _mm_add_epi16(a, b)
#define srai16(a, b) _mm_srai_epi16(a, b)
#define HELPER_DEBLOCK_8_PIX_STRONG_LUMA(p0, p1, p2, p3, q0, q1, q2, q3, alpha_p_mask, alpha_q_mask) \
    p0m2 = _mm_mullo_epi16(p0, reg_2);                                                               \
    p1m2 = _mm_mullo_epi16(p1, reg_2);                                                               \
    p2m3 = _mm_mullo_epi16(p2, reg_3);                                                               \
    p3m2 = _mm_mullo_epi16(p3, reg_2);                                                               \
    q0m2 = _mm_mullo_epi16(q0, reg_2);                                                               \
    q1m2 = _mm_mullo_epi16(q1, reg_2);                                                               \
    q2m3 = _mm_mullo_epi16(q2, reg_3);                                                               \
    q3m2 = _mm_mullo_epi16(q3, reg_2);                                                               \
                                                                                                     \
    p0_res = _mm_blendv_epi8(                                                                        \
        srai16(add16(p1m2, add16(p0, add16(q1, reg_2))), 2),                                         \
        srai16(add16(p2, add16(p1m2, add16(p0m2, add16(q0m2, add16(q1, reg_4))))), 3),               \
        alpha_p_mask                                                                                 \
    );                                                                                               \
    p1_res = _mm_blendv_epi8(                                                                        \
        p1,                                                                                          \
        srai16(add16(p2, add16(p1, add16(p0, add16(q0, reg_2)))), 2),                                \
        alpha_p_mask                                                                                 \
    );                                                                                               \
    p2_res = _mm_blendv_epi8(                                                                        \
        p2,                                                                                          \
        srai16(add16(p3m2, add16(p2m3, add16(p1, add16(p0, add16(q0, reg_4))))), 3),                 \
        alpha_p_mask                                                                                 \
    );                                                                                               \
                                                                                                     \
    q0_res = _mm_blendv_epi8(                                                                        \
        srai16(add16(q1m2, add16(q0, add16(p1, reg_2))), 2),                                         \
        srai16(add16(p1, add16(p0m2, add16(q0m2, add16(q1m2, add16(q2, reg_4))))), 3),               \
        alpha_q_mask                                                                                 \
    );                                                                                               \
    q1_res = _mm_blendv_epi8(                                                                        \
        q1,                                                                                          \
        srai16(add16(p0, add16(q0, add16(q1, add16(q2, reg_2)))), 2),                                \
        alpha_q_mask                                                                                 \
    );                                                                                               \
    q2_res = _mm_blendv_epi8(                                                                        \
        q2,                                                                                          \
        srai16(add16(q3m2, add16(q2m3, add16(q1, add16(q0, add16(p0, reg_4))))), 3),                 \
        alpha_q_mask                                                                                 \
    );                                                                                               \
                                                                                                     \
    _mm_storeu_si128(&p0, p0_res);                                                                   \
    _mm_storeu_si128(&p1, p1_res);                                                                   \
    _mm_storeu_si128(&p2, p2_res);                                                                   \
    _mm_storeu_si128(&q0, q0_res);                                                                   \
    _mm_storeu_si128(&q1, q1_res);                                                                   \
    _mm_storeu_si128(&q2, q2_res);

void deblock_edge_strong_luma_h_sse4(uint8_t *dst, int stride, int alpha, int beta) {
    __m128i zero_reg = _mm_setzero_si128();

    __m128i p0 = _mm_loadu_si128((__m128i*)&dst[-1*stride]);
    __m128i p1 = _mm_loadu_si128((__m128i*)&dst[-2*stride]);
    __m128i p2 = _mm_loadu_si128((__m128i*)&dst[-3*stride]);
    __m128i p3 = _mm_loadu_si128((__m128i*)&dst[-4*stride]);
    __m128i q0 = _mm_loadu_si128((__m128i*)&dst[ 0*stride]);
    __m128i q1 = _mm_loadu_si128((__m128i*)&dst[ 1*stride]);
    __m128i q2 = _mm_loadu_si128((__m128i*)&dst[ 2*stride]);
    __m128i q3 = _mm_loadu_si128((__m128i*)&dst[ 3*stride]);


    __m128i alpha_reg = _mm_set1_epi8(alpha);
    __m128i beta_reg  = _mm_set1_epi8(beta);

    __m128i p0q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), alpha_reg);
    __m128i p1p0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p1, p0), beta_reg);
    __m128i q1q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(q1, q0), beta_reg);
    __m128i filter_cond = _mm_and_si128(_mm_and_si128(p0q0_mask,p1p0_mask),q1q0_mask);


    __m128i aP = ABS_DIFF_epi8(p2, p0);
    __m128i aQ = ABS_DIFF_epi8(q2, q0);

    // 0xFF extends to 0xFFFF
    __m128i alpha_p_mask = _mm_and_si128(
        THRESH_MASK_epu8(aP, beta_reg),
        THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), _mm_set1_epi8((alpha >> 2) + 2)));
    __m128i alpha_q_mask = _mm_and_si128(
        THRESH_MASK_epu8(aQ, beta_reg),
        THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), _mm_set1_epi8((alpha >> 2) + 2)));



    __m128i p0m2, p1m2, p2m3, p3m2;
    __m128i q0m2, q1m2, q2m3, q3m2;

    __m128i p0_res, p1_res, p2_res;
    __m128i q0_res, q1_res, q2_res;

    __m128i reg_2 = _mm_set1_epi16(2);
    __m128i reg_3 = _mm_set1_epi16(3);
    __m128i reg_4 = _mm_set1_epi16(4);


    // lower half
    __m128i p0_lo = _mm_unpacklo_epi8(p0, zero_reg);
    __m128i p1_lo = _mm_unpacklo_epi8(p1, zero_reg);
    __m128i p2_lo = _mm_unpacklo_epi8(p2, zero_reg);
    __m128i p3_lo = _mm_unpacklo_epi8(p3, zero_reg);
    __m128i q0_lo = _mm_unpacklo_epi8(q0, zero_reg);
    __m128i q1_lo = _mm_unpacklo_epi8(q1, zero_reg);
    __m128i q2_lo = _mm_unpacklo_epi8(q2, zero_reg);
    __m128i q3_lo = _mm_unpacklo_epi8(q3, zero_reg);

    __m128i alpha_p_mask_lo = _mm_unpacklo_epi8(alpha_p_mask, zero_reg);
    __m128i alpha_q_mask_lo = _mm_unpacklo_epi8(alpha_q_mask, zero_reg);

    HELPER_DEBLOCK_8_PIX_STRONG_LUMA(p0_lo, p1_lo, p2_lo, p3_lo, q0_lo, q1_lo, q2_lo, q3_lo, alpha_p_mask_lo, alpha_q_mask_lo)



    // upper half
    __m128i p0_hi = _mm_unpackhi_epi8(p0, zero_reg);
    __m128i p1_hi = _mm_unpackhi_epi8(p1, zero_reg);
    __m128i p2_hi = _mm_unpackhi_epi8(p2, zero_reg);
    __m128i p3_hi = _mm_unpackhi_epi8(p3, zero_reg);
    __m128i q0_hi = _mm_unpackhi_epi8(q0, zero_reg);
    __m128i q1_hi = _mm_unpackhi_epi8(q1, zero_reg);
    __m128i q2_hi = _mm_unpackhi_epi8(q2, zero_reg);
    __m128i q3_hi = _mm_unpackhi_epi8(q3, zero_reg);

    __m128i alpha_p_mask_hi = _mm_unpackhi_epi8(alpha_p_mask, zero_reg);
    __m128i alpha_q_mask_hi = _mm_unpackhi_epi8(alpha_q_mask, zero_reg);

    HELPER_DEBLOCK_8_PIX_STRONG_LUMA(p0_hi, p1_hi, p2_hi, p3_hi, q0_hi, q1_hi, q2_hi, q3_hi, alpha_p_mask_hi, alpha_q_mask_hi)




    p0 = _mm_blendv_epi8(p0, _mm_packus_epi16(p0_lo, p0_hi), filter_cond);
    p1 = _mm_blendv_epi8(p1, _mm_packus_epi16(p1_lo, p1_hi), filter_cond);
    p2 = _mm_blendv_epi8(p2, _mm_packus_epi16(p2_lo, p2_hi), filter_cond);
    q0 = _mm_blendv_epi8(q0, _mm_packus_epi16(q0_lo, q0_hi), filter_cond);
    q1 = _mm_blendv_epi8(q1, _mm_packus_epi16(q1_lo, q1_hi), filter_cond);
    q2 = _mm_blendv_epi8(q2, _mm_packus_epi16(q2_lo, q2_hi), filter_cond);


    _mm_storeu_si128((__m128i*)&dst[-1*stride], p0);
    _mm_storeu_si128((__m128i*)&dst[-2*stride], p1);
    _mm_storeu_si128((__m128i*)&dst[-3*stride], p2);
    _mm_storeu_si128((__m128i*)&dst[ 0*stride], q0);
    _mm_storeu_si128((__m128i*)&dst[ 1*stride], q1);
    _mm_storeu_si128((__m128i*)&dst[ 2*stride], q2);
}


void deblock_edge_strong_luma_v_sse4(uint8_t *dst, int stride, int alpha, int beta) {
    __m128i zero_reg = _mm_setzero_si128();

    __m128i p0, p1, p2, p3, q0, q1, q2, q3;
    strided_load_t load = load_strided_16(&dst[-4], stride);
    p3 = load.c0; p2 = load.c1; p1 = load.c2; p0 = load.c3;
    q0 = load.c4; q1 = load.c5; q2 = load.c6; q3 = load.c7;


    __m128i alpha_reg = _mm_set1_epi8(alpha);
    __m128i beta_reg  = _mm_set1_epi8(beta);

    __m128i p0q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), alpha_reg);
    __m128i p1p0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(p1, p0), beta_reg);
    __m128i q1q0_mask = THRESH_MASK_epu8(ABS_DIFF_epi8(q1, q0), beta_reg);
    __m128i filter_cond = _mm_and_si128(_mm_and_si128(p0q0_mask,p1p0_mask),q1q0_mask);


    __m128i aP = ABS_DIFF_epi8(p2, p0);
    __m128i aQ = ABS_DIFF_epi8(q2, q0);

    // 0xFF extends to 0xFFFF
    __m128i alpha_p_mask = _mm_and_si128(
        THRESH_MASK_epu8(aP, beta_reg),
        THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), _mm_set1_epi8((alpha >> 2) + 2)));
    __m128i alpha_q_mask = _mm_and_si128(
        THRESH_MASK_epu8(aQ, beta_reg),
        THRESH_MASK_epu8(ABS_DIFF_epi8(p0, q0), _mm_set1_epi8((alpha >> 2) + 2)));



    __m128i p0m2, p1m2, p2m3, p3m2;
    __m128i q0m2, q1m2, q2m3, q3m2;

    __m128i p0_res, p1_res, p2_res;
    __m128i q0_res, q1_res, q2_res;

    __m128i reg_2 = _mm_set1_epi16(2);
    __m128i reg_3 = _mm_set1_epi16(3);
    __m128i reg_4 = _mm_set1_epi16(4);


    // lower half
    __m128i p0_lo = _mm_unpacklo_epi8(p0, zero_reg);
    __m128i p1_lo = _mm_unpacklo_epi8(p1, zero_reg);
    __m128i p2_lo = _mm_unpacklo_epi8(p2, zero_reg);
    __m128i p3_lo = _mm_unpacklo_epi8(p3, zero_reg);
    __m128i q0_lo = _mm_unpacklo_epi8(q0, zero_reg);
    __m128i q1_lo = _mm_unpacklo_epi8(q1, zero_reg);
    __m128i q2_lo = _mm_unpacklo_epi8(q2, zero_reg);
    __m128i q3_lo = _mm_unpacklo_epi8(q3, zero_reg);

    __m128i alpha_p_mask_lo = _mm_unpacklo_epi8(alpha_p_mask, zero_reg);
    __m128i alpha_q_mask_lo = _mm_unpacklo_epi8(alpha_q_mask, zero_reg);

    HELPER_DEBLOCK_8_PIX_STRONG_LUMA(p0_lo, p1_lo, p2_lo, p3_lo, q0_lo, q1_lo, q2_lo, q3_lo, alpha_p_mask_lo, alpha_q_mask_lo)



    // upper half
    __m128i p0_hi = _mm_unpackhi_epi8(p0, zero_reg);
    __m128i p1_hi = _mm_unpackhi_epi8(p1, zero_reg);
    __m128i p2_hi = _mm_unpackhi_epi8(p2, zero_reg);
    __m128i p3_hi = _mm_unpackhi_epi8(p3, zero_reg);
    __m128i q0_hi = _mm_unpackhi_epi8(q0, zero_reg);
    __m128i q1_hi = _mm_unpackhi_epi8(q1, zero_reg);
    __m128i q2_hi = _mm_unpackhi_epi8(q2, zero_reg);
    __m128i q3_hi = _mm_unpackhi_epi8(q3, zero_reg);

    __m128i alpha_p_mask_hi = _mm_unpackhi_epi8(alpha_p_mask, zero_reg);
    __m128i alpha_q_mask_hi = _mm_unpackhi_epi8(alpha_q_mask, zero_reg);

    HELPER_DEBLOCK_8_PIX_STRONG_LUMA(p0_hi, p1_hi, p2_hi, p3_hi, q0_hi, q1_hi, q2_hi, q3_hi, alpha_p_mask_hi, alpha_q_mask_hi)



    p0 = _mm_blendv_epi8(p0, _mm_packus_epi16(p0_lo, p0_hi), filter_cond);
    p1 = _mm_blendv_epi8(p1, _mm_packus_epi16(p1_lo, p1_hi), filter_cond);
    p2 = _mm_blendv_epi8(p2, _mm_packus_epi16(p2_lo, p2_hi), filter_cond);
    q0 = _mm_blendv_epi8(q0, _mm_packus_epi16(q0_lo, q0_hi), filter_cond);
    q1 = _mm_blendv_epi8(q1, _mm_packus_epi16(q1_lo, q1_hi), filter_cond);
    q2 = _mm_blendv_epi8(q2, _mm_packus_epi16(q2_lo, q2_hi), filter_cond);


    strided_load_t store = (strided_load_t) {p3, p2, p1, p0, q0, q1, q2, q3};
    store_strided_16(&dst[-4], stride, store);
}

