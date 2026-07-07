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



enum DpbStatus {
    UNUSED_REF        = 0,
    SHORT_TERM_REF    = 1,
    LONG_TERM_REF     = 2,
    NON_EXISTING_REF  = 3,
};

typedef struct DPB {
    int size;
    int fullness;
    size_t pictures_dumped;

    Picture *slots[MAX_DPB_SIZE];
    // Picture *l0[1+MAX_DPB_SIZE+1]; // empty picture slot + pictures + safety extra slot at the end
    // Picture *l1[1+MAX_DPB_SIZE+1];
    Picture *lists[2][1+MAX_DPB_SIZE+1];
    int effective_ref_idx_l0_active;
    int effective_ref_idx_l1_active;

    /* last picture in decoding order */
    Picture *prevPic;
    bool mmco_5_prev_occured;

    CodecContext *ctx;

    int prevPocMsb;
    int prevPocLsb;
    int maxPocLsb;

    int curr_pic_dpb_id; // assign a unique id to each picture currently present in the DPB slots
                         // value 0 is reserved to EMPTY_PIC
} DPB ;





/* some java-style sorting abstraction for l0 and l1 initializations because i miss java :( */

typedef int (*PictureField) (const Picture *pic);

static int returnPicNum(const Picture *pic)   { return pic->pic_num; }
static int returnLTPicNum(const Picture *pic) { return pic->long_term_frame_idx; }
static int returnPoc(const Picture *pic)      { return pic->poc; }


typedef bool (*RefTypeCriteria) (const Picture *pic);

static bool shortTermCriteria(const Picture *pic) { return pic->dpb_status == SHORT_TERM_REF; }
static bool longTermCriteria(const Picture *pic)  { return pic->dpb_status == LONG_TERM_REF; }


typedef bool (*ReferenceComparator) (int field, int ref);

static bool greaterThan(int field, int ref)    { return field > ref; }
static bool lowerThan(int field, int ref)      { return field < ref; }
static bool greaterOrEqual(int field, int ref) { return field >= ref; }
static bool lowerOrEqual(int field, int ref)   { return field <= ref; }
static bool equalTo(int field, int ref)        { return field == ref; }
static bool dontGiveAShit(int field, int ref)  { return true; }


/* returns how many pictures were added to the dest list */
static int sortToRefList(DPB *dpb, bool descending, int list, int *idx,
    PictureField fieldGetter, RefTypeCriteria criteria, ReferenceComparator comparator, Picture *refPic) {

    int nbAdded = 0;

    int bestIdx = 0;
    int best = descending ? INT32_MIN : INT32_MAX;
    int prevBest = descending ? INT32_MAX : INT32_MIN;

    for (int i = 0; i < MAX_DPB_SIZE; i++) {
        for (int j = 0; j < MAX_DPB_SIZE; j++) {
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
            Picture *pic = dpb->slots[bestIdx];
            if (!pic->in_list[list]) {
                pic->lowest_list_index[list] = 1 + *idx;
                pic->in_list[list] = true;
            }

            dpb->lists[list][1+(*idx)++] = pic;

            nbAdded++;
            prevBest = best;
            best = descending ? INT32_MIN : INT32_MAX;
        } else break;
    }

    return nbAdded;
}

static Picture *findPic(DPB *dpb, PictureField fieldGetter, int fieldValue) {
    for (int i = 0; i < dpb->size; i++) {
        Picture *pic = dpb->slots[i];
        if (pic == NULL) continue;
        if ((fieldGetter)(pic) == fieldValue) {
            return pic;
        }
    }
    return NULL;
}

static Picture *findRefPic(DPB *dpb, PictureField fieldGetter, int fieldValue) {
    for (int i = 0; i < dpb->size; i++) {
        Picture *pic = dpb->slots[i];
        if (pic == NULL) continue;
        if ((fieldGetter)(pic) == fieldValue && pic->dpb_status != UNUSED_REF) {
            return pic;
        }
    }
    return NULL;
}


static DPB *make_dbp(CodecContext *ctx) {
    DPB *dpb = calloc(1, sizeof(DPB));


    dpb->ctx = ctx;
    dpb->size = MAX_DPB_SIZE; // FIXME use max size specified by level
    dpb->fullness = 0;

    dpb->maxPocLsb = -1;

    dpb->curr_pic_dpb_id = 1;
    for (int i = 0; i < MAX_DPB_SIZE+2; i++) {
        dpb->lists[L0][i] = &EMPTY_PICTURE;
        dpb->lists[L1][i] = &EMPTY_PICTURE;
    }

    /* start at 0 for first picture */
    dpb->prevPocLsb = 0;
    dpb->prevPocMsb = 0;


    return dpb;
}

static inline int picNum(DPB *dpb, Picture **lX, int idx, int maxPicNum) {
    if (lX[idx] != NULL && lX[idx]->dpb_status == SHORT_TERM_REF)
        return lX[idx]->pic_num;
    return maxPicNum;
}

static inline int ltPicNum(DPB *dpb, Picture **lX, int idx, int maxLtIdx) {
    if (lX[idx] != NULL && lX[idx]->dpb_status == LONG_TERM_REF)
        return lX[idx]->long_term_frame_idx;
    return 2 * (maxLtIdx + 1);
}



void derive_poc(DPB *dpb, Picture *pic);
void decode_pic_nums(DPB *dpb, SliceHeader *sh);
int  bump(DPB *dpb);
int  output_oldest_pic(DPB *dpb); // returns of output picture
void store_picture(DPB *dpb, Picture *pic);

void init_ref_pic_lists(DPB *dpb, SliceHeader *sh);


/* MMCOs */
void mark_st_pic_unused(DPB *dpb, int picNum);
void mark_lt_pic_unused(DPB *dpb, int ltPicNum);
void assign_lt_idx_to_st_pic(DPB *dpb, int picNum, int lt_frame_idx);
void decode_max_lt_frame_idx(DPB *dpb, int max_lt_frame_idx);
void mark_all_unused(DPB *dpb);
void mark_curr_pic_lt(DPB *dpb, int lt_frame_idx);


void ref_pic_list_modification(uint8_t type, Slice *slice, int maxFrameNum, int *maxLtIdx, CodecContext *ctx);
void ref_pic_list_modif_st(Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int abs_diff, int maxFrameNum, CodecContext *ctx);
void ref_pic_list_modif_lt(Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int lt_pic_num, int *maxLtIdx, CodecContext *ctx);
void dec_ref_pic_marking(DPB *dpb, Slice *slice,  BitReader *br);
void process_mmcos(Picture *pic, CodecContext *ctx);

void dpb_empty_slots(DPB *dpb);
void dpb_empty_ref_lists(DPB *dpb);
void dpb_flush(DPB *dpb);
void dpb_free(DPB *dpb);





#endif //TOY_H264_DPB_H