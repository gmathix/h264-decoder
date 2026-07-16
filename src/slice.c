//
// Created by gmathix on 3/20/26.
//

// this file is compiled once for CAVLC and once for CABAC
#include "util/mbutil.h"
#include "util/sliceutil.h"
#include "cavlc.h"
#include "cabac.h"
#include "util/predutil.h"
#include "tests/profiler.h"

#undef CAFUNC
#undef CACALL
#include "mb.h"

#if CABAC
    #define CAFUNC(name, ...) name##_cabac(__VA_ARGS__)
    #define CACALL(name, ...) name##_cabac(__VA_ARGS__)
#else
    #define CAFUNC(name, ...) name##_cavlc(__VA_ARGS__)
    #define CACALL(name, ...) name##_cavlc(__VA_ARGS__)
#endif









/* 7.3.5.3.1 */
void CAFUNC(read_residual_luma,
    Macroblock *mb, int type, int t_8x8_flag, int cbp_luma,
    int startIdx, int endIdx,
    SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;


    if (startIdx == 0 && IS_INTRA16x16(type)) {
#if CABAC
        residual_block_cabac(mb, 0, 0, LUMA_INTRA_16x16_DC_LEVEL, mb->residuals.luma_16x16_DC, ctx->luma_total_coeffs, 0, 15, 16, true, sh, ctx);
#else
        residual_block_cavlc(mb, 0, 0, LUMA_INTRA_16x16_DC_LEVEL, mb->residuals.luma_16x16_DC, ctx->luma_total_coeffs, 0, 15, 16, true, sh, ctx);
#endif
    }

    for (int i8x8 = 0; i8x8 < 4; i8x8++) {
        if (!t_8x8_flag || !sh->pps->cabac_flag) {
            for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                int blkIdx = map_4x4[i8x8*4+i4x4];
                if (cbp_luma & (1 << i8x8)) {
                    if (IS_INTRA16x16(type)) {
#if CABAC
                        residual_block_cabac(mb, blkIdx, 0, LUMA_INTRA_16x16_AC_LEVEL, mb->residuals.luma_16x16_AC[blkIdx], ctx->luma_total_coeffs,
                            _max(0, startIdx - 1), endIdx - 1, 15, true, sh, ctx);
#else
                        residual_block_cavlc(mb, blkIdx, 0, LUMA_INTRA_16x16_AC_LEVEL, mb->residuals.luma_16x16_AC[blkIdx], ctx->luma_total_coeffs,
                            _max(0, startIdx - 1), endIdx - 1, 15, true, sh, ctx);
#endif
                    } else {
#if CABAC
                        residual_block_cabac(mb, blkIdx, 0, LUMA_LEVEL_4x4, mb->residuals.luma_4x4_coeffs[blkIdx], ctx->luma_total_coeffs,
                            startIdx, endIdx, 16, true, sh, ctx);
#else
                        residual_block_cavlc(mb, blkIdx, 0, LUMA_LEVEL_4x4, mb->residuals.luma_4x4_coeffs[blkIdx], ctx->luma_total_coeffs,
                            startIdx, endIdx, 16, true, sh, ctx);
#endif
                    }
                } else if (IS_INTRA16x16(type)) {
                    for (int i = 0; i < 15; i++) {
                        mb->residuals.luma_16x16_AC[blkIdx][i] = 0;
                    }
                } else {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_4x4_coeffs[blkIdx][i] = 0;
                    }
                }

                if (!sh->pps->cabac_flag && t_8x8_flag) {
                    for (int i = 0; i < 16; i++) {
                        mb->residuals.luma_8x8_coeffs[i8x8][4*i + i4x4] = mb->residuals.luma_4x4_coeffs[blkIdx][i];
                    }
                }
            }
        } else if (cbp_luma & (1 << i8x8)) {
#if CABAC
            residual_block_cabac(mb, 0, 0, LUMA_LEVEL_8x8, mb->residuals.luma_8x8_coeffs[i8x8], ctx->luma_total_coeffs,
                4*startIdx, 4*endIdx+3, 64, true, sh, ctx);
#else
            residual_block_cavlc(mb, 0, 0, LUMA_LEVEL_8x8, mb->residuals.luma_8x8_coeffs[i8x8], ctx->luma_total_coeffs,
                4*startIdx, 4*endIdx+3, 64, true, sh, ctx);
#endif
        } else {
            for (int i = 0; i < 64; i++) {
                mb->residuals.luma_8x8_coeffs[i8x8][i] = 0;
            }
        }
    }
}


/* 7.3.5.3 */

void CAFUNC(read_residual,
    Macroblock *mb, int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma,
    SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;



    CACALL(read_residual_luma, mb, type, t_8x8_flag, cbp_luma, startIdx, endIdx, sh, ctx);



    if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {

        int numC8x8 = 4 /
            (sub_width_c_info[sh->sps->chroma_format_idc] * sub_height_c_info[sh->sps->chroma_format_idc]);


        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t (*chroma_coeff_table)[16] = iCbCr
                ? ctx->cr_total_coeffs
                : ctx->cb_total_coeffs;

            if ((cbp_chroma & 3) && startIdx == 0) {
                /* chroma DC residual present */
#if CABAC
                residual_block_cabac(mb, 0, iCbCr, CHROMA_DC_LEVEL, mb->residuals.chroma_DC[iCbCr], chroma_coeff_table, 0, 4*numC8x8-1, 4*numC8x8, false, sh, ctx);
#else
                residual_block_cavlc(mb, 0, iCbCr, CHROMA_DC_LEVEL, mb->residuals.chroma_DC[iCbCr], chroma_coeff_table, 0, 4*numC8x8-1, 4*numC8x8, false, sh, ctx);
#endif
            } else {
                for (int i = 0; i < 4 * numC8x8; i++) {
                    mb->residuals.chroma_DC[iCbCr][i] = 0;
                }
            }
        }

        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            uint8_t (*chroma_coeff_table)[16] = iCbCr
                ? ctx->cr_total_coeffs
                : ctx->cb_total_coeffs;

            for (int i8x8 = 0; i8x8 < numC8x8; i8x8++) {
                for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                    if (cbp_chroma & 2) {
                        /* chroma AC residual present */
#if CABAC
                        residual_block_cabac(mb, i4x4, iCbCr, CHROMA_AC_LEVEL, mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4], chroma_coeff_table,
                            _max(0, startIdx-1), endIdx-1, 15, false, sh, ctx);
#else
                        residual_block_cavlc(mb, i4x4, iCbCr, CHROMA_AC_LEVEL, mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4], chroma_coeff_table,
                            _max(0, startIdx-1), endIdx-1, 15, false, sh, ctx);
#endif
                    } else {
                        for (int i = 0; i < 15; i++) {
                            mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4][i] = 0;
                        }
                    }
                }
            }
        }
    } else if (sh->sps->chroma_format_idc == 3) { /* 4:4:4 not handled for now */
        int16_t CbIntra16x16DC[16];
        int16_t CbIntra16x16AC[16][15];
        int16_t Cb4x4[16][16];
        int16_t Cb8x8[4][64];
        CACALL(read_residual_luma, mb, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx, sh, ctx);

        int16_t CrIntra16x16DC[16];
        int16_t CrIntra16x16AC[16][15];
        int16_t Cr4x4[16][16];
        int16_t Cr8x8[4][64];
        CACALL(read_residual_luma, mb, type, t_8x8_flag, cbp_chroma, startIdx,  endIdx, sh, ctx);
    }
}


/* 7.3.5.1 */
void CAFUNC(read_mb_pred,
    Macroblock *mb, SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;
    Picture *currPic = ctx->curr_pic;

    int mbAddr = mb->mbAddr;

    if (IS_INTRA4x4(mb->mb_type) ||
        IS_INTRA8x8(mb->mb_type) ||
        IS_INTRA16x16(mb->mb_type)) {

        if (IS_INTRA4x4(mb->mb_type)) {
            int intraModeA, intraModeB;
            for (int i = 0; i < 16; i++) {
                int blkIdx = map_4x4[i];
                Neighbors n = derive_neighbors_4x4(mb, blkIdx, ctx);
                int dcPredModePredictedFlag;


                int mb_a_type = n.a.av
                    ? ctx->mb_metadata[mbAddr + n.a.mb_off].mb_type
                    : mb->mb_type;
                int mb_b_type = n.b.av
                    ? ctx->mb_metadata[mbAddr + n.b.mb_off].mb_type
                    : mb->mb_type;

                dcPredModePredictedFlag = (!n.a.av || !n.b.av ||
                    (n.a.av && IS_INTER(mb_a_type) && ctx->ps->pps->constrained_intra_pred_flag) ||
                    (n.b.av && IS_INTER(mb_b_type) && ctx->ps->pps->constrained_intra_pred_flag));

                if (dcPredModePredictedFlag || !IS_INTRANxN(mb_a_type)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA4x4(mb_a_type)
                        ? ctx->intra4x4_pred_modes[mbAddr + n.a.mb_off][n.a.idx]
                        : ctx->intra8x8_pred_modes[mbAddr + n.a.mb_off][map_4x4[n.a.idx] / 4];
                }
                if (dcPredModePredictedFlag || !IS_INTRANxN(mb_b_type)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA4x4(mb_b_type)
                        ? ctx->intra4x4_pred_modes[mbAddr + n.b.mb_off][n.b.idx]
                        : ctx->intra8x8_pred_modes[mbAddr + n.b.mb_off][map_4x4[n.b.idx] / 4];
                }

                int predIntraMode = _min(intraModeA, intraModeB);

                uint8_t prev_intra4x4_pred_mode_flag = read_u(br, 1);
                if (!prev_intra4x4_pred_mode_flag) {
                    uint8_t rem_intra4x4_pred_mode = read_u(br, 3);
                    ctx->intra4x4_pred_modes[mbAddr][blkIdx] = rem_intra4x4_pred_mode < predIntraMode
                        ? rem_intra4x4_pred_mode
                        : rem_intra4x4_pred_mode+1;
                } else {
                    ctx->intra4x4_pred_modes[mbAddr][blkIdx] = predIntraMode;
                }
            }
        }
        else if (IS_INTRA8x8(mb->mb_type)) {
            for (int i8x8 = 0; i8x8 < 4; i8x8++) {
                Neighbors n = derive_neighbors_2x2(mb, i8x8, ctx);

                int aType = currPic->mb_types[mb->mbAddr + n.a.mb_off];
                int bType = currPic->mb_types[mb->mbAddr + n.b.mb_off];

                bool dcModePredicted = (!n.a.av || !n.b.av ||
                    (n.a.av && IS_INTER(currPic->mb_types[mb->mbAddr + n.a.mb_off]) && sh->pps->constrained_intra_pred_flag) ||
                    (n.b.av && IS_INTER(currPic->mb_types[mb->mbAddr + n.b.mb_off]) && sh->pps->constrained_intra_pred_flag));

                int intraModeA, intraModeB;

                if (dcModePredicted || !IS_INTRANxN(aType)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA8x8(aType)
                        ? ctx->intra8x8_pred_modes[mb->mbAddr + n.a.mb_off][n.a.idx]
                        : ctx->intra4x4_pred_modes[mb->mbAddr + n.a.mb_off][map_4x4[n.a.idx * 4 + 1]];
                }
                if (dcModePredicted || !IS_INTRANxN(bType)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA8x8(bType)
                        ? ctx->intra8x8_pred_modes[mb->mbAddr + n.b.mb_off][n.b.idx]
                        : ctx->intra4x4_pred_modes[mb->mbAddr + n.b.mb_off][map_4x4[n.b.idx * 4 + 2]];
                }

                int predMode = _min(intraModeA, intraModeB);

                uint8_t prev_intra8x8_pred_mode_flag = read_u(br, 1);
                if (!prev_intra8x8_pred_mode_flag) {
                    uint8_t rem_intra8x8_pred_mode = read_u(br, 3);
                    ctx->intra8x8_pred_modes[mb->mbAddr][i8x8] = rem_intra8x8_pred_mode < predMode
                        ? rem_intra8x8_pred_mode
                        : rem_intra8x8_pred_mode + 1;
                } else {
                    ctx->intra8x8_pred_modes[mb->mbAddr][i8x8] = predMode;
                }
            }
        }
        if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {
            mb->intra_chroma_pred_mode = read_ue(br);
        }
    } else if (!IS_DIRECT(mb->mb_type)) {
        int type = mb->u.pb.mb_info.type;

        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if (sh->num_ref_idx_l0_active_minus1 > 0 &&
                ((part == 0 && (type & MB_TYPE_P0L0)) ||
                 (part == 1 && (type & MB_TYPE_P1L0)))) {
                mb->u.pb.ref_idx[L0][part] = read_te(br, sh->num_ref_idx_l0_active_minus1);
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if (sh->num_ref_idx_l1_active_minus1 > 0 &&
                ((part == 0 && (type & MB_TYPE_P0L1)) ||
                 (part == 1 && (type & MB_TYPE_P1L1)))) {
                mb->u.pb.ref_idx[L1][part] = read_te(br, sh->num_ref_idx_l1_active_minus1);
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if ((part == 0 && (type & MB_TYPE_P0L0)) ||
                (part == 1 && (type & MB_TYPE_P1L0))) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L0][part][0][i] = read_se(br);
                }
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            if ((part == 0 && (type & MB_TYPE_P0L1)) ||
                (part == 1 && (type & MB_TYPE_P1L1))) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L1][part][0][i] = read_se(br);
                }
            }
        }
    }
}


void CAFUNC(read_sub_mb_pred,
    Macroblock *mb, SliceHeader *sh, CodecContext *ctx) {

    BitReader *br = ctx->br;

    for (int part = 0; part < 4; part++) {
        uint32_t sub_mb_type = read_ue(br);
        mb->u.pb.sub_mb_info[part] = IS_P_SLICE(sh->slice_type)
            ? p_sub_mb_type_info[sub_mb_type]
            : b_sub_mb_type_info[sub_mb_type];
    }
    for (int part = 0; part < 4; part++) {
        if (sh->num_ref_idx_l0_active_minus1 > 0 && !(mb->mb_type & MB_TYPE_REF0) &&
            !(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L0)) {
            mb->u.pb.ref_idx[L0][part] = read_te(br, sh->num_ref_idx_l0_active_minus1);
        }
    }
    for (int part = 0; part < 4; part++) {
        if (sh->num_ref_idx_l1_active_minus1 > 0 && !(mb->mb_type & MB_TYPE_REF0) &&
            !(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L1)) {
            mb->u.pb.ref_idx[L1][part] = read_te(br, sh->num_ref_idx_l1_active_minus1);
        }
    }
    for (int part = 0; part < 4; part++) {
        if (!(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L0)) {
            for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L0][part][subPart][i] = read_se(br);
                }
            }
        }
    }
    for (int part = 0; part < 4; part++) {
        if (!(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L1)) {
            for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                for (int i = 0; i < 2; i++) {
                    mb->u.pb.mvd[L1][part][subPart][i] = read_se(br);
                }
            }
        }
    }
}


/* 7.3.5 */
void CAFUNC(read_macroblock,
    Macroblock *mb, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {

    BitReader *br = ctx->br;


    PPS *pps = sh->pps;
    SPS *sps = sh->sps;


    uint32_t mb_type = read_ue(br);
    mb->table_idx = mb_type;


    if ((IS_I_SLICE(sh->slice_type) && mb_type > 25) ||
        (IS_P_SLICE(sh->slice_type) && mb_type > 30)  ||
        (IS_B_SLICE(sh->slice_type) && mb_type > 48)) {

        printf("mb_type out of bounds for %s slice : got %d (mbAddr:%d)\n",
            slice_type_to_string(sh->slice_type), mb_type, mb->mbAddr);
        return;
    }

    mb->mb_height_c = 16 / sub_height_c_info[sh->sps->chroma_format_idc];
    mb->mb_width_c  = 16 / sub_width_c_info[sh->sps->chroma_format_idc];

    mb->QPY = mb->mbAddr == 0
                ? sh->pps->pic_init_qp + sh->slice_qp_delta
                : ctx->prevMb->QPY;


    uint8_t pcm_samples_luma[256];
    uint8_t pcm_samples_chroma[2 * mb->mb_height_c * mb->mb_width_c];

    bool intra_mb = IS_I_SLICE(sh->slice_type) || (IS_P_SLICE(sh->slice_type) && mb_type > 4) || (IS_B_SLICE(sh->slice_type) && mb_type > 22);
    if (intra_mb && IS_P_SLICE(sh->slice_type)) {
        mb_type -= 5;
        mb->table_idx -= 5;
    } else if (intra_mb && IS_B_SLICE(sh->slice_type)) {
        mb_type -= 23;
        mb->table_idx -= 23;
    }

    if (intra_mb) {
        mb->u.i.mb_info = i_mb_type_info[mb_type];
        mb->mb_type     = i_mb_type_info[mb_type].type;
    } else if (IS_P_SLICE(sh->slice_type)) {
        mb->u.pb.mb_info = p_mb_type_info[mb_type];
        mb->mb_type      = p_mb_type_info[mb_type].type;
    } else if (IS_B_SLICE(sh->slice_type)) {
        mb->u.pb.mb_info = b_mb_type_info[mb_type];
        mb->mb_type      = b_mb_type_info[mb_type].type;
    }


    ctx->curr_pic->mb_types[mb->mbAddr] = mb->mb_type;
    int type = mb->mb_type;

    if (intra_mb) mb->pred_mode = i_mb_type_info[mb_type].pred_mode;
    else mb->pred_mode = -1;

    mb->slice_type = sh->slice_type;

    int pred_mode = intra_mb
        ? i_mb_type_info[mb_type].pred_mode
        : -1;

    int cbp_luma = intra_mb
        ? i_mb_type_info[mb_type].cbp_luma
        : -1;

    int cbp_chroma = intra_mb
        ? i_mb_type_info[mb_type].cbp_chroma
        : -1;

    mb->residuals.cbp_chroma = cbp_chroma;
    mb->residuals.cbp_luma = cbp_luma;


    memset(ctx->luma_total_coeffs[mb->mbAddr], 0, 16);
    memset(ctx->cb_total_coeffs[mb->mbAddr], 0, 16);
    memset(ctx->cr_total_coeffs[mb->mbAddr], 0, 16);

    MacroblockMetadata *meta = &ctx->mb_metadata[mb->mbAddr];
    meta->mb_type    = mb->mb_type;
    meta->cbp_luma   = cbp_luma;
    meta->cbp_chroma = cbp_chroma;
    meta->t_8x8_flag = 0;





    if (type == MB_TYPE_INTRA_PCM) { // just inject the samples directly
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);
        }

        int widthY = mb->p_pic->widthY;
        int widthC = mb->p_pic->widthC;
        int posY = mb->mb_y*16*widthY+ mb->mb_x*16;
        int posC = mb->mb_y*mb->mb_height_c*widthC + mb->mb_x*mb->mb_width_c;

        for (int i = 0; i < 256; i++) {
            mb->p_pic->luma[posY + (i/16)*widthY + i%16] = read_u(br, 8);
        }
        for (int i = 0; i < mb->mb_width_c * mb->mb_height_c; i++) {
            mb->p_pic->cb[posC + (i/mb->mb_height_c)*widthC + i%mb->mb_width_c] = read_u(br, 8);
        }
        for (int i = 0; i < mb->mb_width_c * mb->mb_height_c; i++) {
            mb->p_pic->cr[posC + (i/mb->mb_height_c)*widthC + i%mb->mb_width_c] = read_u(br, 8);
        }
    } else {

        bool noSubMbPartSizeLessThan8x8 = true;

        if (!intra_mb && mb->u.pb.mb_info.part_count == 4) {
            CACALL(read_sub_mb_pred, mb, sh, ctx);
            for (int part = 0; part < 4; part++) {
                if (!IS_SUB_DIRECT(mb->u.pb.sub_mb_info[part].type)) {
                    if (mb->u.pb.sub_mb_info[part].part_count > 1) {
                        noSubMbPartSizeLessThan8x8 = false;
                    }
                } else if (!sps->direct_8x8_inference_flag) {
                    noSubMbPartSizeLessThan8x8 = false;
                }
            }
        } else {
            if (pps->transform_8x8_mode_flag && IS_INTRA4x4(type)) {
                mb->t_8x8_flag = read_u(br, 1);
                meta->t_8x8_flag = mb->t_8x8_flag;
                if (mb->t_8x8_flag) {
                    mb->mb_type = MB_TYPE_INTRA8x8;
                    meta->mb_type = mb->mb_type;
                    ctx->curr_pic->mb_types[mb->mbAddr]  = mb->mb_type;
                }
            }
            CACALL(read_mb_pred, mb, sh, ctx);
        }

        if (!IS_INTRA16x16(type)) {
            int32_t cbp = map_coded_block_pattern(read_ue(br), sps->chroma_format_idc,
                intra_mb);

            cbp_luma   = cbp % 16;
            cbp_chroma = cbp / 16;
            mb->residuals.cbp_chroma = cbp_chroma;
            mb->residuals.cbp_luma = cbp_luma;
            meta->cbp_luma = cbp_luma;
            meta->cbp_chroma = cbp_chroma;

            if (cbp_luma > 0 && pps->transform_8x8_mode_flag &&
                !IS_INTRA4x4(type) && noSubMbPartSizeLessThan8x8 &&
                (!IS_DIRECT(type) || sps->direct_8x8_inference_flag)) {

                mb->t_8x8_flag = read_u(br, 1);
                meta->t_8x8_flag = mb->t_8x8_flag;
            }
        }

        if (cbp_luma > 0 || cbp_chroma > 0 || IS_INTRA16x16(type)) {

            mb->mb_qp_delta = read_se(br);

            mb->QPY = mb->mbAddr == 0
                ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + mb->mb_qp_delta + 52) % 52)
                : _clip3(0, 51, (ctx->prevMb->QPY + mb->mb_qp_delta + 52) % 52);


            int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
            mb->QPC = QPcTable[qPi];


            meta->QPY = mb->QPY;
            meta->QPC = mb->QPC;



            residual_func residual_block = sh->pps->cabac_flag
                ? &residual_block_cabac
                : &residual_block_cavlc;


            CACALL(read_residual, mb, type, mb->t_8x8_flag, 0, 15, cbp_luma, cbp_chroma, sh, ctx);
        } else {
            mb->QPY = mb->mbAddr == 0
                ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + 52) % 52)
                : ctx->prevMb->QPY;

            int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
            mb->QPC = QPcTable[qPi];

            meta->QPY = mb->QPY;
            meta->QPC = mb->QPC;
        }
    }


    ctx->prevMb = mb;
}




/* 7.3.4 */
void CAFUNC(decode_slice,
    SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx) {

    BitReader *br = ctx->br;


    PPS *pps = sh->pps;
    SPS *sps = sh->sps;

    if (pps->cabac_flag) {
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);
        }
    }

    int mbaff_frame_flag = sh->sps->mb_aff_flag;
    int currMbAddr = sh->first_mb * (1 + mbaff_frame_flag);

    int moreDataFlag = 1;
    int prevMbSkipped = 0;



    do {
        if (!IS_I_SLICE(sh->slice_type) && !IS_SI_SLICE(sh->slice_type)) {
            if (!pps->cabac_flag) {
                uint32_t mb_skip_run = read_ue(br);

                prevMbSkipped = mb_skip_run > 0;

                // if (ctx->prf->total_frames == 16 && mb_skip_run > 0) {
                //     printf("skip %d mbs\n", mb_skip_run);
                // }

                // decode skipped macroblocks
                for (int i = currMbAddr; i < currMbAddr + mb_skip_run; i++) {
                    Macroblock *mb = ctx->currMb;
                    reset_mb(mb, i, ctx);
                    derive_macroblock_neighbors(mb, ctx);

                    mb->mb_type = MB_TYPE_SKIP;
                    mb->slice_type = sh->slice_type;
                    mb->QPY = mb->mbAddr == sh->first_mb
                        ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + 52) % 52)
                        : ctx->prevMb->QPY;

                    int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
                    mb->QPC = QPcTable[qPi];

                    MacroblockMetadata *meta = &ctx->mb_metadata[mb->mbAddr];
                    meta->mb_type    = MB_TYPE_SKIP;
                    meta->QPY        = mb->QPY;
                    meta->QPC        = mb->QPC;
                    meta->cbp_luma   = 0;
                    meta->cbp_chroma = 0;
                    meta->t_8x8_flag = 0;
                    ctx->curr_pic->mb_types[mb->mbAddr] = mb->mb_type;

                    // for P_Skip only
                    // B_Skip will get this replaced later
                    mb->u.pb.mb_info.part_count = 1;
                    mb->u.pb.mb_info.mb_part_height = 16;
                    mb->u.pb.mb_info.mb_part_width = 16;


                    if (IS_I_SLICE(sh->slice_type)) {
                        decode_i_macroblock(mb, ctx->current_slice, ctx);
                    } else if (IS_P_SLICE(sh->slice_type)) {
                        decode_p_macroblock(mb, ctx->current_slice, ctx);
                    } else if (IS_B_SLICE(sh->slice_type)) {
                        decode_b_macroblock(mb, ctx->current_slice, ctx);
                    }

                    ctx->prevMb = mb;
                }

                currMbAddr += mb_skip_run;
                ctx->current_slice->num_mbs += mb_skip_run;

                if (mb_skip_run > 0) {
                    moreDataFlag = more_rbsp_data(br) && currMbAddr < ctx->num_mbs;
                }
            } else {
                /* read with CABAC */
            }
        }
        if (moreDataFlag) {
            if (mbaff_frame_flag && (currMbAddr%2 == 0 ||
                (currMbAddr%2 == 1 && prevMbSkipped))) {

                int mb_field_decoding_flag = read_u(br, 1);
            }

            profiler_start_mb(ctx->prf);

            Macroblock *mb = ctx->currMb;

            reset_mb(mb, currMbAddr, ctx);
            derive_macroblock_neighbors(mb, ctx);

            CACALL(read_macroblock, mb, sh, nal_unit, ctx);
            if (IS_I_SLICE(sh->slice_type)) {
                decode_i_macroblock(mb, ctx->current_slice, ctx);
            } else if (IS_P_SLICE(sh->slice_type)) {
                decode_p_macroblock(mb, ctx->current_slice, ctx);
            } else if (IS_B_SLICE(sh->slice_type)) {
                decode_b_macroblock(mb, ctx->current_slice, ctx);
            }

            profiler_end_mb(ctx->prf);
        }


        if (!pps->cabac_flag) {
            moreDataFlag = more_rbsp_data(br);
        } else {
            /* cabac shit */
        }

        if (moreDataFlag) {
            currMbAddr++;
            if (currMbAddr >= ctx->num_mbs) {
                moreDataFlag = false;
            } else {
                ctx->current_slice->num_mbs = currMbAddr + 1;
            }
        }
    } while (moreDataFlag);
}