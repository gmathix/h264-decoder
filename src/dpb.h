//
// Created by gmathix on 4/21/26.
//

#ifndef TOY_H264_DPB_H
#define TOY_H264_DPB_H


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "picture.h"


#define MAX_DPB_SIZE     16
#define MAX_PIC_NUM      1000000
#define MIN_PIC_NUM    (-1000000)

enum DpbStatus {
    UNUSED_FOR_REF      = 0,
    USED_SHORT_TERM_REF = 1,
    USED_LONG_TERM_REF  = 2,
    NON_EXISTING        = 3,
};


typedef struct DPB {
    int size;
    int fullness;

    Picture *slots[MAX_DPB_SIZE];
    Picture *l0[MAX_DPB_SIZE+1]; // safety extra slot
    Picture *l1[MAX_DPB_SIZE+1];
    int effective_ref_idx_l0_active;
    int effective_ref_idx_l1_active;

    /* last picture in decoding order */
    Picture *prevPic;
    bool mmco_5_prev_occured;

    CodecContext *ctx;

    int prevPocMsb;
    int prevPocLsb;
    int maxPocLsb;
} DPB ;



typedef int (*PictureField) (const Picture *pic);

static int returnPicNum(const Picture *pic)   { return pic->pic_num; }
static int returnLTPicNum(const Picture *pic) { return pic->long_term_frame_idx; }
static int returnPoc(const Picture *pic)      { return pic->poc; }


typedef bool (*RefTypeCriteria) (const Picture *pic);

static bool shortTermCriteria(const Picture *pic) { return pic->dpb_status == USED_SHORT_TERM_REF; }
static bool longTermCriteria(const Picture *pic)  { return pic->dpb_status == USED_LONG_TERM_REF; }


typedef bool (*ReferenceComparator) (int field, int ref);

static bool greaterThan(int field, int ref)    { return field > ref; }
static bool lowerThan(int field, int ref)      { return field < ref; }
static bool greaterOrEqual(int field, int ref) { return field >= ref; }
static bool lowerOrEqual(int field, int ref)   { return field <= ref; }
static bool equalTo(int field, int ref)        { return field == ref; }
static bool dontCare(int field, int ref)       { return true; }



/* returns how many pictures were added to the dest list */
static int sortToRefList(DPB *dpb, bool descending, Picture **dest, int *idx,
    PictureField fieldGetter, RefTypeCriteria criteria, ReferenceComparator comparator, Picture *refPic) {
    int nbAdded = 0;

    int bestIdx = 0;
    int best = descending ? INT32_MIN : INT32_MAX;
    int prevBest = descending ? INT32_MAX : INT32_MIN;

    for (int i = 0; i < MAX_DPB_SIZE+1; i++) {
        for (int j = 0; j < MAX_DPB_SIZE+1; j++) {
            Picture *pic = dpb->slots[j];
            if (pic == NULL) continue;
            int field = (fieldGetter)(pic);
            int fieldRef = (fieldGetter)(refPic);
            bool compareResult = descending ?
                  field < prevBest && field > best
                : field > prevBest && field < best;
            bool criteriaResult = (criteria)(pic);
            bool refCompareResult = (comparator)(field, fieldRef);
            if (criteriaResult && compareResult && refCompareResult) {
                best = field;
                bestIdx = j;
            }
        }
        if ((descending && best > INT32_MIN) || (!descending && best < INT32_MAX)) {
            dest[(*idx)++] = dpb->slots[bestIdx];
            nbAdded++;
            prevBest = best;
            best = descending ? INT32_MIN : INT32_MAX;
        } else break;
    }

    return nbAdded;
}




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
void decode_pic_nums(DPB *dpb, SliceHeader *sh);
int  bump(DPB *dpb);
int  output_oldest_pic(DPB *dpb); // returns of output picture
void store_picture(DPB *dpb, Picture *pic);

void init_ref_pic_lists(DPB *dpb, SliceHeader *sh);

void ref_pic_list_modification(uint8_t type, Slice *slice, int maxFrameNum, int *maxLtIdx, CodecContext *ctx);
void ref_pic_list_modif_st(Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int abs_diff, int maxFrameNum, CodecContext *ctx);
void ref_pic_list_modif_lt(Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int lt_pic_num, int *maxLtIdx, CodecContext *ctx);
void dec_ref_pic_marking(DPB *dpb, Slice *slice,  BitReader *br);

void dpb_empty_slots(DPB *dpb);
void dpb_empty_ref_lists(DPB *dpb);
void dpb_flush(DPB *dpb);
void dpb_free(DPB *dpb);





#endif //TOY_H264_DPB_H