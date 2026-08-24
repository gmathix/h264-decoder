//
// Created by gmathix on 8/21/26.
//


/*
 *  this file is compiled 7 times, once for every partition shape :
 *  16x16, 16x8, 8x16, 8x8, 8x4, 4x8, 4x4
*/



#include "../global.h"
#include "stdint.h"
#include "../util/formulas.h"



#ifndef WIDTH
#define WIDTH 16
#endif
#ifndef HEIGHT
#define HEIGHT 16
#endif



// several indirection layers are needed to expand WIDTH and HEIGHT because of the pasting token

#define QPEL_FUNC_NAME3(yoff, xoff, W, H) qpel_ ## yoff ## xoff ## _ ## W ## x ## H
#define QPEL_FUNC_NAME2(yoff, xoff, W, H) QPEL_FUNC_NAME3(yoff, xoff, W, H)
#define QPEL_FUNC_NAME(yoff, xoff)        QPEL_FUNC_NAME2(yoff, xoff, WIDTH, HEIGHT)

#define QPEL_FUNC2(yoff, xoff, ...) QPEL_FUNC_NAME(yoff, xoff)(__VA_ARGS__)
#define QPEL_FUNC(yoff, xoff, ...) QPEL_FUNC2(yoff, xoff, __VA_ARGS__)

#define QPEL_HELPER3(name, W, H, ...) name ## _ ## W ## x ## H(__VA_ARGS__)
#define QPEL_HELPER2(name, W, H, ...) QPEL_HELPER3(name, W, H, __VA_ARGS__)
#define QPEL_HELPER(name, ...) QPEL_HELPER2(name, WIDTH, HEIGHT, __VA_ARGS__)

#define QPEL_FUNCS_ARRAY3(W, H)  qpel_funcs_ ## W ## x ## H
#define QPEL_FUNCS_ARRAY2(W, H)  QPEL_FUNCS_ARRAY3(W, H)
#define QPEL_FUNCS_ARRAY  QPEL_FUNCS_ARRAY2(WIDTH, HEIGHT)





static void (*QPEL_FUNCS_ARRAY[16])(const uint8_t*, uint8_t*, int16_t*, int);




static always_inline int32_t QPEL_HELPER(vertical_pass_unclipped, const uint8_t *ref, int y, int x) {
    return       ref[(y-2)*(WIDTH+5) + x]
            -  5*ref[(y-1)*(WIDTH+5) + x]
            + 20*ref[(y+0)*(WIDTH+5) + x]
            + 20*ref[(y+1)*(WIDTH+5) + x]
            -  5*ref[(y+2)*(WIDTH+5) + x]
            +    ref[(y+3)*(WIDTH+5) + x];
}
static always_inline int32_t QPEL_HELPER(horizontal_pass_unclipped, const uint8_t *ref, int y, int x) {
    return       ref[y*(WIDTH+5) + x-2]
            -  5*ref[y*(WIDTH+5) + x-1]
            + 20*ref[y*(WIDTH+5) + x+0]
            + 20*ref[y*(WIDTH+5) + x+1]
            -  5*ref[y*(WIDTH+5) + x+2]
            +    ref[y*(WIDTH+5) + x+3];
}

static always_inline uint8_t QPEL_HELPER(vertical_pass, const uint8_t *ref, int y, int x) {
    return _clip1y(
        (QPEL_HELPER(vertical_pass_unclipped, ref, y, x) + 16) >> 5,
            MAX_U8);
}

static always_inline uint8_t QPEL_HELPER(horizontal_pass, const uint8_t *ref, int y, int x) {
    return _clip1y(
        (QPEL_HELPER(horizontal_pass_unclipped, ref, y, x) + 16) >> 5,
            MAX_U8);
}

static always_inline void QPEL_HELPER(precompute_vertical_passes,
                                      const uint8_t *restrict ref, int16_t *restrict qpel_pass_buf, int y) {
    for (int x = 0; x < WIDTH+5; x++) {
        qpel_pass_buf[x] = QPEL_HELPER(vertical_pass_unclipped, ref, y, x);
    }
}


static always_inline uint8_t QPEL_HELPER(horizontal_filter, int16_t *qpel_pass_buf, int x) {
    return _clip1y(
            (1 * qpel_pass_buf[x-2]
            -  5 * qpel_pass_buf[x-1]
            + 20 * qpel_pass_buf[x+0]
            + 20 * qpel_pass_buf[x+1]
            -  5 * qpel_pass_buf[x+2]
            +  1 * qpel_pass_buf[x+3] + 512) >> 10,
            MAX_U8);
}



// naming : qpel_yx_WxH with y = vertical fractional offset and x = horizontal fractional offset

void QPEL_FUNC(0, 0, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // G
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = ref[(2+y)*(WIDTH+5) + 2+x];
        }
        dst += stride;
    }
}

void QPEL_FUNC(0, 1, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // a
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int b = QPEL_HELPER(horizontal_pass, ref, 2+y, 2+x);
            dst[x]  = (ref[(2+y)*(WIDTH+5) + 2+x] + b + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(0, 2, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // b
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = QPEL_HELPER(horizontal_pass, ref, 2+y, 2+x);
        }
        dst += stride;
    }
}

void QPEL_FUNC(0, 3, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // c
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int b = QPEL_HELPER(horizontal_pass, ref, 2+y, 2+x);
            dst[x] = (ref[(2+y)*(WIDTH+5) + 2+x+1] + b + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(1, 0, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // d
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int h = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x);
            dst[x] = (ref[(2+y)*(WIDTH+5) + 2+x] + h + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(1, 1, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // e
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int b = QPEL_HELPER(horizontal_pass, ref, 2+y, 2+x);
            int h = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x);
            dst[x] = (b + h + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(1, 2, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // f
    for (int y = 0; y < HEIGHT; y++) {
        QPEL_HELPER(precompute_vertical_passes, ref, qpel_pass_buf, 2+y);
        for (int x = 0; x < WIDTH; x++) {
            int b = QPEL_HELPER(horizontal_pass, ref, 2+y, 2+x);
            int j = QPEL_HELPER(horizontal_filter, qpel_pass_buf, 2+x);
            dst[x] = (b + j + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(1, 3, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // g
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int b = QPEL_HELPER(horizontal_pass, ref, 2+y, 2+x);
            int m = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x+1);
            dst[x] = (b + m + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(2, 0, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // h
    for (int y = 0; y < HEIGHT; y++){
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x);
        }
        dst += stride;
    }
}

void QPEL_FUNC(2, 1, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // i
    for (int y = 0; y < HEIGHT; y++) {
        QPEL_HELPER(precompute_vertical_passes, ref, qpel_pass_buf, 2+y);
        for (int x = 0; x < WIDTH; x++) {
            int h = _clip1y((qpel_pass_buf[2+x] + 16) >> 5, MAX_U8);
            int j = QPEL_HELPER(horizontal_filter, qpel_pass_buf, 2+x);
            dst[x] = (h + j + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(2, 2, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // j
    for (int y = 0; y < HEIGHT; y++) {
        QPEL_HELPER(precompute_vertical_passes, ref, qpel_pass_buf, 2+y);
        for (int x = 0; x < WIDTH; x++) {
            dst[x] = QPEL_HELPER(horizontal_filter, qpel_pass_buf, 2+x);
        }
        dst += stride;
    }
}

void QPEL_FUNC(2, 3, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // k
    for (int y = 0; y < HEIGHT; y++) {
        QPEL_HELPER(precompute_vertical_passes, ref, qpel_pass_buf, 2+y);
        for (int x = 0; x < WIDTH; x++) {
            int j = QPEL_HELPER(horizontal_filter, qpel_pass_buf, 2+x);
            int m = _clip1y((qpel_pass_buf[2+x+1] + 16) >> 5, MAX_U8);
            dst[x] = (j + m + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(3, 0, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // n
    for (int y = 0; y < HEIGHT; y++){
        for (int x = 0; x < WIDTH; x++) {
            int h = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x);
            dst[x] = (ref[(2+y+1)*(WIDTH+5) + 2+x] + h + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(3, 1, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // p
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int h = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x);
            int s = QPEL_HELPER(horizontal_pass, ref, 2+y+1, 2+x);
            dst[x] = (h + s + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(3, 2, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // q
    for (int y = 0; y < HEIGHT; y++) {
        QPEL_HELPER(precompute_vertical_passes, ref, qpel_pass_buf, 2+y);
        for (int x = 0; x < WIDTH; x++) {
            int j = QPEL_HELPER(horizontal_filter, qpel_pass_buf, 2+x);
            int s = QPEL_HELPER(horizontal_pass, ref, 2+y+1, 2+x);
            dst[x] = (j + s + 1) >> 1;
        }
        dst += stride;
    }
}

void QPEL_FUNC(3, 3, const uint8_t *restrict ref, uint8_t *restrict dst, int16_t *restrict qpel_pass_buf, int stride) { // r
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int m = QPEL_HELPER(vertical_pass, ref, 2+y, 2+x+1);
            int s = QPEL_HELPER(horizontal_pass, ref, 2+y+1, 2+x);
            dst[x] = (m + s + 1) >> 1;
        }
        dst += stride;
    }
}


static void (*QPEL_FUNCS_ARRAY[16])(const uint8_t*, uint8_t*, int16_t*, int) = {
    QPEL_FUNC_NAME(0, 0), QPEL_FUNC_NAME(0, 1), QPEL_FUNC_NAME(0, 2), QPEL_FUNC_NAME(0, 3),
    QPEL_FUNC_NAME(1, 0), QPEL_FUNC_NAME(1, 1), QPEL_FUNC_NAME(1, 2), QPEL_FUNC_NAME(1, 3),
    QPEL_FUNC_NAME(2, 0), QPEL_FUNC_NAME(2, 1), QPEL_FUNC_NAME(2, 2), QPEL_FUNC_NAME(2, 3),
    QPEL_FUNC_NAME(3, 0), QPEL_FUNC_NAME(3, 1), QPEL_FUNC_NAME(3, 2), QPEL_FUNC_NAME(3, 3),
};




#undef QPEL_FUNC_NAME3
#undef QPEL_FUNC_NAME2
#undef QPEL_FUNC_NAME

#undef QPEL_FUNC2
#undef QPEL_FUNC

#undef QPEL_HELPER3
#undef QPEL_HELPER2
#undef QPEL_HELPER