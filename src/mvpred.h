//
// Created by gmathix on 5/1/26.
//

#ifndef TOY_H264_MVPRED_H
#define TOY_H264_MVPRED_H


#include "global.h"
#include "mb.h"
#include "motion_info.h"
#include "picture.h"
#include "dpb.h"
#include "util/mbutil.h"


static ALWAYS_INLINE MotionVector get_median_mv(Macroblock *mb, int refIdx, int idx_for_abd, int idx_for_c, int list, Undo264Context *ctx) {
    MotionVector mv = {refIdx, 0, 0};

    Picture *currPic = ctx->curr_pic;

    MotionInfo (*motion_info) [16] = currPic->motion_info;

    Neighbor a = derive_a_neighbor_4x4(mb, idx_for_abd, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, idx_for_abd, ctx);
    Neighbor c = derive_c_neighbor_4x4(mb, idx_for_c, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, idx_for_abd, ctx);


    if (!c.av) {
        c = d;
    }

    if (!c.av && !b.av && a.av) {
        b = a;
        c = a;
    }

    MotionVector mv_a = {-1, 0, 0};
    MotionVector mv_b = {-1, 0, 0};
    MotionVector mv_c = {-1, 0, 0};


    if (a.av && !IS_INTRA(currPic->mb_types[mb->mbAddr+a.mb_off])) mv_a = motion_info[mb->mbAddr + a.mb_off][a.idx].mvs[list];
    if (b.av && !IS_INTRA(currPic->mb_types[mb->mbAddr+b.mb_off])) mv_b = motion_info[mb->mbAddr + b.mb_off][b.idx].mvs[list];
    if (c.av && !IS_INTRA(currPic->mb_types[mb->mbAddr+c.mb_off])) mv_c = motion_info[mb->mbAddr + c.mb_off][c.idx].mvs[list];

    int a_match = a.av && (mv_a.ref_idx == refIdx);
    int b_match = b.av && (mv_b.ref_idx == refIdx);
    int c_match = c.av && (mv_c.ref_idx == refIdx);
    if (a_match + b_match + c_match == 1) {
        if      (a_match) { mv.x = mv_a.x; mv.y = mv_a.y; }
        else if (b_match) { mv.x = mv_b.x; mv.y = mv_b.y; }
        else if (c_match) { mv.x = mv_c.x; mv.y = mv_c.y; }
    } else {
        mv.x = _median(mv_a.x, mv_b.x, mv_c.x);
        mv.y = _median(mv_a.y, mv_b.y, mv_c.y);
    }


    return mv;
}

static ALWAYS_INLINE void derive_p_skip_mv(Macroblock *mb, Undo264Context *ctx) {
    MotionVector mv = {0, 0, 0};

    Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, 0, ctx);

    if (a.av && b.av) {
        MotionVector mv_a = ctx->curr_pic->motion_info[mb->mbAddr + a.mb_off][a.idx].mvs[L0];
        MotionVector mv_b = ctx->curr_pic->motion_info[mb->mbAddr + b.mb_off][b.idx].mvs[L0];
        bool a_is_zero = (mv_a.ref_idx == 0 && mv_a.x == 0 && mv_a.y == 0);
        bool b_is_zero = (mv_b.ref_idx == 0 && mv_b.x == 0 && mv_b.y == 0);
        if (!a_is_zero && !b_is_zero) {
            mv = get_median_mv(mb, 0, 0, 3, L0, ctx);
        }
    }

    // broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[L0] = mv;
        ctx->curr_pic->motion_info[mb->mbAddr][i].ref_pics[L0] = ctx->dpb->lists[L0][1+mv.ref_idx];
    }
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L0][0], true, 4 * sizeof(bool));
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][L1][0], false, 4 * sizeof(bool));
}

static ALWAYS_INLINE void derive_16x16_mv(Macroblock *mb, int list, Undo264Context *ctx) {
    MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx[list][0], 0, 3, list, ctx);

    // add delta and broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->curr_pic->motion_info[mb->mbAddr][i].mvs[list] = (MotionVector) {mv.ref_idx,
                                                            (int16_t) (mv.x + mb->u.pb.mvd[list][0][0][0]),
                                                            (int16_t) (mv.y + mb->u.pb.mvd[list][0][0][1])};
        ctx->curr_pic->motion_info[mb->mbAddr][i].ref_pics[list] = ctx->dpb->lists[list][1+mv.ref_idx];
    }
    memset(&ctx->curr_pic->pred_flags[mb->mbAddr][list][0], true, 4 * sizeof(bool));
}

static ALWAYS_INLINE void derive_16x8_part_mv(Macroblock *mb, int partIdx, int list, Undo264Context *ctx) {
    MotionInfo (*motion_info) [16] = ctx->curr_pic->motion_info;

    MotionVector mv1 = {mb->u.pb.ref_idx[list][0], 0, 0};
    MotionVector mv2 = {mb->u.pb.ref_idx[list][1], 0, 0};

    Neighbors n1 = derive_neighbors_4x4(mb, 0, ctx); // top neighbor of first partition
    Neighbors n2 = derive_neighbors_4x4(mb, 8, ctx); // left neighbor of second partition

    if (partIdx == 0) {
        if (n1.b.av && motion_info[mb->mbAddr + n1.b.mb_off][n1.b.idx].mvs[list].ref_idx == mv1.ref_idx) {
            MotionVector mvB = motion_info[mb->mbAddr + n1.b.mb_off][n1.b.idx].mvs[list];
            mv1.x = mvB.x;
            mv1.y = mvB.y;
        } else {
            mv1 = get_median_mv(mb, mv1.ref_idx, 0, 3, list, ctx);
        }
        for (int i = 0; i < 8; i++) {
            motion_info[mb->mbAddr][i].mvs[list] = (MotionVector) {mv1.ref_idx,
                                                (int16_t) (mv1.x + mb->u.pb.mvd[list][0][0][0]),
                                                (int16_t) (mv1.y + mb->u.pb.mvd[list][0][0][1])};
            motion_info[mb->mbAddr][i].ref_pics[list] = ctx->dpb->lists[list][1+mv1.ref_idx];
        }
        ctx->curr_pic->pred_flags[mb->mbAddr][list][0] = true;
        ctx->curr_pic->pred_flags[mb->mbAddr][list][1] = true;
    }
    else {
        if (n2.a.av && motion_info[mb->mbAddr + n2.a.mb_off][n2.a.idx].mvs[list].ref_idx == mv2.ref_idx) {
            MotionVector mvA = motion_info[mb->mbAddr + n2.a.mb_off][n2.a.idx].mvs[list];
            mv2.x = mvA.x;
            mv2.y = mvA.y;
        } else {
            mv2 = get_median_mv(mb, mv2.ref_idx, 8, 11, list, ctx);
        }
        for (int i = 8; i < 16; i++) {
            motion_info[mb->mbAddr][i].mvs[list] = (MotionVector) {mv2.ref_idx,
                                                (int16_t) (mv2.x + mb->u.pb.mvd[list][1][0][0]),
                                                (int16_t) (mv2.y + mb->u.pb.mvd[list][1][0][1])};
            motion_info[mb->mbAddr][i].ref_pics[list] = ctx->dpb->lists[list][1+mv2.ref_idx];
        }
        ctx->curr_pic->pred_flags[mb->mbAddr][list][2] = true;
        ctx->curr_pic->pred_flags[mb->mbAddr][list][3] = true;
    }
}

static ALWAYS_INLINE void derive_8x16_part_mv(Macroblock *mb, int partIdx, int list, Undo264Context *ctx) {
    MotionInfo (*motion_info) [16] = ctx->curr_pic->motion_info;


    MotionVector mv1 = {mb->u.pb.ref_idx[list][0], 0, 0};
    MotionVector mv2 = {mb->u.pb.ref_idx[list][1], 0, 0};

	Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
	Neighbor c = derive_c_neighbor_4x4(mb, 3, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, 2, ctx); /* let's cheat a little here, as we need the top-right macroblock
                                                             and using blkIdx=2 would have given us the bottom-right 4x4 block
                                                             of the macroblock above as the C neighbor */

    if (!c.av) {
        c = d;
    }


    if (partIdx == 0) {
        if (a.av && motion_info[mb->mbAddr + a.mb_off][a.idx].mvs[list].ref_idx == mv1.ref_idx) {
            MotionVector mvA = motion_info[mb->mbAddr + a.mb_off][a.idx].mvs[list];
            mv1.x = mvA.x;
            mv1.y = mvA.y;
        } else {
            mv1 = get_median_mv(mb, mv1.ref_idx, 0, 1, list, ctx);
        }
        for (int i = 0; i < 8; i++) {
            int pos = ((i>>1)<<2) + (i&1);
            motion_info[mb->mbAddr][pos].mvs[list] = (MotionVector) {mv1.ref_idx,
                                                      (int16_t) (mv1.x + mb->u.pb.mvd[list][0][0][0]),
                                                      (int16_t) (mv1.y + mb->u.pb.mvd[list][0][0][1])};
            motion_info[mb->mbAddr][pos].ref_pics[list] = ctx->dpb->lists[list][1+mv1.ref_idx];
        }
        ctx->curr_pic->pred_flags[mb->mbAddr][list][0] = true;
        ctx->curr_pic->pred_flags[mb->mbAddr][list][2] = true;
    }
    else {
        if (c.av && motion_info[mb->mbAddr + c.mb_off][c.idx].mvs[list].ref_idx == mv2.ref_idx) {
            MotionVector mvC = motion_info[mb->mbAddr + c.mb_off][c.idx].mvs[list];
            mv2.x = mvC.x;
            mv2.y = mvC.y;
        } else {
            mv2 = get_median_mv(mb, mv2.ref_idx, 2, 3, list, ctx);
        }
        for (int i = 0; i < 8; i++) {
            int pos = ((i>>1)<<2) + 2 + (i&1);
            motion_info[mb->mbAddr][pos].mvs[list] = (MotionVector) {mv2.ref_idx,
                                                  (int16_t) (mv2.x + mb->u.pb.mvd[list][1][0][0]),
                                                  (int16_t) (mv2.y + mb->u.pb.mvd[list][1][0][1])};
            motion_info[mb->mbAddr][pos].ref_pics[list] = ctx->dpb->lists[list][1+mv2.ref_idx];
        }
        ctx->curr_pic->pred_flags[mb->mbAddr][list][1] = true;
        ctx->curr_pic->pred_flags[mb->mbAddr][list][3] = true;
    }
}

static ALWAYS_INLINE void derive_sub_8x8_mv(Macroblock *mb, int partIdx, int list, Undo264Context *ctx) {
    MotionInfo (*motion_info) [16] = ctx->curr_pic->motion_info;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;
    MotionVector mv = get_median_mv(
        mb, mb->u.pb.ref_idx[list][partIdx],
        part_4x4_idx,
         part_4x4_idx + 1, list, ctx);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            int pos = part_4x4_idx + i*4 + j;
            motion_info[mb->mbAddr][pos].mvs[list] = (MotionVector) {mv.ref_idx,
                                                      (int16_t) (mv.x + mb->u.pb.mvd[list][partIdx][0][0]),
                                                      (int16_t) (mv.y + mb->u.pb.mvd[list][partIdx][0][1])};
            motion_info[mb->mbAddr][pos].ref_pics[list] = ctx->dpb->lists[list][1+mv.ref_idx];
        }
    }
}

static ALWAYS_INLINE void derive_sub_8x4_mv(Macroblock *mb, int partIdx, int list, Undo264Context *ctx) {
    MotionInfo (*motion_info) [16] = ctx->curr_pic->motion_info;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 2; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart*4;
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx[list][partIdx], sub_part_idx, sub_part_idx+1, list, ctx);
        for (int i = 0; i < 2; i++) {
            int pos = sub_part_idx + i;
            motion_info[mb->mbAddr][pos].mvs[list] = (MotionVector) {mv.ref_idx,
                                                      (int16_t) (mv.x + mb->u.pb.mvd[list][partIdx][subPart][0]),
                                                      (int16_t) (mv.y + mb->u.pb.mvd[list][partIdx][subPart][1])};
            motion_info[mb->mbAddr][pos].ref_pics[list] = ctx->dpb->lists[list][1+mv.ref_idx];
        }
    }
}

static ALWAYS_INLINE void derive_sub_4x8_mv(Macroblock *mb, int partIdx, int list, Undo264Context *ctx) {
    MotionInfo (*motion_info) [16] = ctx->curr_pic->motion_info;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 2; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart;
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx[list][partIdx], sub_part_idx, sub_part_idx, list, ctx);
        for (int i = 0; i < 2; i++) {
            int pos = sub_part_idx + i*4;
            motion_info[mb->mbAddr][pos].mvs[list] = (MotionVector) {mv.ref_idx,
                                                      (int16_t) (mv.x + mb->u.pb.mvd[list][partIdx][subPart][0]),
                                                      (int16_t) (mv.y + mb->u.pb.mvd[list][partIdx][subPart][1])};
            motion_info[mb->mbAddr][pos].ref_pics[list] = ctx->dpb->lists[list][1+mv.ref_idx];
        }
    }
}

static ALWAYS_INLINE void derive_sub_4x4_mv(Macroblock *mb, int partIdx, int list, Undo264Context *ctx) {
    MotionInfo (*motion_info) [16] = ctx->curr_pic->motion_info;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 4; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart/2*4 + subPart%2;
        MotionVector mv = get_median_mv(mb, mb->u.pb.ref_idx[list][partIdx], sub_part_idx, sub_part_idx, list, ctx);
        motion_info[mb->mbAddr][sub_part_idx].mvs[list] = (MotionVector) {mv.ref_idx,
                                                          (int16_t) (mv.x + mb->u.pb.mvd[list][partIdx][subPart][0]),
                                                          (int16_t) (mv.y + mb->u.pb.mvd[list][partIdx][subPart][1])};
        motion_info[mb->mbAddr][sub_part_idx].ref_pics[list] = ctx->dpb->lists[list][1+mv.ref_idx];
    }
}




/* this decoder does not support MBAFF and field pictures and is not intended to,
 * so this greatly simplifies the colocated motion vector derivation as the colocated picture has identical
 * geometry.
 */
static ALWAYS_INLINE MotionInfo get_colocated_mv(Macroblock *mb, int partIdx, int subPartIdx, int *list, Undo264Context *ctx) {
    Picture *currPic = ctx->curr_pic;
    Picture *refPic  = ctx->dpb->lists[L1][1+0];

    int luma4x4Idx = ctx->curr_pic->sh->sps->direct_8x8_inference_flag
        ? 5 * partIdx
        : 4 * partIdx + subPartIdx;
    int blkIdx = map_4x4[luma4x4Idx];



    int mbAddrCol = mb->mbAddr;
    int partIdxCol = partIdx;
    if (!IS_INTRA(refPic->mb_types[mbAddrCol])) {
        if (refPic->pred_flags[mbAddrCol][L0][partIdxCol]) {
            *list = L0;
            return refPic->motion_info[mbAddrCol][blkIdx];
        } else {
            *list = L1;
            return refPic->motion_info[mbAddrCol][blkIdx];
        }
    } else {
        *list = L0;
        MotionVector empty = (MotionVector) {-1, 0, 0};
        return (MotionInfo) {{empty, empty}, {&EMPTY_PICTURE, &EMPTY_PICTURE}};
    }
}

static ALWAYS_INLINE void derive_spatial_direct_mv(Macroblock *mb, int partIdx, Undo264Context *ctx) {
    Picture *currPic = ctx->curr_pic;

    MotionVector mvL0 = {0, 0, 0};
    MotionVector mvL1 = {0, 0, 0};

    Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, 0, ctx);
    Neighbor c = derive_c_neighbor_4x4(mb, 3, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, 0, ctx);

    if (!c.av) {
        c = d;
    }

    MotionVector mvL0A = {-1, 0, 0};
    MotionVector mvL0B = {-1, 0, 0};
    MotionVector mvL0C = {-1, 0, 0};
    MotionVector mvL1A = {-1, 0, 0};
    MotionVector mvL1B = {-1, 0, 0};
    MotionVector mvL1C = {-1, 0, 0};

    if (a.av && !IS_INTRA(currPic->mb_types[mb->mbAddr + a.mb_off])) {
        if (currPic->pred_flags[mb->mbAddr+a.mb_off][L0][map_4x4[a.idx] / 4] == 1) {
            mvL0A = currPic->motion_info[mb->mbAddr + a.mb_off][a.idx].mvs[L0];
        }
        if (currPic->pred_flags[mb->mbAddr+a.mb_off][L1][map_4x4[a.idx] / 4] == 1) {
            mvL1A = currPic->motion_info[mb->mbAddr + a.mb_off][a.idx].mvs[L1];
        }
    }
    if (b.av && !IS_INTRA(currPic->mb_types[mb->mbAddr + b.mb_off])) {
        if (currPic->pred_flags[mb->mbAddr+b.mb_off][L0][map_4x4[b.idx] / 4] == 1) {
            mvL0B = currPic->motion_info[mb->mbAddr + b.mb_off][b.idx].mvs[L0];
        }
        if (currPic->pred_flags[mb->mbAddr+b.mb_off][L1][map_4x4[b.idx] / 4] == 1) {
            mvL1B = currPic->motion_info[mb->mbAddr + b.mb_off][b.idx].mvs[L1];
        }
    }
    if (c.av && !IS_INTRA(currPic->mb_types[mb->mbAddr + c.mb_off])) {
        if (currPic->pred_flags[mb->mbAddr+c.mb_off][L0][map_4x4[c.idx] / 4] == 1) {
            mvL0C = currPic->motion_info[mb->mbAddr + c.mb_off][c.idx].mvs[L0];
        }
        if (currPic->pred_flags[mb->mbAddr+c.mb_off][L1][map_4x4[c.idx] / 4] == 1) {
            mvL1C = currPic->motion_info[mb->mbAddr + c.mb_off][c.idx].mvs[L1];
        }
    }


    int refIdxL0 = _minPositive(mvL0A.ref_idx, _minPositive(mvL0B.ref_idx, mvL0C.ref_idx));
    int refIdxL1 = _minPositive(mvL1A.ref_idx, _minPositive(mvL1B.ref_idx, mvL1C.ref_idx));
    bool directZeroPred = false;

    if (refIdxL0 < 0 && refIdxL1 < 0) {
        refIdxL0 = 0;
        refIdxL1 = 0;
        directZeroPred = true;
    }

    for (int subPart = 0; subPart < 4; subPart++) {
        int list;
        MotionInfo colocated = get_colocated_mv(mb, partIdx, subPart, &list, ctx);
        MotionVector mvCol = colocated.mvs[list];
        mvL0 = (MotionVector) {0, 0, 0};
        mvL1 = (MotionVector) {0, 0, 0};

        bool colZero = (ctx->dpb->lists[L1][1+0]->dpb_status == SHORT_TERM_REF) &&
                       (mvCol.ref_idx == 0) &&
                       (mvCol.x >= -1 && mvCol.x <= 1) &&
                       (mvCol.y >= -1 && mvCol.y <= 1);



        if (!directZeroPred) {
            if (!(refIdxL0 < 0 || (refIdxL0 == 0 && colZero))) {
                mvL0 = get_median_mv(mb, refIdxL0, 0, 3, 0, ctx);
            }
            if (!(refIdxL1 < 0 || (refIdxL1 == 0 && colZero))) {
                mvL1 = get_median_mv(mb, refIdxL1, 0, 3, 1, ctx);
            }
        }
        mvL0.ref_idx = (int8_t) refIdxL0;
        mvL1.ref_idx = (int8_t) refIdxL1;

        bool predFlagL0 = refIdxL0 >= 0;
        bool predFlagL1 = refIdxL1 >= 0;


        currPic->pred_flags[mb->mbAddr][L0][partIdx] = predFlagL0;
        currPic->pred_flags[mb->mbAddr][L1][partIdx] = predFlagL1;
        int pos = map_4x4[partIdx * 4 + subPart];
        currPic->motion_info[mb->mbAddr][pos].mvs[L0] = mvL0;
        currPic->motion_info[mb->mbAddr][pos].mvs[L1] = mvL1;
        currPic->motion_info[mb->mbAddr][pos].ref_pics[L0] = ctx->dpb->lists[L0][1+mvL0.ref_idx];
        currPic->motion_info[mb->mbAddr][pos].ref_pics[L1] = ctx->dpb->lists[L1][1+mvL1.ref_idx];
    }

}

static ALWAYS_INLINE void derive_temporal_direct_mv(Macroblock *mb, int partIdx, Undo264Context *ctx) {
    Picture *currPic = ctx->curr_pic;

    MotionVector mvL0 = {0, 0, 0};
    MotionVector mvL1 = {0, 0, 0};

    /// FIXME when direct_8x8_inference_flag is 1, there is actually just one MV per 8x8 partition so the subpart loop would be useless
    for (int subPart = 0; subPart < 4; subPart++) {
        int list;
        MotionInfo colocated = get_colocated_mv(mb, partIdx, subPart, &list, ctx);
        MotionVector mvCol = colocated.mvs[list];

        int8_t refIdxL0 = mvCol.ref_idx < 0 ? 0 : colocated.ref_pics[list]->lowest_list_index[list];
        int8_t refIdxL1 = 0;

        Picture *pic0 = ctx->dpb->lists[L0][1+refIdxL0];
        Picture *pic1 = ctx->dpb->lists[L1][1+refIdxL1];

        if (pic0->dpb_status == LONG_TERM_REF || (pic1->poc - pic0->poc == 0)) {
            mvL0 = (MotionVector) {refIdxL0, mvCol.x, mvCol.y};
            mvL1 = (MotionVector) {refIdxL1, 0, 0};
        } else {
            int td = _clip3(-128, 127, pic1->poc - pic0->poc);
            int tb = _clip3(-128, 127, currPic->poc - pic0->poc);
            int tx = (16384 + _abs(td / 2)) / td;
            int distScaleFactor = _clip3(-1024, 1023, (tb * tx + 32) >> 6);
            mvL0 = (MotionVector) {refIdxL0, (int16_t) ((distScaleFactor * mvCol.x + 128) >> 8),
                                             (int16_t) ((distScaleFactor * mvCol.y + 128) >> 8)};
            mvL1 = (MotionVector) {refIdxL1, (int16_t) (mvL0.x - mvCol.x),
                                             (int16_t) (mvL0.y - mvCol.y)};
        }


        int pos = map_4x4[partIdx * 4 + subPart];
        currPic->motion_info[mb->mbAddr][pos].mvs[L0] = mvL0;
        currPic->motion_info[mb->mbAddr][pos].mvs[L1] = mvL1;
        currPic->motion_info[mb->mbAddr][pos].ref_pics[L0] = ctx->dpb->lists[L0][1+mvL0.ref_idx];
        currPic->motion_info[mb->mbAddr][pos].ref_pics[L1] = ctx->dpb->lists[L1][1+mvL1.ref_idx];
    }

    currPic->pred_flags[mb->mbAddr][L0][partIdx] = 1;
    currPic->pred_flags[mb->mbAddr][L1][partIdx] = 1;
}

static ALWAYS_INLINE void derive_direct_mv(Macroblock *mb, int partIdx, Undo264Context *ctx) {
    if (ctx->current_slice->sh->direct_spatial_mv_pred_flag) {
        derive_spatial_direct_mv(mb, partIdx, ctx);
    } else {
        derive_temporal_direct_mv(mb, partIdx, ctx);
    }
}



static ALWAYS_INLINE void derive_8x8_mv(Macroblock *mb, Undo264Context *ctx) {
    for (int part = 0; part < 4; part++) {
        int subType = mb->u.pb.sub_mb_info[part].type;

        ctx->curr_pic->pred_flags[mb->mbAddr][L0][part] = (subType & MB_TYPE_P0L0) > 0;
        ctx->curr_pic->pred_flags[mb->mbAddr][L1][part] = (subType & MB_TYPE_P0L1) > 0;

        if (subType & SUB_MB_TYPE_DIRECT) {
            derive_direct_mv(mb, part, ctx); // will override pred flags as needed
        } else if (subType & SUB_MB_TYPE_8x8) {
            if (subType & MB_TYPE_P0L0) derive_sub_8x8_mv(mb, part, L0, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_8x8_mv(mb, part, L1, ctx);
        } else if (subType & SUB_MB_TYPE_8x4) {
            if (subType & MB_TYPE_P0L0) derive_sub_8x4_mv(mb, part, L0, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_8x4_mv(mb, part, L1, ctx);
        } else if (subType & SUB_MB_TYPE_4x8) {
            if (subType & MB_TYPE_P0L0) derive_sub_4x8_mv(mb, part, L0, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_4x8_mv(mb, part, L1, ctx);
        } else if (subType & SUB_MB_TYPE_4x4) {
            if (subType & MB_TYPE_P0L0) derive_sub_4x4_mv(mb, part, L0, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_4x4_mv(mb, part, L1, ctx);
        }
    }
}


// whew! that was sweaty code to write! now time for my beer.



#endif //TOY_H264_MVPRED_H