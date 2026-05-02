//
// Created by gmathix on 4/21/26.
//

#include "dpb.h"

#include <limits.h>

#include "picture.h"
#include "util/expgolomb.h"
#include "util/sliceutil.h"


void derive_poc(DPB *dpb, Picture *pic) {
    if (dpb->maxPocLsb == -1) {
        dpb->maxPocLsb = 1 << (dpb->ctx->ps->sps->log2_max_poc_lsb_minus4 + 4);
    }
    if (pic->sh->sps->poc_type == 0) {
        /* most used, provides the most flexibility for complex GOP
         * and handles reordering and gaps in frame numbers gracefully
         */

        if (pic->is_idr) {
            dpb->prevPocLsb = 0;
            dpb->prevPocMsb = 0;
        }
        int curr_poc_lsb = pic->sh->poc_lsb;
        int pocMsb;
        if (curr_poc_lsb < dpb->prevPocLsb && dpb->prevPocLsb - curr_poc_lsb >= dpb->maxPocLsb / 2) {
            pocMsb = dpb->prevPocMsb + dpb->maxPocLsb;
        } else if (curr_poc_lsb > dpb->prevPocLsb && curr_poc_lsb - dpb->prevPocLsb > dpb->maxPocLsb / 2) {
            pocMsb = dpb->prevPocMsb - dpb->maxPocLsb;
        } else {
            pocMsb = dpb->prevPocMsb;
        }
        pic->top_field_order_cnt = pocMsb + curr_poc_lsb;
        pic->poc = pic->top_field_order_cnt;

    } else if (pic->sh->sps->poc_type == 1) {
        /* virtually unused, because it's dense and very fragile
         * most encoders found that the bits saved werent worth the complexity
         * in fact, many hardware decoders have had bugs specifically in their type 1 implementation,
         * just because there is so little content out there to test with
         */

        /* if this isn't implemented yet, refer to the reasons above to understand why :) */

        printf("poc type 1 not implemented\n");
        exit(67);

    } else if (pic->sh->sps->poc_type == 2) {
        /* commonly used (specific use cases)
         * it's extremely efficient because it requires almost no extra bits in the slice header,
         * but it cannot support B frames
         * so if the stream is strictly I-P-P-P, type 2 is the way to go
         */

        int prevFrameNum = dpb->prevPic == NULL ? 0 : dpb->prevPic->frame_num;
        int prevFrameNumOffset;
        if (!pic->is_idr) {
            prevFrameNumOffset = dpb->mmco_5_prev_occured ? 0 : dpb->prevPic->frame_num_offset;
        }
        if (pic->is_idr) {
            pic->frame_num_offset = 0;
        } else if (prevFrameNum > pic->frame_num) {
            pic->frame_num_offset = prevFrameNumOffset + 1 << (pic->sh->sps->log2_max_frame_num_minus4 + 4);
        } else {
            pic->frame_num_offset = prevFrameNumOffset;
        }

        int tmpPoc;
        if (pic->is_idr) {
            tmpPoc = 0;
        } else if (pic->nal_ref_idc == 0) {
            tmpPoc = 2 * (pic->frame_num_offset + pic->frame_num) - 1;
        } else {
            tmpPoc = 2 * (pic->frame_num_offset + pic->frame_num);
        }

        if (!pic->sh->field_pic_flag) {
            pic->top_field_order_cnt    = tmpPoc;
            pic->bottom_field_order_cnt = tmpPoc;
            pic->poc                    = tmpPoc;
        } else if (pic->sh->bottom_field_flag) {
            pic->bottom_field_order_cnt = tmpPoc;
        } else {
            pic->top_field_order_cnt    = tmpPoc;
        }
    }
}

/* index is the slot index in the DPB that gets freed by the bumping process */
void bump(DPB *dpb, int *index) {
    for (int i = 0; i < dpb->size; i++) {
        if (dpb->slots[i] == NULL) {
            *index = i;
            return;
        }
    }
    while (dpb->fullness == dpb->size) {
        int min_poc = INT_MAX;
        int i = 0;
        for (int j = 0; j < dpb->size; j++) {
            if (dpb->slots[j] != NULL && dpb->slots[j]->poc < min_poc && !dpb->slots[j]->is_output) {
                min_poc = dpb->slots[j]->poc;
                i = j;
            }
        }

        Picture *pic = dpb->slots[i];

        dump_picture(pic, dpb->ctx);
        pic->is_output = true;
        if (pic->dpb_status == UNUSED_FOR_REF) {
            picture_free(pic); // FIXME reuse buffers
            dpb->fullness--;
            *index = i;
        }
    }
}

void store_picture(DPB *dpb, Picture *pic) {
    derive_poc(dpb, pic);

    if (pic->is_idr) {
        dpb_flush(dpb);

        dpb->l0[0] = pic;
        if (!pic->long_term_ref) {
            dpb->l0[0]->dpb_status = USED_SHORT_TERM_REF;
        } else {
            dpb->l0[0]->dpb_status = USED_LONG_TERM_REF;
        }
    } else {
        if (!pic->adaptive_ref_pic_marking_mode) {

        } else {

        }
    }

    int index = 0;
    bump(dpb, &index);

    dpb->slots[index] = pic;
    dpb->prevPic = pic;
    dpb->fullness++;
}


void init_ref_pic_lists(DPB *dpb, SliceHeader *sh) {
    dpb_empty_ref_lists(dpb);

    for (int i = 0; i < dpb->size; i++) {
        Picture *pic = dpb->slots[i];
        if (pic != NULL && pic->dpb_status == USED_SHORT_TERM_REF) {
            if (pic->frame_num > sh->frame_num)
                pic->pic_num = pic->frame_num - dpb->ctx->maxFrameNum;
            else
                pic->pic_num = pic->frame_num;
        } else if (pic != NULL && pic->dpb_status == USED_LONG_TERM_REF) {
            pic->pic_num = pic->long_term_frame_idx;
        }
    }

    if (IS_P_SLICE(sh->slice_type) || IS_SP_SLICE(sh->slice_type)) {
        /* short-term ref frames first, sorted by descending PicNum
         * long-term ref frames second, sorted by ascending PicNum
         */

        // FIXME maybe use faster and smarter sorting using pre-computed max picNums in DPB

        int idx = 0;
        int maxPicNum = INT_MIN;
        int maxIdx = 0;
        int prevMax = INT_MAX;
        for (int i = 0; i < dpb->size; i++) {
            for (int j = 0; j < dpb->size; j++) {
                Picture *pic = dpb->slots[j];
                if (pic != NULL && pic->dpb_status == USED_SHORT_TERM_REF &&
                    pic->pic_num < prevMax && pic->pic_num > maxPicNum) {
                    maxPicNum = pic->pic_num;
                    maxIdx = j;
                }
            }
            if (maxPicNum > INT_MIN) {
                dpb->l0[idx++] = dpb->slots[maxIdx];
                prevMax = maxPicNum;
                maxPicNum = INT_MIN;
            } else break; // no more short term pics to use
        }

        int minPicNum = INT_MAX;
        int minIdx = 0;
        int prevMin = INT_MIN;
        for (int i = 0; i < dpb->size; i++) {
            for (int j = 0; j < dpb->size; j++) {
                Picture *pic = dpb->slots[j];
                if (pic != NULL && pic->dpb_status == USED_LONG_TERM_REF &&
                    pic->pic_num > prevMin && pic->pic_num < minPicNum) {
                    minPicNum = pic->pic_num;
                    minIdx = j;
                }
            }
            if (minPicNum < INT_MAX) {
                dpb->l0[idx++] = dpb->slots[minIdx];
                prevMin = minPicNum;
                minPicNum = INT_MAX;
            } else break; // no more long term pics to use
        }
    }
}



void ref_pic_list_modification(DPB *dpb, uint8_t type, Slice *slice, int maxFrameNum, int *maxLtIdx, BitReader *br) {
    int refIdxL0 = 0;
    if (type%5 != 2 && type%5 != 4) {
        printf("REF MODIF FOR P/B SLICE\n");
        int l0_modif_flag = read_u(br, 1);
        if (l0_modif_flag) {
            uint32_t modif_idc = 0;
            do {
                modif_idc = read_ue(br);
                printf("modif_idc:%d\n", modif_idc);
                if (modif_idc == 0 || modif_idc == 1) {
                    uint32_t abs_diff = read_ue(br) + 1;
                    ref_pic_list_modif_st(dpb, slice, true, &refIdxL0, modif_idc, abs_diff, maxFrameNum);
                } else if (modif_idc == 2) {
                    uint32_t lt_pic_num = read_ue(br);
                    ref_pic_list_modif_lt(dpb, slice, true, &refIdxL0, modif_idc, lt_pic_num, maxLtIdx);
                }
            } while (modif_idc != 3);
        } else {
            printf("no modification\n");
        }
    }

    int refIdxL1 = 0;
    if (type%5 == 1) {
        int l1_modif_flag = read_u(br, 1);
        if (l1_modif_flag) {
            uint32_t modif_idc = 0;
            do {
                modif_idc = read_ue(br);
                if (modif_idc == 0 || modif_idc == 1) {
                    uint32_t abs_diff = read_ue(br) + 1;
                    ref_pic_list_modif_st(dpb, slice, false, &refIdxL1, modif_idc, abs_diff, maxFrameNum);
                } else if (modif_idc == 2) {
                    uint32_t lt_pic_num = read_ue(br);
                    ref_pic_list_modif_lt(dpb, slice, false, &refIdxL1, modif_idc, lt_pic_num, maxLtIdx);
                }
            } while (modif_idc != 3);
        }
    }
}

void ref_pic_list_modif_st(DPB *dpb, Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int abs_diff, int maxFrameNum) {
    int picNumLXPred = is_l0 ? slice->picNumL0Pred : slice->picNumL1Pred;
    int num_ref_idx_lX_active = is_l0
        ? slice->sh->num_ref_idx_l0_active_minus1 + 1
        : slice->sh->num_ref_idx_l1_active_minus1 + 1;
    Picture **lX = is_l0 ? dpb->l0 : dpb->l1;

    int picNumLXNoWrap = 0;
    if (modif_idc == 0) {
        picNumLXNoWrap = abs_diff < 0
            ? picNumLXPred - abs_diff + maxFrameNum
            : picNumLXPred - abs_diff;
    } else {
        picNumLXNoWrap = abs_diff >= maxFrameNum
            ? picNumLXPred + abs_diff - maxFrameNum
            : picNumLXPred + abs_diff;
    }

    int picNumLX = picNumLXNoWrap > slice->sh->frame_num
        ? picNumLXNoWrap - maxFrameNum
        : picNumLXNoWrap;

    Picture *refpic;
    for (int cIdx = num_ref_idx_lX_active; cIdx > *refIdxLX; cIdx++) {
        if (lX[cIdx]->frame_num == picNumLX) refpic = lX[cIdx];
        lX[cIdx] = lX[cIdx-1];
    }
    lX[(*refIdxLX)++] = refpic;
    int nIdx = *refIdxLX;
    for (int cIdx = *refIdxLX; cIdx <= num_ref_idx_lX_active; cIdx++) {
        if (picNum(dpb, lX, cIdx, maxFrameNum) != picNumLX) {
            lX[nIdx++] = lX[cIdx];
        }
    }
}


void ref_pic_list_modif_lt(DPB *dpb, Slice *slice, bool is_l0,  int *refIdxLX, int modif_idc, int lt_pic_num, int *maxLtIdx) {
    int picNumLXPred = is_l0 ? slice->picNumL0Pred : slice->picNumL1Pred;
    int num_ref_idx_lX_active = is_l0
        ? slice->sh->num_ref_idx_l0_active_minus1 + 1
        : slice->sh->num_ref_idx_l1_active_minus1 + 1;
    Picture **lX = is_l0 ? dpb->l0 : dpb->l1;

    Picture *refpic;
    for (int cIdx = num_ref_idx_lX_active; cIdx > *refIdxLX; cIdx--) {
        if (lX[cIdx]->long_term_frame_idx == lt_pic_num) refpic = lX[cIdx];
        lX[cIdx] = lX[cIdx-1];
    }
    lX[(*refIdxLX)++] = refpic;
    int nIdx = *refIdxLX;
    for (int cIdx = *refIdxLX; cIdx <= num_ref_idx_lX_active; cIdx++) {
        if (ltPicNum(dpb, lX, cIdx, *maxLtIdx) != lt_pic_num) {
            lX[nIdx++] = lX[cIdx];
        }
    }
}


/* 7.3.3.3 */
void dec_ref_pic_marking(DPB *dpb, Slice *slice, BitReader *br) {

    if (slice->sh->idr_pic_flag) {

        slice->sh->no_output_of_prior_pics_flag  = read_u(br, 1);
        slice->sh->long_term_reference_flag      = read_u(br, 1);

    } else {
        int adaptive_ref_pic_marking_mode_flag = read_u(br, 1);
        if (adaptive_ref_pic_marking_mode_flag) {
            uint32_t mmco = 1;
            do {
                printf("mmco:%d\n", mmco);
                mmco = read_ue(br);
                if (mmco == 1 ||
                    mmco == 3) {
                    uint32_t difference_of_pics_nums_minus1 = read_ue(br);
                    }
                if (mmco == 2) {
                    uint32_t long_term_pic_num = read_ue(br);
                }
                if (mmco == 3 ||
                    mmco == 6) {
                    uint32_t max_long_term_frame_idx_plus1 = read_ue(br);
                    }
            } while (mmco != 0);
        } else {
            printf("no adaptive ref pic marking\n");
        }
    }
}

void dpb_empty_slots(DPB *dpb) {
    for (int i = 0; i < dpb->size; i++) {
        if (dpb->slots[i] != NULL) {
            picture_free(dpb->slots[i]);
            free(dpb->slots[i]);
        }
        dpb->l0[i] = NULL;
        dpb->l1[i] = NULL;
    }
    dpb->l0[dpb->size-1] = NULL;
    dpb->l1[dpb->size-1] = NULL;
}
void dpb_empty_ref_lists(DPB *dpb) {
    for (int i = 0; i < dpb->size + 1; i++) {
        dpb->l0[i] = NULL;
        dpb->l1[i] = NULL;
    }
}

void dpb_flush(DPB *dpb) {
    while (dpb->fullness != 0) {
        int min_poc = INT_MAX;
        for (int i = 0; i < dpb->size; i++) {
            if (dpb->slots[i] != NULL && dpb->slots[i]->poc < min_poc && !dpb->slots[i]->is_output) {
                dump_picture(dpb->slots[i], dpb->ctx);
                picture_free(dpb->slots[i]);
                dpb->slots[i] = NULL;
                dpb->fullness--;
            }
        }
    }
}

void dpb_free(DPB *dpb) {
    for (int i = 0; i < 16; i++) {
        if (dpb->l0[i] != NULL) {
            if (dpb->l0[i] != NULL) {
                picture_free(dpb->l0[i]);
                free(dpb->l0[i]);
            }

        }
        if (dpb->l1[i] != NULL) {
            if (dpb->l1[i] != NULL) {
                picture_free(dpb->l1[i]);
                free(dpb->l1[i]);
            }
        }
    }
    free(dpb);
}
