//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_Picture_H
#define TOY_H264_Picture_H


#include "global.h"
#include "slice.h"
#include "mb.h"


typedef struct Picture {
    int nal_ref_idc;

    /* needed for the DPB */
    bool is_idr;
    bool long_term_ref;
    bool num_ref_idx_active_override;
    bool adaptive_ref_pic_marking_mode;

    int  frame_num_offset;
    int  frame_num_wrap;
    int  frame_num;
    int  pic_num;
    int  poc;


    /* after dpb storing */
    int dpb_status;
    int long_term_frame_idx;
    bool is_output;

    int top_field_order_cnt;
    int bottom_field_order_cnt;



    /* will only be used for attributes that are common for all slices of one picture */
    SliceHeader *sh;


    int        num_mbs;

	/* uncropped */
	int widthY, heightY;
	int widthC, heightC;

    /* cropped */
	int widthCropY, heightCropY;
	int widthCropC, heightCropC;


    /* metadata used for B slices prediction */
    int *mb_types;
    MotionVector (*mvs_l0) [16];
    MotionVector (*mvs_l1) [16];
    bool (*pred_flag_l0) [4];
    bool (*pred_flag_l1) [4];


    uint8_t   *luma;
    uint8_t   *cb;
    uint8_t   *cr;
} Picture ;


typedef struct Slice {
    SliceHeader *sh;

    Picture *p_pic;
    int num_mbs;

    decode_macroblock_func decode_macroblock;

    int picNumL0Pred;
    int picNumL1Pred;
} Slice ;



static ALWAYS_INLINE uint8_t *Picture_luma_ptr(Picture *p, int mbAddr, int mb_width, int blk_x, int blk_y) {
    int mb_x = (mbAddr % mb_width) * 16;
    int mb_y = (mbAddr / mb_width) * 16;
    return &p->luma[(mb_y + blk_y) * p->widthY + mb_x + blk_x];
}


Picture *picture_alloc(SliceHeader *sh, CodecContext *ctx);
Slice   *slice_alloc();
void     slice_free(Slice *slice);
void     slice_reset(Slice *slice);
void     picture_free(Picture *p);

void     picture_reset(Picture *p);
void     dump_picture(Picture *p, CodecContext *ctx);


#endif //TOY_H264_Picture_H