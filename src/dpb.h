//
// Created by gmathix on 4/21/26.
//

#ifndef TOY_H264_DPB_H
#define TOY_H264_DPB_H


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "picture.h"


#define MAX_DPB_SIZE     16;



enum DpbStatus {
    UNUSED_FOR_REF      = 0,
    USED_SHORT_TERM_REF = 1,
    USED_LONG_TERM_REF  = 2,
    NON_EXISTING        = 3,
};


typedef struct DPB {
    int size;
    int fullness;

    Picture *slots[16];
    Picture *l0[17]; // safety extra slot
    Picture *l1[17];

    /* last picture in decoding order */
    Picture *prevPic;
    bool mmco_5_prev_occured;

    CodecContext *ctx;

    int prevPocMsb;
    int prevPocLsb;
    int maxPocLsb;
} DPB ;



static DPB *make_dbp(CodecContext *ctx) {
    DPB *dpb = calloc(1, sizeof(DPB));


    dpb->ctx = ctx;
    dpb->size = MAX_DPB_SIZE; // FIXME use max size specified by level
    dpb->fullness = 0;

    dpb->maxPocLsb = -1;

    /* start at 0 for first picture */
    dpb->prevPocLsb = 0;
    dpb->prevPocMsb = 0;


    return dpb;
}

static inline int picNum(DPB *dpb, Picture **lX, int idx, int maxPicNum) {
    if (lX[idx] != NULL && lX[idx]->dpb_status == USED_SHORT_TERM_REF)
        return lX[idx]->frame_num;
    return maxPicNum;
}

static inline int ltPicNum(DPB *dpb, Picture **lX, int idx, int maxLtIdx) {
    if (lX[idx] != NULL && lX[idx]->dpb_status == USED_LONG_TERM_REF)
        return lX[idx]->long_term_frame_idx;
    return 2 * (maxLtIdx + 1);
}



void derive_poc(DPB *dpb, Picture *pic);
void bump(DPB *dpb, int *index);
void store_picture(DPB *dpb, Picture *pic);

void init_ref_pic_lists(DPB *dpb, SliceHeader *sh);

void ref_pic_list_modification(DPB *dpb, uint8_t type, Slice *slice, int maxFrameNum, int *maxLtIdx, BitReader *br);
void ref_pic_list_modif_st(DPB *dpb, Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int abs_diff, int maxFrameNum);
void ref_pic_list_modif_lt(DPB *dpb, Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int lt_pic_num, int *maxLtIdx);
void dec_ref_pic_marking(DPB *dpb, Slice *slice,  BitReader *br);

void dpb_empty_slots(DPB *dpb);
void dpb_empty_ref_lists(DPB *dpb);
void dpb_flush(DPB *dpb);
void dpb_free(DPB *dpb);





#endif //TOY_H264_DPB_H