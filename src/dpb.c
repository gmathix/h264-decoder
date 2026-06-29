//
// Created by gmathix on 4/21/26.
//

#include "dpb.h"
#include <stdint.h>

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
            pic->frame_num_offset = prevFrameNumOffset + (1 << (pic->sh->sps->log2_max_frame_num_minus4 + 4));
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

void decode_pic_nums(DPB *dpb, SliceHeader *sh) {
    for (int i = 0; i < dpb->size; i++) {
        Picture *pic = dpb->slots[i];
        if (pic != NULL) {
            if (pic->dpb_status == USED_SHORT_TERM_REF) {
                if (pic->frame_num > sh->frame_num)
                    pic->frame_num_wrap = pic->frame_num - dpb->ctx->maxFrameNum;
                else
                    pic->frame_num_wrap = pic->frame_num;
                pic->pic_num = pic->frame_num_wrap;
            } else if (pic->dpb_status == USED_LONG_TERM_REF) {
                pic->pic_num = pic->long_term_frame_idx;
            }
        }
    }
}

int output_oldest_pic(DPB *dpb) {
    int min_poc = INT32_MAX;
    int min_idx = -1;
    for (int i = 0; i < dpb->size; i++) {
        Picture *pic = dpb->slots[i];
        if (pic != NULL && pic->poc < min_poc && !pic->is_output) {
            min_poc = pic->poc;
            min_idx = i;
        }
    }
    if (min_idx != -1) {
        Picture *old_pic = dpb->slots[min_idx];

        dump_picture(old_pic, dpb->ctx);
        old_pic->is_output = true;
        picture_free(old_pic);
        dpb->slots[min_idx] = NULL;
        dpb->fullness--;
    }

    return min_idx;
}

/* index is the slot index in the DPB that gets freed by the bumping process */
int bump(DPB *dpb) {
    for (int i = 0; i < dpb->size; i++) {
        if (dpb->slots[i] == NULL) {
            return i;
        }
    }
    while (dpb->fullness == dpb->size) {
        return output_oldest_pic(dpb);
    }
    return 0;
}



void store_picture(DPB *dpb, Picture *pic) {
    derive_poc(dpb, pic);
    decode_pic_nums(dpb, pic->sh);

    if (pic->is_idr) {
        for (int i = 0; i < dpb->size; i++) {
            output_oldest_pic(dpb);
        }
        dpb->fullness = 0;
        if (pic->sh->long_term_reference_flag) {
            pic->dpb_status = USED_LONG_TERM_REF;
            pic->long_term_frame_idx = 0;
            dpb->ctx->maxLongTermFrameIdx = 0;
        } else {
            pic->dpb_status = USED_SHORT_TERM_REF;
        }
    } else {
        if (!pic->sh->adaptive_ref_pic_marking_mode_flag) {
            int numShortTerm = 0;
            int numLongTerm = 0;
            for (int i = 0; i < dpb->size; i++) {
                if (dpb->slots[i] != NULL && dpb->slots[i]->dpb_status == USED_SHORT_TERM_REF) {
                    numShortTerm++;
                } else if (dpb->slots[i] != NULL && dpb->slots[i]->dpb_status == USED_LONG_TERM_REF) {
                    numLongTerm++;
                }
            }
            if (numShortTerm + numLongTerm == _max(pic->sh->sps->max_num_ref_frames, 1)) {
                int minFrameNumWrap = INT32_MAX;
                int minIdx = 0;
                for (int i = 0; i < dpb->size; i++) {
                    Picture *pic = dpb->slots[i];
                    if (pic != NULL && pic->dpb_status == USED_SHORT_TERM_REF && pic->frame_num_wrap < minFrameNumWrap) {
                        minFrameNumWrap = pic->frame_num_wrap;
                        minIdx = i;
                    }
                }
                dpb->slots[minIdx]->dpb_status = UNUSED_FOR_REF;
            }
        } else {
            // MMCOs
        }
    }

    if (pic->dpb_status != USED_LONG_TERM_REF) {
        pic->dpb_status = USED_SHORT_TERM_REF;
    }

    int index = bump(dpb);

    dpb->slots[index] = pic;
    dpb->prevPic = pic;
    dpb->fullness++;
}


void init_ref_pic_lists(DPB *dpb, SliceHeader *sh) {
    dpb_empty_ref_lists(dpb);

    decode_pic_nums(dpb, sh);

    Picture *curr_pic = dpb->ctx->current_pic;

    if (IS_P_SLICE(sh->slice_type) || IS_SP_SLICE(sh->slice_type)) {
        /* short-term ref frames first, sorted by descending PicNum
         * long-term ref frames second, sorted by ascending PicNum
         */

        // FIXME maybe use faster and smarter sorting using pre-computed max picNums in DPB

        // int idx = 0;
        //
        // int maxPicNum = MIN_PIC_NUM;
        // int maxIdx = 0;
        // int prevMax = MAX_PIC_NUM;
        // for (int i = 0; i < dpb->size; i++) {
        //     for (int j = 0; j < dpb->size; j++) {
        //         Picture *pic = dpb->slots[j];
        //         if (pic != NULL && pic->dpb_status == USED_SHORT_TERM_REF &&
        //             pic->pic_num < prevMax && pic->pic_num > maxPicNum) {
        //             maxPicNum = pic->pic_num;
        //             maxIdx = j;
        //         }
        //     }
        //     if (maxPicNum > MIN_PIC_NUM) {
        //         dpb->l0[idx++] = dpb->slots[maxIdx];
        //         dpb->effective_ref_idx_l0_active++;
        //         prevMax = maxPicNum;
        //         maxPicNum = MIN_PIC_NUM;
        //     } else break; // no more short term pics to use
        // }
        //
        // int minPicNum = MAX_PIC_NUM;
        // int minIdx = 0;
        // int prevMin = MIN_PIC_NUM;
        // for (int i = 0; i < dpb->size; i++) {
        //     for (int j = 0; j < dpb->size; j++) {
        //         Picture *pic = dpb->slots[j];
        //         if (pic != NULL && pic->dpb_status == USED_LONG_TERM_REF &&
        //             pic->pic_num > prevMin && pic->pic_num < minPicNum) {
        //             minPicNum = pic->pic_num;
        //             minIdx = j;
        //         }
        //     }
        //     if (minPicNum < MAX_PIC_NUM) {
        //         dpb->l0[idx++] = dpb->slots[minIdx];
        //         dpb->effective_ref_idx_l0_active++;
        //         prevMin = minPicNum;
        //         minPicNum = MAX_PIC_NUM;
        //     } else break; // no more long term pics to use
        // }

        int idx = 0;
        int nbAdded = 0;
        nbAdded += sortToRefList(dpb, true,  dpb->l0, &idx,
            returnPicNum,
            shortTermCriteria,
            dontCare,
            curr_pic);
        nbAdded += sortToRefList(dpb, false, dpb->l0,
            &idx, returnPicNum,
            longTermCriteria,
            dontCare,
            curr_pic);

        dpb->effective_ref_idx_l0_active += nbAdded;

    } else if (IS_B_SLICE(sh->slice_type)) {

        int idx = 0;

        int nbAdded = 0;
        nbAdded += sortToRefList(dpb, true,  dpb->l0, &idx,
            returnPoc,
            shortTermCriteria,
            lowerThan,
            curr_pic);
        nbAdded += sortToRefList(dpb, false, dpb->l0, &idx,
            returnPoc,
            shortTermCriteria,
            greaterOrEqual,
            curr_pic);
        nbAdded += sortToRefList(dpb, false, dpb->l0, &idx,
            returnLTPicNum,
            longTermCriteria,
            dontCare,
            curr_pic);

        dpb->effective_ref_idx_l0_active += nbAdded;


        idx = 0;
        nbAdded = 0;
        nbAdded += sortToRefList(dpb, false, dpb->l1, &idx,
            returnPoc,
            shortTermCriteria,
            greaterThan,
            curr_pic);
        nbAdded += sortToRefList(dpb, true, dpb->l1, &idx,
            returnPoc,
            shortTermCriteria,
            lowerOrEqual,
            curr_pic);
        nbAdded += sortToRefList(dpb, false, dpb->l1, &idx,
            returnLTPicNum,
            longTermCriteria,
            dontCare,
            curr_pic);

        dpb->effective_ref_idx_l1_active += nbAdded;


        /* check if l0 == l1 */
        bool equal = true;
        for (int i = 0; i < dpb->size; i++) {
            if (dpb->l0[i] != dpb->l1[i]) {
                equal = false;
                break;
            }
        }
        if (equal) {
            Picture *tmp = dpb->l1[0];
            dpb->l1[0] = dpb->l1[1];
            dpb->l1[1] = tmp;
        }
    }
}



void ref_pic_list_modification(uint8_t type, Slice *slice, int maxFrameNum, int *maxLtIdx, CodecContext *ctx) {
    BitReader *br = ctx->br;

    int refIdxL0 = 0;
    if (type%5 != 2 && type%5 != 4) {
        printf("  * l0 modifications\n");
        int l0_modif_flag = read_u(br, 1);
        if (l0_modif_flag) {
            uint32_t modif_idc = 0;
            do {
                modif_idc = read_ue(br);
                printf("   modif_idc:%d\n", modif_idc);
                if (modif_idc == 0 || modif_idc == 1) {
                    uint32_t abs_diff = read_ue(br) + 1;
                    ref_pic_list_modif_st(slice, true, &refIdxL0, modif_idc, abs_diff, maxFrameNum, ctx);
                } else if (modif_idc == 2) {
                    uint32_t lt_pic_num = read_ue(br);
                    ref_pic_list_modif_lt(slice, true, &refIdxL0, modif_idc, lt_pic_num, maxLtIdx, ctx);
                }
            } while (modif_idc != 3);
        } else {
            printf("    no modification\n");
        }
    }

    int refIdxL1 = 0;
    if (type%5 == 1) {
        printf("  * l1 modifications\n");
        int l1_modif_flag = read_u(br, 1);
        if (l1_modif_flag) {
            uint32_t modif_idc = 0;
            do {
                modif_idc = read_ue(br);
                if (modif_idc == 0 || modif_idc == 1) {
                    uint32_t abs_diff = read_ue(br) + 1;
                    ref_pic_list_modif_st(slice, false, &refIdxL1, modif_idc, abs_diff, maxFrameNum, ctx);
                } else if (modif_idc == 2) {
                    uint32_t lt_pic_num = read_ue(br);
                    ref_pic_list_modif_lt(slice, false, &refIdxL1, modif_idc, lt_pic_num, maxLtIdx, ctx);
                }
            } while (modif_idc != 3);
        } else {
            printf("    no modifications\n");
        }
    }
}

void ref_pic_list_modif_st(Slice *slice, bool is_l0, int *refIdxLX, int modif_idc, int abs_diff, int maxFrameNum, CodecContext *ctx) {
    DPB *dpb = ctx->dpb;

    int picNumLXPred = ctx->current_pic->pic_num;
    int num_ref_idx_lX_active = is_l0 ? dpb->effective_ref_idx_l0_active : dpb->effective_ref_idx_l1_active;
    Picture **lX = is_l0 ? dpb->l0 : dpb->l1;

    int picNumLXNoWrap = 0;
    if (modif_idc == 0) {
        picNumLXNoWrap = (picNumLXPred - abs_diff) < 0
            ? picNumLXPred - abs_diff + maxFrameNum
            : picNumLXPred - abs_diff;
    } else {
        picNumLXNoWrap = (picNumLXPred + abs_diff) >= maxFrameNum
            ? picNumLXPred + abs_diff - maxFrameNum
            : picNumLXPred + abs_diff;
    }

    int picNumLX = picNumLXNoWrap > ctx->current_pic->pic_num
        ? picNumLXNoWrap - maxFrameNum
        : picNumLXNoWrap;

    Picture *refpic;
    for (int cIdx = num_ref_idx_lX_active-1; cIdx > *refIdxLX; cIdx--) {
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


void ref_pic_list_modif_lt(Slice *slice, bool is_l0,  int *refIdxLX, int modif_idc, int lt_pic_num, int *maxLtIdx, CodecContext *ctx) {
    DPB *dpb = ctx->dpb;

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
        slice->sh->adaptive_ref_pic_marking_mode_flag = read_u(br, 1);
        if (slice->sh->adaptive_ref_pic_marking_mode_flag) {
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
    dpb->effective_ref_idx_l0_active = 0;
    dpb->effective_ref_idx_l1_active = 0;
}


void dpb_flush(DPB *dpb) {
	while (dpb->fullness != 0) {
		output_oldest_pic(dpb);
	}
}

void dpb_free(DPB *dpb) {
    for (int i = 0; i < dpb->size; i++) {
        if (dpb->slots[i] != NULL) {
            picture_free(dpb->slots[i]);
        }
    }
    free(dpb);
}