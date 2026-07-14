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



static ALWAYS_INLINE int32_t vertical_pass_unclipped_2(const uint8_t *ref, int y, int x, int width) {
    return       ref[(y-2)*width + x] - 5*ref[(y-1)*width + x] + 20*ref[(y+0)*width + x]
            + 20*ref[(y+1)*width + x] - 5*ref[(y+2)*width + x] +    ref[(y+3)*width + x];
}
static ALWAYS_INLINE int32_t horizontal_pass_unclipped_2(const uint8_t *ref, int y, int x, int width) {
    return      ref[y*width + x-2] - 5*ref[y*width + x-1] + 20*ref[y*width + x+0]
           + 20*ref[y*width + x+1] - 5*ref[y*width + x+2] +    ref[y*width + x+3];
}

static ALWAYS_INLINE uint8_t vertical_pass_2(const uint8_t *ref, int y, int x, int width) {
    return _clip1y(
        (vertical_pass_unclipped_2(ref, y, x, width) + 16) >> 5,
            8);
}

static ALWAYS_INLINE uint8_t horizontal_pass_2(const uint8_t *ref, int y, int x, int width) {
    return _clip1y(
        (horizontal_pass_unclipped_2(ref, y, x, width) + 16) >> 5,
            8);
}

static ALWAYS_INLINE uint8_t vertical_filter_2(const uint8_t *ref, int y, int x, int width) {
    return _clip1y(
            (1 * horizontal_pass_unclipped_2(ref, y-2, x, width)
            -  5 * horizontal_pass_unclipped_2(ref, y-1, x, width)
            + 20 * horizontal_pass_unclipped_2(ref, y+0, x, width)
            + 20 * horizontal_pass_unclipped_2(ref, y+1, x, width)
            -  5 * horizontal_pass_unclipped_2(ref, y+2, x, width)
            +  1 * horizontal_pass_unclipped_2(ref, y+3, x, width) + 512) >> 10,
            8);
}


// naming : qpel_yx with y = vertical fractional offset and x = horizontal fractional offset

void qpel_00(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // G

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dst[x] = ref[(2+y)*(width+5) + 2+x];
        }
        dst += stride;
    }
}

void qpel_01(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // a

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int b = horizontal_pass_2(ref, 2+y, 2+x, width+5);
            dst[x]  = (ref[(2+y)*(width+5) + 2+x] + b + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_02(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // b

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dst[x] = horizontal_pass_2(ref, 2+y, 2+x, width+5);
        }
        dst += stride;
    }
}

void qpel_03(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // c

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int b = horizontal_pass_2(ref, 2+y, 2+x, width+5);
            dst[x] = (ref[(2+y)*(width+5) + 2+x+1] + b + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_10(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // d

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int h = vertical_pass_2(ref, 2+y, 2+x, width+5);
            dst[x] = (ref[(2+y)*(width+5) + 2+x] + h + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_11(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // e

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int b = horizontal_pass_2(ref, 2+y, 2+x, width+5);
            int h = vertical_pass_2(ref, 2+y, 2+x, width+5);
            dst[x] = (b + h + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_12(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // f

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int b = horizontal_pass_2(ref, 2+y, 2+x, width+5);
            int j = vertical_filter_2(ref, 2+y, 2+x, width+5);
            dst[x] = (b + j + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_13(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // g

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int b = horizontal_pass_2(ref, 2+y, 2+x, width+5);
            int m = vertical_pass_2(ref, 2+y, 2+x+1, width+5);
            dst[x] = (b + m + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_20(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // h

    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++) {
            dst[x] = vertical_pass_2(ref, 2+y, 2+x, width+5);
        }
        dst += stride;
    }
}

void qpel_21(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // i

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int h = vertical_pass_2(ref, 2+y, 2+x, width+5);
            int j = vertical_filter_2(ref, 2+y, 2+x, width+5);
            dst[x] = (h + j + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_22(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // j

    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++) {
            dst[x]  = vertical_filter_2(ref, y+2, x+2, width+5);
        }
        dst += stride;
    }
}

void qpel_23(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // k

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int j = vertical_filter_2(ref, 2+y, 2+x, width+5);
            int m = vertical_pass_2(ref, 2+y, 2+x+1, width+5);
            dst[x] = (j + m + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_30(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // n

    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++) {
            int h = vertical_pass_2(ref, 2+y, 2+x, width+5);
            dst[x] = (ref[(2+y+1)*(width+5) + 2+x] + h + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_31(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // p

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int h = vertical_pass_2(ref, 2+y, 2+x, width+5);
            int s = horizontal_pass_2(ref, 2+y+1, 2+x, width+5);
            dst[x] = (h + s + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_32(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // q

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int j = vertical_filter_2(ref, 2+y, 2+x, width+5);
            int s = horizontal_pass_2(ref, 2+y+1, 2+x, width+5);
            dst[x] = (j + s + 1) >> 1;
        }
        dst += stride;
    }
}

void qpel_33(const uint8_t * restrict ref, uint8_t * restrict dst,
    int stride, int width, int height) { // r

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int m = vertical_pass_2(ref, 2+y, 2+x+1, width+5);
            int s = horizontal_pass_2(ref, 2+y+1, 2+x, width+5);
            dst[x] = (m + s + 1) >> 1;
        }
        dst += stride;
    }
}
