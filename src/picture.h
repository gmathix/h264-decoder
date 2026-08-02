//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_Picture_H
#define TOY_H264_Picture_H


#include "global.h"

#include "decoder.h"
#include "motion_info.h"
#include "ps.h"

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



typedef struct PicturePool {
	Picture *slots[MAX_DPB_SIZE + 2];
	bool     available[MAX_DPB_SIZE + 2];
	int      nb_available;
	int      size;
} PicturePool ;





Picture *picture_alloc(SPS *sps, Undo264Context *ctx);
void     picture_free(Picture *p);
void     picture_reset(Picture *p);
void     dump_picture(Picture *p, Undo264Context *ctx);

void pic_pool_init(PicturePool *pool, Undo264Context *ctx);
void pic_pool_free(PicturePool *pool, Undo264Context *ctx);
Picture *pic_pool_get(PicturePool *pool);
void pic_pool_getback(Picture *pic, PicturePool *pool);

#endif //TOY_H264_Picture_H