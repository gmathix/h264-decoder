//
// Created by gmathix on 5/1/26.
//

#ifndef TOY_H264_MVPRED_H
#define TOY_H264_MVPRED_H


#include <mmintrin.h>

#include "global.h"
#include "mb.h"
#include "mv.h"



static ALWAYS_INLINE MotionVector get_median_mv(Macroblock *mb, int refIdx, int idx_for_abd, int idx_for_c, bool l0, CodecContext *ctx) {
    MotionVector mv = {refIdx, 0, 0};

    Picture *currPic = ctx->curr_pic;
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;

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

    if (a.av && !IS_INTRA(currPic->mb_types[mb->mbAddr+a.mb_off])) mv_a = mvList[mb->mbAddr + a.mb_off][a.idx];
    if (b.av && !IS_INTRA(currPic->mb_types[mb->mbAddr+b.mb_off])) mv_b = mvList[mb->mbAddr + b.mb_off][b.idx];
    if (c.av && !IS_INTRA(currPic->mb_types[mb->mbAddr+c.mb_off])) mv_c = mvList[mb->mbAddr + c.mb_off][c.idx];

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

static ALWAYS_INLINE void derive_p_skip_mv(Macroblock *mb, CodecContext *ctx) {
    MotionVector mv = {0, 0, 0};

    Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, 0, ctx);

    if (a.av && b.av) {
        MotionVector mv_a = ctx->curr_pic->mvs_l0[mb->mbAddr + a.mb_off][a.idx];
        MotionVector mv_b = ctx->curr_pic->mvs_l0[mb->mbAddr + b.mb_off][b.idx];
        bool a_is_zero = (mv_a.ref_idx == 0 && mv_a.x == 0 && mv_a.y == 0);
        bool b_is_zero = (mv_b.ref_idx == 0 && mv_b.x == 0 && mv_b.y == 0);
        if (!a_is_zero && !b_is_zero) {
            mv = get_median_mv(mb, 0, 0, 3, true, ctx);
        }
    }

    // broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        ctx->curr_pic->mvs_l0[mb->mbAddr][i] = mv;
    }
    memset(&ctx->curr_pic->pred_flag_l0[mb->mbAddr][0], 1, 4);
    memset(&ctx->curr_pic->pred_flag_l1[mb->mbAddr][0], 0, 4);
}

static ALWAYS_INLINE void derive_16x16_mv(Macroblock *mb, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;
    bool (*predFlagList)[4]     = l0 ? ctx->curr_pic->pred_flag_l0 : ctx->curr_pic->pred_flag_l1;

    MotionVector mv = get_median_mv(mb, refIdxList[0], 0, 3, l0, ctx);

    // add delta and broadcast the MV through the whole 4x4 MV block
    for (int i = 0; i < 16; i++) {
        mvList[mb->mbAddr][i] = (MotionVector) {mv.ref_idx,
                                            (int16_t) (mv.x + mvdList[0][0][0]),
                                            (int16_t) (mv.y + mvdList[0][0][1])};
    }
    memset(&predFlagList[mb->mbAddr][0], 1, 4);
}

static ALWAYS_INLINE void derive_16x8_part_mv(Macroblock *mb, int partIdx, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;
    bool (*predFlagList)[4]     = l0 ? ctx->curr_pic->pred_flag_l0 : ctx->curr_pic->pred_flag_l1;

    MotionVector mv1 = {refIdxList[0], 0, 0};
    MotionVector mv2 = {refIdxList[1], 0, 0};

    Neighbors n1 = derive_neighbors_4x4(mb, 0, ctx); // top neighbor of first partition
    Neighbors n2 = derive_neighbors_4x4(mb, 8, ctx); // left neighbor of second partition


    if (partIdx == 0) {
        if (n1.b.av && mvList[mb->mbAddr + n1.b.mb_off][n1.b.idx].ref_idx == mv1.ref_idx) {
            MotionVector mvB = mvList[mb->mbAddr + n1.b.mb_off][n1.b.idx];
            mv1.x = mvB.x;
            mv1.y = mvB.y;
        } else {
            mv1 = get_median_mv(mb, mv1.ref_idx, 0, 3, l0, ctx);
        }
        for (int i = 0; i < 8; i++) {
            mvList[mb->mbAddr][i] = (MotionVector) {mv1.ref_idx,
                                                (int16_t) (mv1.x + mvdList[0][0][0]),
                                                (int16_t) (mv1.y + mvdList[0][0][1])};
        }
        memset(&predFlagList[mb->mbAddr][0], 1, 2);
    }
    else {
        if (n2.a.av && mvList[mb->mbAddr + n2.a.mb_off][n2.a.idx].ref_idx == mv2.ref_idx) {
            MotionVector mvA = mvList[mb->mbAddr + n2.a.mb_off][n2.a.idx];
            mv2.x = mvA.x;
            mv2.y = mvA.y;
        } else {
            mv2 = get_median_mv(mb, mv2.ref_idx, 8, 11, l0, ctx);
        }
        for (int i = 8; i < 16; i++) {
            mvList[mb->mbAddr][i] = (MotionVector) {mv2.ref_idx,
                                                (int16_t) (mv2.x + mvdList[1][0][0]),
                                                (int16_t) (mv2.y + mvdList[1][0][1])};
        }
        memset(&predFlagList[mb->mbAddr][2], 1, 2);
    }
}

static ALWAYS_INLINE void derive_8x16_part_mv(Macroblock *mb, int partIdx, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;
    bool (*predFlagList)[4]     = l0 ? ctx->curr_pic->pred_flag_l0 : ctx->curr_pic->pred_flag_l1;


    MotionVector mv1 = {refIdxList[0], 0, 0};
    MotionVector mv2 = {refIdxList[1], 0, 0};

	Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
	Neighbor c = derive_c_neighbor_4x4(mb, 3, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, 2, ctx); /* let's cheat a little here, as we need the top-right macroblock
                                                             and using blkIdx=2 would have given us the bottom-right 4x4 block
                                                             of the macroblock above as the C neighbor */

    if (!c.av) {
        c = d;
    }


    if (partIdx == 0) {
        if (a.av && mvList[mb->mbAddr + a.mb_off][a.idx].ref_idx == mv1.ref_idx) {
            MotionVector mvA = mvList[mb->mbAddr + a.mb_off][a.idx];
            mv1.x = mvA.x;
            mv1.y = mvA.y;
        } else {
            mv1 = get_median_mv(mb, mv1.ref_idx, 0, 1, l0, ctx);
        }
        for (int i = 0; i < 8; i++) {
            int pos = ((i>>1)<<2) + (i&1);
            mvList[mb->mbAddr][pos] = (MotionVector) {mv1.ref_idx,
                                                  (int16_t) (mv1.x + mvdList[0][0][0]),
                                                  (int16_t) (mv1.y + mvdList[0][0][1])};
        }
        predFlagList[mb->mbAddr][0] = 1;
        predFlagList[mb->mbAddr][2] = 1;
    }
    else {
        if (c.av && mvList[mb->mbAddr + c.mb_off][c.idx].ref_idx == mv2.ref_idx) {
            MotionVector mvC = mvList[mb->mbAddr + c.mb_off][c.idx];
            mv2.x = mvC.x;
            mv2.y = mvC.y;
        } else {
            mv2 = get_median_mv(mb, mv2.ref_idx, 2, 3, l0, ctx);
        }
        for (int i = 0; i < 8; i++) {
            int pos = ((i>>1)<<2) + 2 + (i&1);
            mvList[mb->mbAddr][pos] = (MotionVector) {mv2.ref_idx,
                                                  (int16_t) (mv2.x + mvdList[1][0][0]),
                                                  (int16_t) (mv2.y + mvdList[1][0][1])};
        }
        predFlagList[mb->mbAddr][1] = 1;
        predFlagList[mb->mbAddr][3] = 1;
    }


}

static ALWAYS_INLINE void derive_sub_8x8_mv(Macroblock *mb, int partIdx, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;
    MotionVector mv = get_median_mv(
        mb, refIdxList[partIdx],
        part_4x4_idx,
         part_4x4_idx + 1, l0, ctx);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            mvList[mb->mbAddr][part_4x4_idx + i*4 + j] =
                (MotionVector) {mv.ref_idx,
                            (int16_t) (mv.x + mvdList[partIdx][0][0]),
                            (int16_t) (mv.y + mvdList[partIdx][0][1])};
        }
    }
}

static ALWAYS_INLINE void derive_sub_8x4_mv(Macroblock *mb, int partIdx, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 2; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart*4;
        MotionVector mv = get_median_mv(mb, refIdxList[partIdx], sub_part_idx, sub_part_idx+1, l0, ctx);
        for (int i = 0; i < 2; i++) {
            mvList[mb->mbAddr][sub_part_idx + i] =
                (MotionVector) {mv.ref_idx,
                            (int16_t) (mv.x + mvdList[partIdx][subPart][0]),
                            (int16_t) (mv.y + mvdList[partIdx][subPart][1])};
        }
    }
}

static ALWAYS_INLINE void derive_sub_4x8_mv(Macroblock *mb, int partIdx, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 2; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart;
        MotionVector mv = get_median_mv(mb, refIdxList[partIdx], sub_part_idx, sub_part_idx, l0, ctx);
        for (int i = 0; i < 2; i++) {
            mvList[mb->mbAddr][sub_part_idx + i*4] =
                (MotionVector) {mv.ref_idx,
                            (int16_t) (mv.x + mvdList[partIdx][subPart][0]),
                            (int16_t) (mv.y + mvdList[partIdx][subPart][1])};
        }
    }
}

static ALWAYS_INLINE void derive_sub_4x4_mv(Macroblock *mb, int partIdx, bool l0, CodecContext *ctx) {
    MotionVector (*mvList) [16] = l0 ? ctx->curr_pic->mvs_l0 : ctx->curr_pic->mvs_l1;
    int16_t (*mvdList)[4][2]    = l0 ? mb->u.pb.mvd_l0 : mb->u.pb.mvd_l1;
    uint8_t *refIdxList         = l0 ? mb->u.pb.ref_idx_l0 : mb->u.pb.ref_idx_l1;

    int part_4x4_idx = partIdx/2*8 + (partIdx%2)*2;

    for (int subPart = 0; subPart < 4; subPart++) {
        int sub_part_idx = part_4x4_idx + subPart/2*4 + subPart%2;
        MotionVector mv = get_median_mv(mb, refIdxList[partIdx], sub_part_idx, sub_part_idx, l0, ctx);
        mvList[mb->mbAddr][sub_part_idx] =
                (MotionVector) {mv.ref_idx,
                            (int16_t) (mv.x + mvdList[partIdx][subPart][0]),
                            (int16_t) (mv.y + mvdList[partIdx][subPart][1])};
    }
}




/* this decoder does not support MBAFF and field pictures and is not intended to,
 * so this greatly simplifies the colocated motion vector derivation as the colocated picture has identical
 * geometry.
 */
static ALWAYS_INLINE MotionVector get_colocated_mv(Macroblock *mb, int partIdx, int subPartIdx, CodecContext *ctx) {
    Picture *currPic = ctx->curr_pic;
    Picture *refPic  = ctx->dpb->l1[0];

    int luma4x4Idx = ctx->curr_pic->sh->sps->direct_8x8_inference_flag
        ? 5 * partIdx
        : 4 * partIdx + subPartIdx;
    int blkIdx = map_4x4[luma4x4Idx];



    int mbAddrCol = mb->mbAddr;
    int partIdxCol = partIdx;
    if (!IS_INTRA(refPic->mb_types[mbAddrCol])) {
        if (refPic->pred_flag_l0[mbAddrCol][partIdxCol]) {
            return refPic->mvs_l0[mbAddrCol][blkIdx];
        } else {
            return refPic->mvs_l1[mbAddrCol][blkIdx];
        }
    } else {
        return (MotionVector) {-1, 0, 0};
    }
}

static ALWAYS_INLINE void derive_spatial_direct_mv(Macroblock *mb, int partIdx, bool part8x8, CodecContext *ctx) {
    if (debugging) {
        printf("debugging\n");
    }
    Picture *currPic = ctx->curr_pic;

    MotionVector mvL0 = {0, 0, 0};
    MotionVector mvL1 = {0, 0, 0};

    Neighbor a = derive_a_neighbor_4x4(mb, 0, ctx);
    Neighbor b = derive_b_neighbor_4x4(mb, 0, ctx);
    Neighbor c = derive_c_neighbor_4x4(mb, 3, ctx);
    Neighbor d = derive_d_neighbor_4x4(mb, 0, ctx);

    if (debugging) {
        printf("a type : %s\n", mb_type_to_string(currPic->mb_types[mb->mbAddr + a.mb_off]));
        printf("b type : %s\n", mb_type_to_string(currPic->mb_types[mb->mbAddr + b.mb_off]));
        printf("c type : %s\n", mb_type_to_string(currPic->mb_types[mb->mbAddr + c.mb_off]));
    }

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
        if (currPic->pred_flag_l0[mb->mbAddr+a.mb_off][map_4x4[a.idx] / 4] == 1) {
            mvL0A = currPic->mvs_l0[mb->mbAddr + a.mb_off][a.idx];
        }
        if (currPic->pred_flag_l1[mb->mbAddr+a.mb_off][map_4x4[a.idx] / 4] == 1) {
            mvL1A = currPic->mvs_l1[mb->mbAddr + a.mb_off][a.idx];
        }
    }
    if (b.av && !IS_INTRA(currPic->mb_types[mb->mbAddr + b.mb_off])) {
        if (currPic->pred_flag_l0[mb->mbAddr+b.mb_off][map_4x4[b.idx] / 4] == 1) {
            mvL0B = currPic->mvs_l0[mb->mbAddr + b.mb_off][b.idx];
        }
        if (currPic->pred_flag_l1[mb->mbAddr+b.mb_off][map_4x4[b.idx] / 4] == 1) {
            mvL1B = currPic->mvs_l1[mb->mbAddr + b.mb_off][b.idx];
        }
    }
    if (c.av && !IS_INTRA(currPic->mb_types[mb->mbAddr + c.mb_off])) {
        if (currPic->pred_flag_l0[mb->mbAddr+c.mb_off][map_4x4[c.idx] / 4] == 1) {
            mvL0C = currPic->mvs_l0[mb->mbAddr + c.mb_off][c.idx];
        }
        if (currPic->pred_flag_l1[mb->mbAddr+c.mb_off][map_4x4[c.idx] / 4] == 1) {
            mvL1C = currPic->mvs_l1[mb->mbAddr + c.mb_off][c.idx];
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

    MotionVector mvCol = get_colocated_mv(mb, partIdx, 0, ctx);

    bool colZero = (ctx->dpb->l1[0]->dpb_status == SHORT_TERM_REF) &&
                   (mvCol.ref_idx == 0) &&
                   (mvCol.x >= -1 && mvCol.x <= 1) &&
                   (mvCol.y >= -1 && mvCol.y <= 1);



    if (!directZeroPred) {
        if (!(refIdxL0 < 0 || (refIdxL0 == 0 && colZero))) {
            mvL0 = get_median_mv(mb, refIdxL0, 0, 3, true, ctx);
        }
        if (!(refIdxL1 < 0 || (refIdxL1 == 0 && colZero))) {
            mvL1 = get_median_mv(mb, refIdxL1, 0, 3, false, ctx);
        }
    }
    mvL0.ref_idx = (int8_t) refIdxL0;
    mvL1.ref_idx = (int8_t) refIdxL1;

    bool predFlagL0 = refIdxL0 >= 0;
    bool predFlagL1 = refIdxL1 >= 0;


    if (part8x8) {
        currPic->pred_flag_l0[mb->mbAddr][partIdx] = predFlagL0;
        currPic->pred_flag_l1[mb->mbAddr][partIdx] = predFlagL1;
        for (int i = 0; i < 4; i++) {
            currPic->mvs_l0[mb->mbAddr][map_4x4[partIdx * 4 + i]] = mvL0;
            currPic->mvs_l1[mb->mbAddr][map_4x4[partIdx * 4 + i]] = mvL1;
        }
    } else {
        memset(&currPic->pred_flag_l0[mb->mbAddr][0], predFlagL0, 4);
        memset(&currPic->pred_flag_l1[mb->mbAddr][0], predFlagL1, 4);
        for (int i = 0; i < 16; i++) {
            memcpy(&currPic->mvs_l0[mb->mbAddr][i], &mvL0, sizeof(MotionVector));
            memcpy(&currPic->mvs_l1[mb->mbAddr][i], &mvL1, sizeof(MotionVector));
            // currPic->mvs_l0[mb->mbAddr][i] = mvL0;
            // currPic->mvs_l1[mb->mbAddr][i] = mvL1;
        }
    }
}

static ALWAYS_INLINE void derive_temporal_direct_mv(Macroblock *mb, int partIdx, bool part8x8, CodecContext *ctx) {
    Picture *currPic = ctx->curr_pic;

    MotionVector mvL0 = {0, 0, 0};
    MotionVector mvL1 = {0, 0, 0};


    int8_t refIdxL0 = 0;
    int8_t refIdxL1 = 0;

    Picture *pic0 = ctx->dpb->l0[0];
    Picture *pic1 = ctx->dpb->l1[0];


    // part8x8 = 1 : do once for the current 8x8 partition
    // part8x8 = 0 : do for the whole macroblock (4 partitions)
    // if this was not clear, email at smegmuscarlsen@gmail.com

    for (int part = 0; part < 4; part++) {
        if (part8x8 && part != partIdx) continue;

        /// FIXME when direct_8x8_inference_flag is 1, there is actually just one MV per 8x8 partition so the subpart loop would be useless
        for (int subPart = 0; subPart < 4; subPart++) {

            MotionVector mvCol = get_colocated_mv(mb, partIdx, subPart, ctx);

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

            currPic->pred_flag_l0[mb->mbAddr][partIdx] = 1;
            currPic->pred_flag_l1[mb->mbAddr][partIdx] = 1;

            currPic->mvs_l0[mb->mbAddr][map_4x4[partIdx * 4 + subPart]] = mvL0;
            currPic->mvs_l1[mb->mbAddr][map_4x4[partIdx * 4 + subPart]] = mvL1;
        }
    }
}

static ALWAYS_INLINE void derive_direct_mv(Macroblock *mb, int partIdx, bool part8x8, CodecContext *ctx) {
    if (ctx->current_slice->sh->direct_spatial_mv_pred_flag) {
        derive_spatial_direct_mv(mb, partIdx, part8x8, ctx);
    } else {
        derive_temporal_direct_mv(mb, partIdx, part8x8, ctx);
    }
}



static ALWAYS_INLINE void derive_8x8_mv(Macroblock *mb, CodecContext *ctx) {
    for (int part = 0; part < 4; part++) {
        int subType = mb->u.pb.sub_mb_info[part].type;

        ctx->curr_pic->pred_flag_l0[mb->mbAddr][part] = (subType & MB_TYPE_P0L0) > 0;
        ctx->curr_pic->pred_flag_l1[mb->mbAddr][part] = (subType & MB_TYPE_P0L1) > 0;

        if (subType & SUB_MB_TYPE_DIRECT) {
            derive_direct_mv(mb, part, true, ctx); // will override pred flags as needed
        } else if (subType & SUB_MB_TYPE_8x8) {
            if (subType & MB_TYPE_P0L0) derive_sub_8x8_mv(mb, part, true, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_8x8_mv(mb, part, false, ctx);
        } else if (subType & SUB_MB_TYPE_8x4) {
            if (subType & MB_TYPE_P0L0) derive_sub_8x4_mv(mb, part, true, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_8x4_mv(mb, part, false, ctx);
        } else if (subType & SUB_MB_TYPE_4x8) {
            if (subType & MB_TYPE_P0L0) derive_sub_4x8_mv(mb, part, true, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_4x8_mv(mb, part, false, ctx);
        } else if (subType & SUB_MB_TYPE_4x4) {
            if (subType & MB_TYPE_P0L0) derive_sub_4x4_mv(mb, part, true, ctx);
            if (subType & MB_TYPE_P0L1) derive_sub_4x4_mv(mb, part, false, ctx);
        }
    }
}


// whew! that was sweaty code to write! now time for my beer.



#endif //TOY_H264_MVPRED_H