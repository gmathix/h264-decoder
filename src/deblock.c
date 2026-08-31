//
// Created by gmathix on 5/12/26.
//



#include "deblock.h"


#include "dpb.h"
#include "mb.h"
#include "motion_info.h"


#include "util/mbutil.h"


const uint8_t alpha_table[52] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   4,   4,   5,   6,   7,   8,   9,  10,  12,  13,
     15,  17,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
     71,  80,  90, 101, 113, 127, 144, 162, 182, 203, 226, 255, 255,
};

const uint8_t beta_table[52] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,
     6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12,
    12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
};

const uint8_t treshold_table[3][52] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,
       1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,
       3,  4,  4,  4,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    },

    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
       1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  4,
       4,  5,  5,  6,  7,  8,  8, 10, 11, 12, 13, 15, 17,
    },

    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,
       1,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  6,
       6,  7,  8,  9, 10, 11, 13, 14, 16, 18, 20, 23, 25,
    },
};




static always_inline bool same_ref_pics(
    const Picture *picL0_0, const Picture *picL1_0,
    const Picture *picL0_1, const Picture *picL1_1) {

    // when refLX_X is -1 (no ref) we just get EMPTY_PIC which has dpb_pic_id = 0
    long hash0 = ((picL0_0->dpb_pic_id != 0) * (1L << picL0_0->dpb_pic_id)) |
                ((picL1_0->dpb_pic_id != 0)  * (1L << picL1_0->dpb_pic_id));
    long hash1 = ((picL0_1->dpb_pic_id != 0) * (1L << picL0_1->dpb_pic_id)) |
                ((picL1_1->dpb_pic_id != 0)  * (1L << picL1_1->dpb_pic_id));
    return hash0 == hash1;
}

static always_inline bool same_ref_pics_one_block(int refL0, int refL1, const Undo264Context *ctx) {
    return ctx->dpb->lists[L0][1+refL0]->dpb_pic_id == ctx->dpb->lists[L1][1+refL1]->dpb_pic_id;
}

static always_inline bool mv_diff_g4(MotionVector mv1, MotionVector mv2) {
    return (_abs(mv1.x - mv2.x) >= 4) || (_abs(mv1.y - mv2.y) >= 4);
}



/**
 * derive the 4 bS values needed for a vertical or horizontal edge
 * @param mbAddr     address if the macroblock to deblock
 * @param mbAddrN    address of the macroblock containing the sample p0 (neighbor of mbAddr)
 * @param blkIdx     initial 4x4 block index in current mb
 * @param blkIdxN    initial 4x4 block index in neighbor mb
 * @param blkIdx8x8  initial 8x8 block_index in current mb
 * @param blkIdx8x8N initial 8x8 block index in neighbor mb
 */
always_inline flatten void derive_low_bS_list(int mbAddr, int mbAddrN, int blkIdx, int blkIdxN, int blkIdx8x8, int blkIdx8x8N,
                        bool vertical, int bS_list[4], const Undo264Context *ctx) {

    MacroblockMetadata meta   = ctx->mb_metadata[mbAddr];
    MacroblockMetadata meta_n = ctx->mb_metadata[mbAddrN];


    int blkAdd    = 1 + vertical*3; // 1 for horizontal, 4 for vertical
    int blkAdd8x8 = 1 + vertical;

    for (int i = 0; i < 4; i++) {
        int idx       = blkIdx      + i*blkAdd;
        int idx_n     = blkIdxN     + i*blkAdd;
        int idx_8x8   = blkIdx8x8   + (i/2)*blkAdd8x8;
        int idx_n_8x8 = blkIdx8x8N  + (i/2)*blkAdd8x8;

        bS_list[i] = 0;

        /*
         * bS = 3
         * <=> either or both of mb and mb_n are intra macroblocks
         */
        bS_list[i] += ((IS_INTRA(meta.mb_type) || IS_INTRA(meta_n.mb_type))) * 3;
        if (bS_list[i] == 3) continue;

        /*
         * bS = 2
         * <=> the corresponding 4x4 or 8x8 transform blocks (depending on transform_8x8_flag) have non-zero coeff levels
         */
        bS_list[i] += (((meta.t_8x8_flag    && (meta.cbp_luma   & (1 << idx_8x8))) ||
             (meta_n.t_8x8_flag  && (meta_n.cbp_luma & (1 << idx_n_8x8))) ||
             (!meta.t_8x8_flag   && (ctx->total_coeffs[mbAddr][idx] > 0)) ||
             (!meta_n.t_8x8_flag && (ctx->total_coeffs[mbAddrN][idx_n] > 0)))) * 2;
        if (bS_list[i] == 2) continue;

        /* bS = 1
         * <=> too much writing, 8.7.2.1
         */
        const MotionVector mvL0_0 = ctx->curr_pic->motion_info[mbAddr][idx].mvs[L0];
        const MotionVector mvL1_0 = ctx->curr_pic->motion_info[mbAddr][idx].mvs[L1];
        const Picture *picL0_0    = ctx->curr_pic->motion_info[mbAddr][idx].ref_pics[L0];
        const Picture *picL1_0    = ctx->curr_pic->motion_info[mbAddr][idx].ref_pics[L1];

        const MotionVector mvL0_1 = ctx->curr_pic->motion_info[mbAddrN][idx_n].mvs[L0];
        const MotionVector mvL1_1 = ctx->curr_pic->motion_info[mbAddrN][idx_n].mvs[L1];
        const Picture *picL0_1    = ctx->curr_pic->motion_info[mbAddrN][idx_n].ref_pics[L0];
        const Picture *picL1_1    = ctx->curr_pic->motion_info[mbAddrN][idx_n].ref_pics[L1];

    	int flagL0_0 = ctx->curr_pic->pred_flags[mbAddr][L0][idx_8x8];
    	int flagL1_0 = ctx->curr_pic->pred_flags[mbAddr][L1][idx_8x8];
    	int flagL0_1 = ctx->curr_pic->pred_flags[mbAddrN][L0][idx_n_8x8];
    	int flagL1_1 = ctx->curr_pic->pred_flags[mbAddrN][L1][idx_n_8x8];
        int nbMV0    = flagL0_0 + flagL1_0;
        int nbMV1    = flagL0_1 + flagL1_1;
        MotionVector singleMV0 = flagL0_0 ? mvL0_0 : mvL1_0;
        MotionVector singleMV1 = flagL0_1 ? mvL0_1 : mvL1_1;

        bool same_pics = same_ref_pics(picL0_0, picL1_0, picL0_1, picL1_1);

        bS_list[i] += ((
        	    (!same_pics) ||
                (nbMV0 != nbMV1) || // different number of MVs
                ((nbMV0 == 1 && nbMV1 == 1) && mv_diff_g4(singleMV0, singleMV1)) || // one MV on each side and abs(mv0-mv1) >= 4 for x or y
                ((nbMV0 == 2 && nbMV1 == 2) && !same_ref_pics_one_block(mvL0_1.ref_idx, mvL1_1.ref_idx, ctx) &&
                     ((mvL0_0.ref_idx == mvL0_1.ref_idx && (mv_diff_g4(mvL0_0, mvL0_1) || mv_diff_g4(mvL1_0, mvL1_1))) ||
                      (mvL0_0.ref_idx != mvL0_1.ref_idx && (mv_diff_g4(mvL0_0, mvL1_1) || mv_diff_g4(mvL1_0, mvL0_1))))) ||
                ((nbMV0 == 2 && nbMV1 == 2) && same_ref_pics_one_block(mvL0_0.ref_idx, mvL1_0.ref_idx, ctx) &&
                     ((mv_diff_g4(mvL0_0, mvL0_1) || mv_diff_g4(mvL1_0, mvL1_1)) &&
                      (mv_diff_g4(mvL0_1, mvL1_0) || mv_diff_g4(mvL1_1, mvL0_0))))
            )) * 1;
    }
}


always_inline void derive_alpha_beta(Picture *pic, int mbAddr, int mbAddrN, uint8_t alpha[3], uint8_t beta[3], uint8_t indexA[3], const Undo264Context *ctx) {
    MacroblockMetadata *meta0 = &ctx->mb_metadata[mbAddr];
    MacroblockMetadata *meta1 = &ctx->mb_metadata[mbAddrN];

    // Y plane
    int qp0 = !IS_PCM(meta0->mb_type) * meta0->QPY;
    int qp1 = !IS_PCM(meta1->mb_type) * meta1->QPY;
    int qpAv = (qp0 + qp1 + 1) >> 1;

    indexA[0]  = _clip3(0, 51, qpAv + pic->sh->slice_alpha_c0_offset_div2 * 2);
    int indexB = _clip3(0, 51, qpAv + pic->sh->slice_beta_offset_div2 * 2);

    alpha[0] = alpha_table[indexA[0]]; // assume bitDepth = 8
    beta[0]  = beta_table[indexB];

    // Cb / Cr planes
    for (int i = 0; i < 2; i++) {
        qp0 = !IS_PCM(meta0->mb_type) * meta0->QPC[i];
        qp1 = !IS_PCM(meta1->mb_type) * meta1->QPC[i];
        qpAv = (qp0 + qp1 + 1) >> 1;

        indexA[1+i] = _clip3(0, 51, qpAv + pic->sh->slice_alpha_c0_offset_div2 * 2);
        indexB      = _clip3(0, 51, qpAv + pic->sh->slice_beta_offset_div2 * 2);

        alpha[1+i] = alpha_table[indexA[1+i]];
        beta[1+i]  = beta_table[indexB];
    }
}

always_inline flatten void deblock_edge_low_bs_luma(uint8_t *dst, int xstride, int ystride, int alpha, int beta, int indexA, int *bS) {

    uint8_t p0, p1, p2, q0, q1, q2;
    int aP, aQ;

    for (int edge = 0; edge < 4; edge++) {
        if (bS[edge] == 0) {
            dst += 4*xstride;
            continue;
        }
        int tc0 = treshold_table[bS[edge]-1][indexA];

        for (int i = 0; i < 4; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];  p2 = dst[-3*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];  q2 = dst[ 2*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                aP = ABS(p2 - p0);
                aQ = ABS(q2 - q0);

                int t = tc0 + ((aP < beta) + (aQ < beta));
                int delta = CLIP3(-t, t, (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

                if (aP < beta) {
                    /*p1*/ dst[-2*ystride] += CLIP3(-tc0, tc0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
                }
                if (aQ < beta) {
                    /*q1*/ dst[ 1*ystride] += CLIP3(-tc0, tc0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
                }
                /*p0*/     dst[-1*ystride] = CLIP3(0, MAX_U8, p0 + delta);
                /*q0*/     dst[ 0*ystride] = CLIP3(0, MAX_U8, q0 - delta);
            }

            dst += xstride;
        }
    }
}

always_inline flatten void deblock_edge_high_bs_luma(uint8_t *dst, int xstride, int ystride, int alpha, int beta) {

    uint8_t p0, p1, p2, p3, q0, q1, q2, q3;
    int aP, aQ;

    for (int edge = 0; edge < 4; edge++) {
        for (int i = 0; i < 4; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];  p2 = dst[-3*ystride];  p3 = dst[-4*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];  q2 = dst[ 2*ystride];  q3 = dst[ 3*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                aP = ABS(p2 - p0);
                aQ = ABS(q2 - q0);

                if (aP < beta && ABS(p0 - q0) < ((alpha >> 2) + 2)) {
                    /*p0*/ dst[-1*ystride] = (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3;
                    /*p1*/ dst[-2*ystride] = (p2 + p1 + p0 + q0 + 2) >> 2;
                    /*p2*/ dst[-3*ystride] = (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3;
                } else {
                    /*p0*/ dst[-1*ystride] = (2*p1 + p0 + q1 + 2) >> 2;
                }

                if (aQ < beta && ABS(p0 - q0) < ((alpha >> 2) + 2)) {
                    /*q0*/ dst[ 0*ystride] = (p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3;
                    /*q1*/ dst[ 1*ystride] = (p0 + q0 + q1 + q2 + 2) >> 2;
                    /*q2*/ dst[ 2*ystride] = (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3;
                } else {
                    /*q0*/ dst[ 0*ystride] = (2*q1 + q0 + p1 + 2) >> 2;
                }
            }

            dst += xstride;
        }
    }
}

always_inline flatten void deblock_edge_low_bs_chroma(uint8_t *dst, int xstride, int ystride, int alpha, int beta, int indexA, int *bS) {

    uint8_t p0, p1, q0, q1;

    for (int edge = 0; edge < 4; edge++) {
        if (bS[edge] == 0) {
            dst += 2*xstride;
            continue;
        }
        int tc0 = treshold_table[bS[edge]-1][indexA];

        for (int i = 0; i < 2; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                int t = tc0 + 1;
                int delta = CLIP3(-t, t, (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

                /*p0*/     dst[-1*ystride] = CLIP3(0, MAX_U8, p0 + delta);
                /*q0*/     dst[ 0*ystride] = CLIP3(0, MAX_U8, q0 - delta);
            }

            dst += xstride;
        }
    }
}

always_inline flatten void deblock_edge_high_bs_chroma(uint8_t *dst, int xstride, int ystride, int alpha, int beta) {

    uint8_t p0, p1, q0, q1;

    for (int edge = 0; edge < 4; edge++) {
        for (int i = 0; i < 2; i++) {
            p0 = dst[-1*ystride];  p1 = dst[-2*ystride];
            q0 = dst[ 0*ystride];  q1 = dst[ 1*ystride];

            if (ABS(p0 - q0) < alpha &&
                ABS(p1 - p0) < beta &&
                ABS(q1 - q0) < beta) {

                /*p0*/ dst[-1*ystride] = (2*p1 + p0 + q1 + 2) >> 2;
                /*q0*/ dst[ 0*ystride] = (2*q1 + q0 + p1 + 2) >> 2;
            }

            dst += xstride;
        }
    }
}


void deblock_macroblock(Picture *pic, SliceHeader *sh, int mbAddr, const Undo264Context *ctx) {

    SPS *sps = sh->sps;


    // make dummy mb just for accessing the neighbors afterward
    Macroblock *mb = make_mb(mbAddr, ctx);
    derive_macroblock_neighbors(mb, sh->first_mb, ctx);


    const int widthY     = pic->widthY;
    const int widthC     = pic->widthC;
    const int mbWidth    = sps->pic_width_in_mbs;

    const bool disableSliceBoundaries = sh->disable_deblocking_filter_idc == 2;
    const bool filterInternalEdges    = sh->disable_deblocking_filter_idc != 1;
    const bool filterLeftMbEdge       = filterInternalEdges && (mb->mbAddr % mbWidth != 0) && (!disableSliceBoundaries || mb->has_mb_a);
    const bool filterTopMbEdge        = filterInternalEdges && (mb->mbAddr >= mbWidth) && (!disableSliceBoundaries || mb->has_mb_b);


    const int luma_pos   = mb->mb_y*16*widthY + mb->mb_x*16;
    const int chroma_pos = mb->mb_y*8*widthC + mb->mb_x*8;
    uint8_t *luma_base_dst = &pic->luma[luma_pos];
    uint8_t *cb_base_dst   = &pic->cb[chroma_pos];
    uint8_t *cr_base_dst   = &pic->cr[chroma_pos];

    int bS_list[4];

    uint8_t alphaLeft[3], betaLeft[3], indexALeft[3];
    uint8_t alphaTop[3],  betaTop[3],  indexATop[3];
    uint8_t alphaIn[3],   betaIn[3],   indexAIn[3];

    bool mb8x8 = ctx->mb_metadata[mb->mbAddr].t_8x8_flag;

    if (filterLeftMbEdge) {
        // x = 0
        derive_alpha_beta(pic, mbAddr, mbAddr - 1, alphaLeft, betaLeft, indexALeft, ctx);

        if (IS_INTRA(ctx->mb_metadata[mbAddr-1].mb_type)) {
            deblock_edge_high_bs_luma(luma_base_dst, widthY, 1, alphaLeft[0], betaLeft[0]);
            deblock_edge_high_bs_chroma(cb_base_dst, widthC, 1, alphaLeft[1], betaLeft[1]);
            deblock_edge_high_bs_chroma(cr_base_dst, widthC, 1, alphaLeft[2], betaLeft[2]);
        } else {
            derive_low_bS_list(mbAddr, mbAddr - 1, 0, 3, 0, 1, true, bS_list, ctx);
            deblock_edge_low_bs_luma(luma_base_dst, widthY, 1, alphaLeft[0], betaLeft[0], indexALeft[0], bS_list);
            deblock_edge_low_bs_chroma(cb_base_dst, widthC, 1, alphaLeft[1], betaLeft[1], indexALeft[1], bS_list);
            deblock_edge_low_bs_chroma(cr_base_dst, widthC, 1, alphaLeft[2], betaLeft[2], indexALeft[2], bS_list);
        }
    }
    if (filterInternalEdges) {
        derive_alpha_beta(pic, mbAddr, mbAddr, alphaIn, betaIn, indexAIn, ctx);

        // x = 4
        if (!mb8x8) {
            derive_low_bS_list(mbAddr, mbAddr, 1, 0, 0, 0, true, bS_list, ctx);
            deblock_edge_low_bs_luma(luma_base_dst + 4, widthY, 1, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }

        // x = 8
        derive_low_bS_list(mbAddr, mbAddr, 2, 1, 1, 0, true, bS_list, ctx);
        deblock_edge_low_bs_luma(luma_base_dst + 8, widthY, 1, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        deblock_edge_low_bs_chroma(cb_base_dst + 4, widthC, 1, alphaIn[1], betaIn[1], indexAIn[1], bS_list);
        deblock_edge_low_bs_chroma(cr_base_dst + 4, widthC, 1, alphaIn[2], betaIn[2], indexAIn[2], bS_list);

        // x = 12
        if (!mb8x8) {
            derive_low_bS_list(mbAddr, mbAddr, 3, 2, 1, 1, true, bS_list, ctx);
            deblock_edge_low_bs_luma(luma_base_dst + 12, widthY, 1, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }
    }


    if (filterTopMbEdge) {
        // y = 0
        derive_alpha_beta(pic, mbAddr, mbAddr - mbWidth, alphaTop, betaTop, indexATop, ctx);

        if (IS_INTRA(ctx->mb_metadata[mbAddr - mbWidth].mb_type)) {
            deblock_edge_high_bs_luma(luma_base_dst, 1, widthY, alphaTop[0], betaTop[0]);
            deblock_edge_high_bs_chroma(cb_base_dst, 1, widthC, alphaTop[1], betaTop[1]);
            deblock_edge_high_bs_chroma(cr_base_dst, 1, widthC, alphaTop[2], betaTop[2]);
        } else {
            derive_low_bS_list(mbAddr, mbAddr - mbWidth, 0, 12, 0, 2, false, bS_list, ctx);
            deblock_edge_low_bs_luma(luma_base_dst, 1, widthY, alphaTop[0], betaTop[0], indexATop[0], bS_list);
            deblock_edge_low_bs_chroma(cb_base_dst, 1, widthC, alphaTop[1], betaTop[1], indexATop[1], bS_list);
            deblock_edge_low_bs_chroma(cr_base_dst, 1, widthC, alphaTop[2], betaTop[2], indexATop[2], bS_list);
        }
    }
    if (filterInternalEdges) {
        // y = 4
        if (!mb8x8) {
            derive_low_bS_list(mbAddr, mbAddr, 4, 0, 0, 0, false, bS_list, ctx);
            deblock_edge_low_bs_luma(luma_base_dst + 4*widthY, 1, widthY, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }

        // y = 8
        derive_low_bS_list(mbAddr, mbAddr, 8, 4, 2, 0, false, bS_list, ctx);
        deblock_edge_low_bs_luma(luma_base_dst + 8*widthY, 1, widthY, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        deblock_edge_low_bs_chroma(cb_base_dst + 4*widthC, 1, widthC, alphaIn[1], betaIn[1], indexAIn[1], bS_list);
        deblock_edge_low_bs_chroma(cr_base_dst + 4*widthC, 1, widthC, alphaIn[2], betaIn[2], indexAIn[2], bS_list);

        // y = 12
        if (!mb8x8) {
            derive_low_bS_list(mbAddr, mbAddr, 12, 8, 2, 2, false, bS_list, ctx);
            deblock_edge_low_bs_luma(luma_base_dst + 12*widthY, 1, widthY, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }
    }

    free(mb);
}

void deblock_macroblock_intra(Picture *pic, SliceHeader *sh, int mbAddr, const Undo264Context *ctx) {
    SPS *sps = sh->sps;

    // make dummy mb just for accessing the neighbors afterward
    Macroblock *mb = make_mb(mbAddr, ctx);
    derive_macroblock_neighbors(mb, sh->first_mb, ctx);


    const int widthY     = pic->widthY;
    const int widthC     = pic->widthC;
    const int mbWidth    = sps->pic_width_in_mbs;

    const bool disableSliceBoundaries = sh->disable_deblocking_filter_idc == 2;
    const bool filterInternalEdges    = sh->disable_deblocking_filter_idc != 1;
    const bool filterLeftMbEdge       = filterInternalEdges && (mb->mbAddr % mbWidth != 0) && (!disableSliceBoundaries || mb->has_mb_a);
    const bool filterTopMbEdge        = filterInternalEdges && (mb->mbAddr >= mbWidth) && (!disableSliceBoundaries || mb->has_mb_b);


    const int luma_pos   = mb->mb_y*16*widthY + mb->mb_x*16;
    const int chroma_pos = mb->mb_y*8*widthC + mb->mb_x*8;
    uint8_t *luma_base_dst = &pic->luma[luma_pos];
    uint8_t *cb_base_dst   = &pic->cb[chroma_pos];
    uint8_t *cr_base_dst   = &pic->cr[chroma_pos];

    const int bS_list[4] = {3, 3, 3, 3};

    uint8_t alphaLeft[3], betaLeft[3], indexALeft[3];
    uint8_t alphaTop[3],  betaTop[3],  indexATop[3];
    uint8_t alphaIn[3],   betaIn[3],   indexAIn[3];

    bool mb8x8 = ctx->mb_metadata[mbAddr].t_8x8_flag;


    if (filterLeftMbEdge) {
        // x = 0
        derive_alpha_beta(pic, mbAddr, mbAddr - 1, alphaLeft, betaLeft, indexALeft, ctx);

        deblock_edge_high_bs_luma(luma_base_dst, widthY, 1, alphaLeft[0], betaLeft[0]);
        deblock_edge_high_bs_chroma(cb_base_dst, widthC, 1, alphaLeft[1], betaLeft[1]);
        deblock_edge_high_bs_chroma(cr_base_dst, widthC, 1, alphaLeft[2], betaLeft[2]);
    }
    if (filterInternalEdges) {
        derive_alpha_beta(pic, mbAddr, mbAddr, alphaIn, betaIn, indexAIn, ctx);

        // x = 4
        if (!mb8x8) {
            deblock_edge_low_bs_luma(luma_base_dst + 4, widthY, 1, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }

        // x = 8
        deblock_edge_low_bs_luma(luma_base_dst + 8, widthY, 1, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        deblock_edge_low_bs_chroma(cb_base_dst + 4, widthC, 1, alphaIn[1], betaIn[1], indexAIn[1], bS_list);
        deblock_edge_low_bs_chroma(cr_base_dst + 4, widthC, 1, alphaIn[2], betaIn[2], indexAIn[2], bS_list);

        // x = 12
        if (!mb8x8) {
            deblock_edge_low_bs_luma(luma_base_dst + 12, widthY, 1, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }
    }


    if (filterTopMbEdge) {
        // y = 0
        derive_alpha_beta(pic, mbAddr, mbAddr - mbWidth, alphaTop, betaTop, indexATop, ctx);

        deblock_edge_high_bs_luma(luma_base_dst, 1, widthY, alphaTop[0], betaTop[0]);
        deblock_edge_high_bs_chroma(cb_base_dst, 1, widthC, alphaTop[1], betaTop[1]);
        deblock_edge_high_bs_chroma(cr_base_dst, 1, widthC, alphaTop[2], betaTop[2]);
    }
    if (filterInternalEdges) {
        // y = 4
        if (!mb8x8) {
            deblock_edge_low_bs_luma(luma_base_dst + 4*widthY, 1, widthY, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }

        // y = 8
        deblock_edge_low_bs_luma(luma_base_dst + 8*widthY, 1, widthY, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        deblock_edge_low_bs_chroma(cb_base_dst + 4*widthC, 1, widthC, alphaIn[1], betaIn[1], indexAIn[1], bS_list);
        deblock_edge_low_bs_chroma(cr_base_dst + 4*widthC, 1, widthC, alphaIn[2], betaIn[2], indexAIn[2], bS_list);

        // y = 12
        if (!mb8x8) {
            deblock_edge_low_bs_luma(luma_base_dst + 12*widthY, 1, widthY, alphaIn[0], betaIn[0], indexAIn[0], bS_list);
        }
    }

    free(mb);
}

void deblock_slice(Picture *pic, SliceHeader *sh, const Undo264Context *ctx) {
    for (unsigned i = sh->first_mb; i < sh->first_mb + ctx->current_slice->num_mbs; i++) {
        if (IS_INTRA(ctx->mb_metadata[i].mb_type)) {
            deblock_macroblock_intra(pic, sh, i, ctx);
        } else {
            deblock_macroblock(pic, sh, i, ctx);
        }
    }
}