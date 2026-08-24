//
// Created by gmathix on 8/22/26.
//


#include "immintrin.h"
#include "stdint.h"



// the compiler can't easily vectorize the scalar version of this in inter_template.c, so we have to do it manually



#ifndef HEIGHT
#define HEIGHT 8
#endif


#define WEIGHTED_SSE_FUNC3(name, width, H, ...) name ## _ ## width ## x ## H(__VA_ARGS__)
#define WEIGHTED_SSE_FUNC2(name, width, H, ...) WEIGHTED_SSE_FUNC3(name, width, H, __VA_ARGS__)
#define WEIGHTED_SSE_FUNC(name, width, ...)     WEIGHTED_SSE_FUNC2(name, width, HEIGHT, __VA_ARGS__)



void WEIGHTED_SSE_FUNC(weigh_bi_sse, 2,
                       const uint8_t *restrict temp_bi_buf, uint8_t *restrict dst, int stride,
                       int logWD, int w0, int w1, int o0, int o1) {

    const __m128i logWD_reg = _mm_set1_epi32(1 << logWD);

    const __m128i w01_reg = _mm_set1_epi32((int32_t) ((unsigned)w1 << 16 | ((unsigned)w0 & 0xFFFF)));
    const __m128i o01_reg = _mm_set1_epi32((o0 + o1 + 1) >> 1);
    const __m128i shift_reg = _mm_cvtsi32_si128(logWD + 1);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t0 = _mm_cvtepu8_epi16(_mm_loadu_si16(&temp_bi_buf[y*2]));
        __m128i t1 = _mm_cvtepu8_epi16(_mm_loadu_si16(&temp_bi_buf[HEIGHT*2 + y*2]));

        __m128i low = _mm_unpacklo_epi16(t0, t1); // [t0_0, t1_0, t0_1, t1_1, t0_2, t1_2, t0_3, t1_3]

        __m128i sum_low  = _mm_madd_epi16(low, w01_reg);
        sum_low          = _mm_sra_epi32(_mm_add_epi32(sum_low, logWD_reg), shift_reg);
        sum_low          = _mm_add_epi32(sum_low, o01_reg);
        __m128i packed16 = _mm_packs_epi32(sum_low, sum_low);
        __m128i packed8  = _mm_packus_epi16(packed16, packed16);

        _mm_storeu_si16(dst, packed8);

        dst += stride;
    }
}

void WEIGHTED_SSE_FUNC(weigh_bi_sse, 4,
                       const uint8_t *restrict temp_bi_buf, uint8_t *restrict dst, int stride,
                       int logWD, int w0, int w1, int o0, int o1) {

    const __m128i logWD_reg = _mm_set1_epi32(1 << logWD);

    const __m128i w01_reg = _mm_set1_epi32((int32_t) ((unsigned)w1 << 16 | ((unsigned)w0 & 0xFFFF)));
    const __m128i o01_reg = _mm_set1_epi32((o0 + o1 + 1) >> 1);
    const __m128i shift_reg = _mm_cvtsi32_si128(logWD + 1);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t0 = _mm_cvtepu8_epi16(_mm_loadu_si32(&temp_bi_buf[y*4]));
        __m128i t1 = _mm_cvtepu8_epi16(_mm_loadu_si32(&temp_bi_buf[HEIGHT*4 + y*4]));

        __m128i low = _mm_unpacklo_epi16(t0, t1); // [t0_0, t1_0, t0_1, t1_1, t0_2, t1_2, t0_3, t1_3]

        __m128i sum_low  = _mm_madd_epi16(low, w01_reg);
        sum_low          = _mm_sra_epi32(_mm_add_epi32(sum_low, logWD_reg), shift_reg);
        sum_low          = _mm_add_epi32(sum_low, o01_reg);
        __m128i packed16 = _mm_packs_epi32(sum_low, sum_low);
        __m128i packed8  = _mm_packus_epi16(packed16, packed16);

        _mm_storeu_si32(dst, packed8);

        dst += stride;
    }
}

void WEIGHTED_SSE_FUNC(weigh_bi_sse, 8,
                       const uint8_t *restrict temp_bi_buf, uint8_t *restrict dst, int stride,
                       int logWD, int w0, int w1, int o0, int o1) {

    const __m128i logWD_reg = _mm_set1_epi32(1 << logWD);

    // lanes are ordered little-endian so this will give [w0, w1, w0, w1, w0, w1, w0, w1]
    const __m128i w01_reg   = _mm_set1_epi32((int32_t) ((unsigned)w1 << 16 | ((unsigned)w0 & 0xFFFF)));
    const __m128i o01_reg   = _mm_set1_epi32((o0 + o1 + 1) >> 1);
    const __m128i shift_reg = _mm_cvtsi32_si128(logWD + 1);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&temp_bi_buf[y*8]));
        __m128i t1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&temp_bi_buf[HEIGHT*8 + y*8]));

        __m128i low      = _mm_unpacklo_epi16(t0, t1); // [t0_0, t1_0, t0_1, t1_1, t0_2, t1_2, t0_3, t1_3]
        __m128i high     = _mm_unpackhi_epi16(t0, t1); // [t0_4, t1_4, t0_5, t1_5, t0_6, t1_6, t0_7, t1_7]

        __m128i sum_low  = _mm_madd_epi16(low, w01_reg); // t0*w0 + w1*w1, 32bit x 4
        __m128i sum_high = _mm_madd_epi16(high, w01_reg);

        // add (1 << logWD) and shift right by (logWD + 1)
        sum_low          = _mm_sra_epi32(_mm_add_epi32(sum_low, logWD_reg), shift_reg);
        sum_high         = _mm_sra_epi32(_mm_add_epi32(sum_high, logWD_reg), shift_reg);

        // add (o0 + o1 + 1) >> 1
        sum_low          = _mm_add_epi32(sum_low, o01_reg);
        sum_high         = _mm_add_epi32(sum_high, o01_reg);

        // pack low+high to signed 16-bit
        __m128i packed16 = _mm_packs_epi32(sum_low, sum_high);

        // pack to unsigned 8-bit, clamping to 255 already done, we get 64 bytes
        __m128i packed8  = _mm_packus_epi16(packed16, packed16);

        // only store the low 64 bytes
        _mm_storeu_si64(dst, packed8);

        dst += stride;
    }
}

void WEIGHTED_SSE_FUNC(weigh_bi_sse, 16,
                       const uint8_t *restrict temp_bi_buf, uint8_t *restrict dst, int stride,
                       int logWD, int w0, int w1, int o0, int o1) {

    // no fancy stuff here for now, just the WIDTH=8 version of this ran twice per row

    const __m128i logWD_reg = _mm_set1_epi32(1 << logWD);

    const __m128i w01_reg   = _mm_set1_epi32((int32_t) ((unsigned)w1 << 16 | ((unsigned)w0 & 0xFFFF)));
    const __m128i o01_reg   = _mm_set1_epi32((o0 + o1 + 1) >> 1);
    const __m128i shift_reg = _mm_cvtsi32_si128(logWD + 1);

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < 2; x++) {
            __m128i t0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&temp_bi_buf[y*16 + x*8]));
            __m128i t1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&temp_bi_buf[HEIGHT*16 + y*16 + x*8]));

            __m128i low      = _mm_unpacklo_epi16(t0, t1);
            __m128i high     = _mm_unpackhi_epi16(t0, t1);

            __m128i sum_low  = _mm_madd_epi16(low, w01_reg);
            __m128i sum_high = _mm_madd_epi16(high, w01_reg);

            sum_low          = _mm_sra_epi32(_mm_add_epi32(sum_low, logWD_reg), shift_reg);
            sum_high         = _mm_sra_epi32(_mm_add_epi32(sum_high, logWD_reg), shift_reg);

            sum_low          = _mm_add_epi32(sum_low, o01_reg);
            sum_high         = _mm_add_epi32(sum_high, o01_reg);

            __m128i packed16 = _mm_packs_epi32(sum_low, sum_high);

            __m128i packed8  = _mm_packus_epi16(packed16, packed16);

            _mm_storeu_si64(&dst[x*8], packed8);
        }
        dst += stride;
    }
}



void WEIGHTED_SSE_FUNC(weigh_single_sse, 2,
                       uint8_t *dst, int stride,
                       int logWD, int w, int o) {

    __m128i logWD_reg = _mm_set1_epi16((int16_t) ((logWD > 0) ? (1 << (logWD-1)) : 0));
    __m128i w_reg     = _mm_set1_epi16((int16_t) w);
    __m128i o_reg     = _mm_set1_epi16((int16_t) o);
    __m128i shift_reg = _mm_cvtsi32_si128(logWD);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t = _mm_cvtepu8_epi16(_mm_loadu_si16((__m128i*)dst));

        __m128i res = _mm_add_epi16(_mm_mullo_epi16(t, w_reg), logWD_reg);
        res = _mm_add_epi16(_mm_sra_epi16(res, shift_reg), o_reg);

        _mm_storeu_si16(dst, _mm_packus_epi16(res, res));

        dst += stride;
    }
}

void WEIGHTED_SSE_FUNC(weigh_single_sse, 4,
                       uint8_t *dst, int stride,
                       int logWD, int w, int o) {

    __m128i logWD_reg = _mm_set1_epi16((int16_t) ((logWD > 0) ? (1 << (logWD-1)) : 0));
    __m128i w_reg     = _mm_set1_epi16((int16_t) w);
    __m128i o_reg     = _mm_set1_epi16((int16_t) o);
    __m128i shift_reg = _mm_cvtsi32_si128(logWD);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t = _mm_cvtepu8_epi16(_mm_loadu_si32((__m128i*)dst));

        __m128i res = _mm_add_epi16(_mm_mullo_epi16(t, w_reg), logWD_reg);
        res = _mm_add_epi16(_mm_sra_epi16(res, shift_reg), o_reg);

        _mm_storeu_si32(dst, _mm_packus_epi16(res, res));

        dst += stride;
    }
}

void WEIGHTED_SSE_FUNC(weigh_single_sse, 8,
                       uint8_t *dst, int stride,
                       int logWD, int w, int o) {

    __m128i logWD_reg = _mm_set1_epi16((int16_t) ((logWD > 0) ? (1 << (logWD-1)) : 0));
    __m128i w_reg     = _mm_set1_epi16((int16_t) w);
    __m128i o_reg     = _mm_set1_epi16((int16_t) o);
    __m128i shift_reg = _mm_cvtsi32_si128(logWD);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t = _mm_cvtepu8_epi16(_mm_loadu_si64((__m128i*)dst));

        __m128i res = _mm_add_epi16(_mm_mullo_epi16(t, w_reg), logWD_reg);
        res = _mm_add_epi16(_mm_sra_epi16(res, shift_reg), o_reg);

        _mm_storeu_si64(dst, _mm_packus_epi16(res, res));

        dst += stride;
    }
}

void WEIGHTED_SSE_FUNC(weigh_single_sse, 16,
                       uint8_t *dst, int stride,
                       int logWD, int w, int o) {

    __m128i logWD_reg = _mm_set1_epi16((int16_t) ((logWD > 0) ? (1 << (logWD-1)) : 0));
    __m128i w_reg     = _mm_set1_epi16((int16_t) w);
    __m128i o_reg     = _mm_set1_epi16((int16_t) o);
    __m128i shift_reg = _mm_cvtsi32_si128(logWD);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i t1 = _mm_cvtepu8_epi16(_mm_loadu_si64((__m128i*)&dst[0]));
        __m128i t2 = _mm_cvtepu8_epi16(_mm_loadu_si64((__m128i*)&dst[8]));

        __m128i res1 = _mm_add_epi16(_mm_mullo_epi16(t1, w_reg), logWD_reg);
        __m128i res2 = _mm_add_epi16(_mm_mullo_epi16(t2, w_reg), logWD_reg);

        res1 = _mm_add_epi16(_mm_sra_epi16(res1, shift_reg), o_reg);
        res2 = _mm_add_epi16(_mm_sra_epi16(res2, shift_reg), o_reg);

        _mm_storeu_si64(&dst[0], _mm_packus_epi16(res1, res1));
        _mm_storeu_si64(&dst[8], _mm_packus_epi16(res2, res2));

        dst += stride;
    }
}