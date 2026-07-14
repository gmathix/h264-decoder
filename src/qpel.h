//
// Created by gmathix on 5/4/26.
//

#ifndef H264_DECODER_QPEL_H
#define H264_DECODER_QPEL_H


#include "global.h"


typedef void (*qpel_func)(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);

extern const qpel_func qpel_funcs[16];

// naming : qpel_yx with y = vertical fractional offset and x = horizontal fractional offset
void qpel_00(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_01(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_02(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_03(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_10(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_11(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_12(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_13(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_20(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_21(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_22(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_23(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_30(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_31(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_32(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);
void qpel_33(const uint8_t *ref, uint8_t *dst, int stride, int width, int height);



#endif //H264_DECODER_QPEL_H