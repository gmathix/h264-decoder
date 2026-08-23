//
// Created by gmathix on 8/23/26.
//


#include "immintrin.h"
#include "stdint.h"



#ifndef HEIGHT
#define HEIGHT 4
#endif


#define CHROMA_SSE_FUNC3(name, width, H, ...) name ## _ ## width ## x ## H(__VA_ARGS__)
#define CHROMA_SSE_FUNC2(name, width, H, ...) CHROMA_SSE_FUNC3(name, width, H, __VA_ARGS__)
#define CHROMA_SSE_FUNC(name, width, ...) CHROMA_SSE_FUNC2(name, width, HEIGHT, __VA_ARGS__)




void CHROMA_SSE_FUNC(chroma_interpolation_sse, 2,
                     const uint8_t *restrict scratch_buf, uint8_t *restrict dst, int stride,
                     int xFrac, int yFrac) {

    __m128i mul_A = _mm_set1_epi16((8-xFrac) * (8-yFrac));
    __m128i mul_B = _mm_set1_epi16(   xFrac  * (8-yFrac));
    __m128i mul_C = _mm_set1_epi16((8-xFrac) *    yFrac );
    __m128i mul_D = _mm_set1_epi16(   xFrac  *    yFrac );

    __m128i add32   = _mm_set1_epi16(32);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i A = _mm_cvtepu8_epi16(_mm_loadu_si16(&scratch_buf[(y+2)*7 + 2]));
        __m128i B = _mm_cvtepu8_epi16(_mm_loadu_si16(&scratch_buf[(y+2)*7 + 3]));
        __m128i C = _mm_cvtepu8_epi16(_mm_loadu_si16(&scratch_buf[(y+3)*7 + 2]));
        __m128i D = _mm_cvtepu8_epi16(_mm_loadu_si16(&scratch_buf[(y+3)*7 + 3]));

        // the maximum temporary value held by the total sum of mul_A*A + mul_B*B + mul_C*C + mul*D*D is 16320, holds in 16-bit
        // (equation 8-270 from the spec)
        // so with _mm_mullo_epi16 we just keep the low 16 bits of the temporary 32-bit product, which is what we want
        __m128i sum = _mm_adds_epu16(
            _mm_adds_epu16(_mm_mullo_epi16(A, mul_A), _mm_mullo_epi16(B, mul_B)),
            _mm_adds_epu16(_mm_mullo_epi16(C, mul_C), _mm_mullo_epi16(D, mul_D))
        );

        sum = _mm_srai_epi16(_mm_adds_epi16(sum, add32), 6);

        _mm_storeu_si16(dst, _mm_packus_epi16(sum, sum));

        dst += stride;
    }
}

void CHROMA_SSE_FUNC(chroma_interpolation_sse, 4,
                     const uint8_t *restrict scratch_buf, uint8_t *restrict dst, int stride,
                     int xFrac, int yFrac) {

    __m128i mul_A = _mm_set1_epi16((8-xFrac) * (8-yFrac));
    __m128i mul_B = _mm_set1_epi16(   xFrac  * (8-yFrac));
    __m128i mul_C = _mm_set1_epi16((8-xFrac) *    yFrac );
    __m128i mul_D = _mm_set1_epi16(   xFrac  *    yFrac );

    __m128i add32   = _mm_set1_epi16(32);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i A = _mm_cvtepu8_epi16(_mm_loadu_si32(&scratch_buf[(y+2)*9 + 2]));
        __m128i B = _mm_cvtepu8_epi16(_mm_loadu_si32(&scratch_buf[(y+2)*9 + 3]));
        __m128i C = _mm_cvtepu8_epi16(_mm_loadu_si32(&scratch_buf[(y+3)*9 + 2]));
        __m128i D = _mm_cvtepu8_epi16(_mm_loadu_si32(&scratch_buf[(y+3)*9 + 3]));

        __m128i sum = _mm_adds_epu16(
            _mm_adds_epu16(_mm_mullo_epi16(A, mul_A), _mm_mullo_epi16(B, mul_B)),
            _mm_adds_epu16(_mm_mullo_epi16(C, mul_C), _mm_mullo_epi16(D, mul_D))
        );

        sum = _mm_srai_epi16(_mm_adds_epi16(sum, add32), 6);

        _mm_storeu_si32(dst, _mm_packus_epi16(sum, sum));

        dst += stride;
    }
}

void CHROMA_SSE_FUNC(chroma_interpolation_sse, 8,
                     const uint8_t *restrict scratch_buf, uint8_t *restrict dst, int stride,
                     int xFrac, int yFrac) {

    __m128i mul_A = _mm_set1_epi16((8-xFrac) * (8-yFrac));
    __m128i mul_B = _mm_set1_epi16(   xFrac  * (8-yFrac));
    __m128i mul_C = _mm_set1_epi16((8-xFrac) *    yFrac );
    __m128i mul_D = _mm_set1_epi16(   xFrac  *    yFrac );

    __m128i add32   = _mm_set1_epi16(32);

    for (int y = 0; y < HEIGHT; y++) {
        __m128i A = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&scratch_buf[(y+2)*13 + 2]));
        __m128i B = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&scratch_buf[(y+2)*13 + 3]));
        __m128i C = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&scratch_buf[(y+3)*13 + 2]));
        __m128i D = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)&scratch_buf[(y+3)*13 + 3]));

        __m128i sum = _mm_adds_epu16(
            _mm_adds_epu16(_mm_mullo_epi16(A, mul_A), _mm_mullo_epi16(B, mul_B)),
            _mm_adds_epu16(_mm_mullo_epi16(C, mul_C), _mm_mullo_epi16(D, mul_D))
        );

        sum = _mm_srai_epi16(_mm_adds_epi16(sum, add32), 6);

        _mm_storeu_si64((__m128i*)dst, _mm_packus_epi16(sum, sum));

        dst += stride;
    }
}




