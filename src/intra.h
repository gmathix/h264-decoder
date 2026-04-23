//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_INTRA_H
#define TOY_H264_INTRA_H


#include "decoder.h"
#include "mb.h"


typedef void (*intra_pred_4x4_func)
    (uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

typedef void (*intra_pred_16x16_func)
    (uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth);

typedef void (*intra_pred_8x8_chroma_func)
    (uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at);



void vert_4x4_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void hor_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void dc_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void diag_down_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void diag_down_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void vert_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void hor_down_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void vert_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);

void hor_up_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t top_samples[9], const uint8_t left_samples[5]);



void OPTIMIZE_O3 vert_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth);

void OPTIMIZE_O3 hor_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth);

void OPTIMIZE_O3 dc_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth);

void OPTIMIZE_O3 plane_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
    const uint8_t top_samples[17], const uint8_t left_samples[17], int bitDepth);



void vert_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at);

void hor_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at);

void dc_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at);

void plane_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t top_samples_cb[9], const uint8_t left_samples_cb[9],
    const uint8_t top_samples_cr[9], const uint8_t left_samples_cr[9],
    int bitDepth, int chroma_at);



void intra_pred_4x4(Macroblock *mb, int blkIdx, int pred_mode, CodecContext *ctx);
void intra_pred_16x16(Macroblock *mb, CodecContext *ctx);
void intra_chroma_pred(Macroblock *mb, CodecContext *ctx);


#endif //TOY_H264_INTRA_H