//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_INTRA_H
#define TOY_H264_INTRA_H


#include "global.h"


#include "mb.h"


typedef void (*intra_pred_func)
    (uint8_t *dst, int stride,  int a_av, int b_av,
    const uint8_t *top, const uint8_t *left);

typedef void (*intra_pred_chroma_func)
    (uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at);



void vert_4x4_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void hor_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void dc_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void diag_down_left_4x4_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
 void diag_down_right_4x4_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void vert_right_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void hor_down_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void vert_left_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void hor_up_4x4_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);

void vert_8x8_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void hor_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void dc_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);
void diag_down_left_8x8_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);
void diag_down_right_8x8_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);
void vert_right_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);
void hor_down_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);
void vert_left_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);
void hor_up_8x8_pred(uint8_t *dst, int stride,  int a_av, int b_av,
        const uint8_t top[16], const uint8_t *left);

void vert_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void hor_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void dc_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);
void plane_16x16_pred(uint8_t *dst, int stride, int a_av, int b_av,
        const uint8_t *top, const uint8_t *left);



void vert_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at);
void hor_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at);
void dc_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at);
void plane_8x8_chroma_pred(uint8_t *dst_cb, uint8_t *dst_cr, int stride, int a_av, int b_av,
    const uint8_t *top_cb, const uint8_t *left_cb,
    const uint8_t *top_cr, const uint8_t *left_cr,
    int bitDepth, int chroma_at);



void intra_pred_4x4(Macroblock *mb, int blkIdx, int pred_mode, const Undo264Context *ctx);
void intra_pred_8x8(Macroblock *mb, int idx8x8, int pred_mode, const Undo264Context *ctx);
void intra_pred_16x16(Macroblock *mb, const Undo264Context *ctx);
void intra_chroma_pred(Macroblock *mb, const Undo264Context *ctx);


#endif //TOY_H264_INTRA_H