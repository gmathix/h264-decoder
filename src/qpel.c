//
// Created by gmathix on 5/4/26.
//



#include "qpel.h"

#include "util/formulas.h"


const qpel_func qpel_funcs[16] = {
    qpel_00, qpel_01, qpel_02, qpel_03,
    qpel_10, qpel_11, qpel_12, qpel_13,
    qpel_20, qpel_21, qpel_22, qpel_23,
    qpel_30, qpel_31, qpel_32, qpel_33
};



static ALWAYS_INLINE int32_t vertical_pass_unclipped(uint8_t ref[9][9], int y, int x, int bit_depth) {
    return ref[y-2][x] - 5*ref[y-1][x] + 20*ref[y+0][x] + 20*ref[y+1][x] - 5*ref[y+2][x] + ref[y+3][x];
}
static ALWAYS_INLINE int32_t horizontal_pass_unclipped(uint8_t ref[9][9], int y, int x, int bit_depth) {
    return ref[y][x-2] - 5*ref[y][x-1] + 20*ref[y][x+0] + 20*ref[y][x+1] - 5*ref[y][x+2] + ref[y][x+3];
}

static ALWAYS_INLINE uint8_t vertical_pass(uint8_t ref[9][9], int y, int x, int bit_depth) {
    return _clip1y(
        (vertical_pass_unclipped(ref, y, x, bit_depth) + 16) >> 5,
            bit_depth);
}

static ALWAYS_INLINE uint8_t horizontal_pass(uint8_t ref[9][9], int y, int x, int bit_depth) {
    return _clip1y(
        (horizontal_pass_unclipped(ref, y, x, bit_depth) + 16) >> 5,
            bit_depth);
}

static ALWAYS_INLINE uint8_t vertical_filter(uint8_t ref[9][9], int y, int x, int bit_depth) {
    return _clip1y(
            (1 * horizontal_pass_unclipped(ref, y-2, x, bit_depth)
            -  5 * horizontal_pass_unclipped(ref, y-1, x, bit_depth)
            + 20 * horizontal_pass_unclipped(ref, y+0, x, bit_depth)
            + 20 * horizontal_pass_unclipped(ref, y+1, x, bit_depth)
            -  5 * horizontal_pass_unclipped(ref, y+2, x, bit_depth)
            +  1 * horizontal_pass_unclipped(ref, y+3, x, bit_depth) + 512) >> 10,
            bit_depth);
}


static ALWAYS_INLINE uint8_t horizontal_filter(uint8_t ref[9][9], int y, int x, int  bit_depth) {
    return _clip1y(
            (1 * vertical_pass_unclipped(ref, y, x-2, bit_depth)
            -  5 * vertical_pass_unclipped(ref, y, x-1, bit_depth)
            + 20 * vertical_pass_unclipped(ref, y, x+0, bit_depth)
            + 20 * vertical_pass_unclipped(ref, y, x+1, bit_depth)
            -  5 * vertical_pass_unclipped(ref, y, x+2, bit_depth)
            +  1 * vertical_pass_unclipped(ref, y, x+3, bit_depth) + 512) >> 10,
            bit_depth);
}




void qpel_00(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // G
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            *(dst++) = ref[2+y][2+x];
        }
        dst += stride - 4;
    }
}

void qpel_01(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // a
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int b = horizontal_pass(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (ref[2+y][2+x] + b + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_02(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // b
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            *(dst++) = horizontal_pass(ref, 2+y, 2+x, bit_depth);
        }
        dst += stride - 4;
    }
}

void qpel_03(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // c
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int b = horizontal_pass(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (ref[2+y][2+x+1] + b + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_10(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // d
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int h = vertical_pass(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (ref[2+y][2+x] + h + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_11(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // e
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int b = horizontal_pass(ref, 2+y, 2+x, bit_depth);
            int h = vertical_pass(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (b + h + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_12(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // f
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int b = horizontal_pass(ref, 2+y, 2+x, bit_depth);
            int j = vertical_filter(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (b + j + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_13(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // g
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int b = horizontal_pass(ref, 2+y, 2+x, bit_depth);
            int m = vertical_pass(ref, 2+y, 2+x+1, bit_depth);
            *(dst++) = (b + m + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_20(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // h
    for (int y = 0; y < 4; y++){
        for (int x = 0; x < 4; x++) {
            *(dst++) = vertical_pass(ref, 2+y, 2+x, bit_depth);
        }
        dst += stride - 4;
    }
}

void qpel_21(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // i
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int h = vertical_pass(ref, 2+y, 2+x, bit_depth);
            int j = vertical_filter(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (h + j + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_22(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // j
    for (int y = 0; y < 4; y++){
        for (int x = 0; x < 4; x++) {
            *(dst++) = vertical_filter(ref, y+2, x+2, bit_depth);
        }
        dst += stride - 4;
    }
}

void qpel_23(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // k
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int j = vertical_filter(ref, 2+y, 2+x, bit_depth);
            int m = vertical_pass(ref, 2+y, 2+x+1, bit_depth);
            *(dst++) = (j + m + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_30(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // n
    for (int y = 0; y < 4; y++){
        for (int x = 0; x < 4; x++) {
            int h = vertical_pass(ref, 2+y, 2+x, bit_depth);
            *(dst++) = (ref[2+y+1][2+x] + h + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_31(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // p
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int h = vertical_pass(ref, 2+y, 2+x, bit_depth);
            int s = horizontal_pass(ref, 2+y+1, 2+x, bit_depth);
            *(dst++) = (h + s + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_32(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // q
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int j = vertical_filter(ref, 2+y, 2+x, bit_depth);
            int s = horizontal_pass(ref, 2+y+1, 2+x, bit_depth);
            *(dst++) = (j + s + 1) >> 1;
        }
        dst += stride - 4;
    }
}

void qpel_33(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth) { // r
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int m = vertical_pass(ref, 2+y, 2+x+1, bit_depth);
            int s = horizontal_pass(ref, 2+y+1, 2+x, bit_depth);
            *(dst++) = (m + s + 1) >> 1;
        }
        dst += stride - 4;
    }
}
