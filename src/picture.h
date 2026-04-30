//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_Picture_H
#define TOY_H264_Picture_H


#include "slice.h"
#include "mb.h"


typedef struct Picture {
    int nal_ref_idc;

    /* needed for the DPB */
    bool is_idr;
    bool long_term_ref;
    bool num_ref_idx_active_override;
    bool adaptive_ref_pic_marking_mode;
    int  frame_num;
    int  frame_num_offset;
    int  pic_num;


    /* after dpb storing */
    int dpb_status;
    int long_term_frame_idx;
    bool is_output;

    int top_field_order_cnt;
    int bottom_field_order_cnt;
    int poc;


    /* will only be used for attributes that are common for all slices of one picture */
    SliceHeader *sh;


    int        num_mbs;

    int        width;
    int        height;


    uint8_t   *luma;
    uint8_t   *cb;
    uint8_t   *cr;

    int strideY, strideC;
} Picture ;


typedef struct Slice {
    SliceHeader *sh;

    Picture *p_pic;
    int num_mbs;

    decode_macroblock_func decode_macroblock;

    int picNumL0Pred;
    int picNumL1Pred;
} Slice ;



ALWAYS_INLINE uint8_t *Picture_luma_ptr(Picture *f, int mbAddr, int mb_width, int blk_x, int blk_y) {
    int mb_x = (mbAddr % mb_width) * 16;
    int mb_y = (mbAddr / mb_width) * 16;
    return &f->luma[(mb_y + blk_y) * f->width + mb_x + blk_x];
}


Picture *picture_alloc(SliceHeader *sh, CodecContext *ctx);
Slice   *slice_alloc();
void     slice_free(Slice *slice);
void     slice_reset(Slice *slice);
void     picture_free(Picture *f);
void     picture_reset(Picture *f);
void     dump_picture(Picture *f, CodecContext *ctx);


#endif //TOY_H264_Picture_H