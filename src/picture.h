//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_Picture_H
#define TOY_H264_Picture_H


#include "slice.h"
#include "mb.h"


typedef struct Picture {
    Macroblock *mb_array;
    int        num_mbs;

    int        width;
    int        height;
    int        poc;

    uint8_t   *luma;
    uint8_t   *cb;
    uint8_t   *cr;

    int strideY, strideC;

    uint8_t (*luma_total_coeffs) [16];
    uint8_t (*cb_total_coeffs)   [16];
    uint8_t (*cr_total_coeffs)   [16];
} Picture ;


typedef struct Slice {
    SliceHeader *sh;

    Picture *p_pic;
    int num_mbs;

    decode_macroblock_func decode_macroblock;
} Slice ;



ALWAYS_INLINE uint8_t *Picture_luma_ptr(Picture *f, int mbAddr, int mb_width, int blk_x, int blk_y) {
    int mb_x = (mbAddr % mb_width) * 16;
    int mb_y = (mbAddr / mb_width) * 16;
    return &f->luma[(mb_y + blk_y) * f->width + mb_x + blk_x];
}


Picture *picture_alloc(int width, int height, int nb_mbs);
Slice   *slice_alloc();
void     slice_free(Slice *slice);
void     slice_reset(Slice *slice);
void     picture_free(Picture *f);
void     dump_picture(Picture *f, CodecContext *ctx);


#endif //TOY_H264_Picture_H