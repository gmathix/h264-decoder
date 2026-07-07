//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_Picture_H
#define TOY_H264_Picture_H


#include "global.h"

#include "decoder.h"
#include "motion_info.h"

typedef struct Picture {
    int nal_ref_idc;


    int  frame_num_offset;
    int  frame_num_wrap;
    int  frame_num;
    int  pic_num;
    int  poc;
    int  dpb_pic_id; // 0..MAX_DPB_SIZE
    bool in_list[2];
    int  lowest_list_index[2];


    /* after dpb storing */
    int dpb_status;
    int long_term_frame_idx;
    bool is_output;

    int top_field_order_cnt;
    int bottom_field_order_cnt;



    /* will only be used for attributes that are common for all slices of one picture */
    struct SliceHeader *sh;


    int        num_mbs;

	/* uncropped */
	int widthY, heightY;
	int widthC, heightC;

    /* cropped */
	int widthCropY, heightCropY;
	int widthCropC, heightCropC;


    /* metadata used for B slices prediction */
    int *mb_types;
    MotionInfo   (*motion_info) [16];
    bool (*pred_flags) [2][4];


    uint8_t   *luma;
    uint8_t   *cb;
    uint8_t   *cr;
} Picture ;

extern const Picture EMPTY_PICTURE;





static ALWAYS_INLINE uint8_t *Picture_luma_ptr(Picture *p, int mbAddr, int mb_width, int blk_x, int blk_y) {
    int mb_x = (mbAddr % mb_width) * 16;
    int mb_y = (mbAddr / mb_width) * 16;
    return &p->luma[(mb_y + blk_y) * p->widthY + mb_x + blk_x];
}


Picture *picture_alloc(struct SliceHeader *sh, CodecContext *ctx);

void     picture_free(Picture *p);

void     picture_reset(Picture *p);
void     dump_picture(Picture *p, CodecContext *ctx);


#endif //TOY_H264_Picture_H