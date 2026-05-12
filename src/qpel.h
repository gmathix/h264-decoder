//
// Created by gmathix on 5/4/26.
//

#ifndef H264_DECODER_QPEL_H
#define H264_DECODER_QPEL_H



#include <stdint.h>



typedef void (*qpel_func)(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);


extern const qpel_func qpel_funcs[16];


void qpel_00(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_01(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_02(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_03(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_10(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_11(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_12(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_13(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_20(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_21(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_22(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_23(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_30(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_31(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_32(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);
void qpel_33(uint8_t ref[9][9], uint8_t *dst, int stride, int bit_depth);



#endif //H264_DECODER_QPEL_H