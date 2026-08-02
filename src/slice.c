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
    SliceHeader *sh, Undo264Context *ctx) {

    BitReader *br = ctx->br;


    if (startIdx == 0 && IS_INTRA16x16(type)) {
        CACALL(residual_block, mb, 0, 0, LUMA_INTRA_16x16_DC_LEVEL, mb->residuals.luma_16x16_DC, ctx->luma_total_coeffs, 0, 15, 16, true, sh, ctx);
    }

    for (int i8x8 = 0; i8x8 < 4; i8x8++) {
        if (!t_8x8_flag || !sh->pps->cabac_flag) {
            for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                int blkIdx = map_4x4[i8x8*4+i4x4];
                if (cbp_luma & (1 << i8x8)) {
                    if (IS_INTRA16x16(type)) {
                        CACALL(residual_block, mb, blkIdx, 0, LUMA_INTRA_16x16_AC_LEVEL, mb->residuals.luma_16x16_AC[blkIdx], ctx->luma_total_coeffs,
                                _max(0, startIdx - 1), endIdx - 1, 15, true, sh, ctx);
                    } else {
                        CACALL(residual_block, mb, blkIdx, 0, LUMA_LEVEL_4x4, mb->residuals.luma_4x4_coeffs[blkIdx], ctx->luma_total_coeffs,
                                startIdx, endIdx, 16, true, sh, ctx);
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
            CACALL(residual_block, mb, i8x8, 0, LUMA_LEVEL_8x8, mb->residuals.luma_8x8_coeffs[i8x8], ctx->luma_total_coeffs,
                    4*startIdx, 4*endIdx+3, 64, true, sh, ctx);
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
    SliceHeader *sh, Undo264Context *ctx) {

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
                CACALL(residual_block, mb, 0, iCbCr, CHROMA_DC_LEVEL, mb->residuals.chroma_DC[iCbCr], chroma_coeff_table, 0, 4*numC8x8-1, 4*numC8x8, false, sh, ctx);
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
                        CACALL(residual_block, mb, i4x4, iCbCr, CHROMA_AC_LEVEL, mb->residuals.chroma_AC[iCbCr][i8x8*4 + i4x4], chroma_coeff_table,
                                _max(0, startIdx-1), endIdx-1, 15, false, sh, ctx);
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




//===== CABAC/CAVLC syntax element parsing =====//

int CAFUNC(read_I_mb_type,
    Macroblock *mb, SliceHeader *sh, int ctxIdx, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading mb_type I slice\n");
        #endif
        // maxBinIdx=6 ctxIdxOffset=3 for I slice
        // maxBinIdx=5 ctxIdxOffset=17 for P slice
        // maxBinIdx=5 ctxIdxOffset=32 for B slice
        static int ctxIdxIncI[7]  = {0, 0, 3, 4, 5, 6, 7};
        static int ctxIdxIncPB[7] = {0, 0, 1, 2, 2, 3, 3};
        if (ctxIdx == 3) { // I slice, needs computed ctxIdxInc for first bin
            ctxIdxIncI[0] = (mb->has_mb_a && !IS_INTRANxN(ctx->curr_pic->mb_types[mb->mbAddr + mb->mb_a_off])) +
                        (mb->has_mb_b && !IS_INTRANxN(ctx->curr_pic->mb_types[mb->mbAddr + mb->mb_b_off]));
        }
        int *ctxIdxInc = ctxIdx == 3 ? ctxIdxIncI : ctxIdxIncPB;

        int mb_type;
        if (!cabac_get_bit(ctx, ctxIdx + ctxIdxInc[0])) { // I_4x4
            mb_type = 0;
        } else {
            if (cabac_get_bit_term(ctx)) { // I_PCM
                mb_type = 25;
            } else { // I_16x16
                int str = 0;
                static const int str2mbtype1[12] = {
                    1, 2, 3, 4, -1, -1, -1, -1, 13, 14, 15, 16
                };
                static const int str2mbtype2[32] = {
                    -1, -1, -1, -1, -1, -1, -1, -1,
                    5, 6, 7, 8, 9, 10, 11, 12,
                    -1, -1, -1, -1, -1, -1, -1, -1,
                    17, 18, 19, 20, 21, 22, 23, 24,
                };
                str += str + cabac_get_bit(ctx, ctxIdx + ctxIdxInc[2]);
                str += str + cabac_get_bit(ctx, ctxIdx + ctxIdxInc[3]);
                int inc = ctxIdxInc[4] + !(str & 1);
                str += str + cabac_get_bit(ctx, ctxIdx + inc);
                inc = ctxIdx == 3 ? ctxIdxInc[5] + !(str >> 1 & 1) : ctxIdxInc[5];
                str += str + cabac_get_bit(ctx, ctxIdx + inc);
                if (!((str >= 0 && str <= 3) || (str >= 8 && str <= 11))) {
                    str += str + cabac_get_bit(ctx, ctxIdx + ctxIdxInc[6]);
                    mb_type = str2mbtype2[str];
                } else {
                    mb_type = str2mbtype1[str];
                }
            }
        }
    #else
        int mb_type = read_ue(br);
    #endif

    return mb_type;
}

int CAFUNC(read_P_mb_type,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading mb_type P slice\n");
        #endif
        // prefix: maxBinIdxCtx=2 ctxIdxOffset=14
        // suffix: maxBinIdxCtx=5 ctxIdxOffset=17
        static const int str2P_mbtype[4] = {0, 3, 2, 1};
        int mb_type = 0, str = 0;
        if (str += str + cabac_get_bit(ctx, 14), str == 0) { // P macroblock
            str += str + cabac_get_bit(ctx, 14 + 1);
            int inc = 2 + (str & 1);
            str += str + cabac_get_bit(ctx, 14 + inc);
            mb_type = str2P_mbtype[str];
        } else { // I macroblock
            mb_type = read_I_mb_type_cabac(mb, sh, 17, ctx) + 5;
        }
        return mb_type;
    #else
        return read_ue(br);
    #endif
}

int CAFUNC(read_B_mb_type,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading mb_type B slice\n");
        #endif
        // prefix: maxBinIdxCtx=3 ctxIdxOffset=27
        // suffix: maxBinIdxCtx=5 ctxIdxOffset=32
        static const int str2B_mb_type[26] = { // only for mb_type >= 3
             3,  4,  5,  6,  7,
             8,  9, 10, -1, -1,
            -1, -1, -1, -1, 11,
            22, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21
        };
        int mb_type = 0, str = 0;

        MacroblockMetadata metaA = mb->has_mb_a ? ctx->mb_metadata[mb->mbAddr - 1] : (MacroblockMetadata) {};
        MacroblockMetadata metaB = mb->has_mb_b ? ctx->mb_metadata[mb->mbAddr + mb->mb_b_off] : (MacroblockMetadata) {};

        int inc = (mb->has_mb_a && !IS_SKIP(metaA.mb_type) && !IS_DIRECT(metaA.mb_type)) +
                  (mb->has_mb_b && !IS_SKIP(metaB.mb_type) && !IS_DIRECT(metaB.mb_type));
        if (str += str + cabac_get_bit(ctx, 27 + inc), str == 0) {
            mb_type = 0; // B_Direct_16x16
        } else {
            str = 0;
            if (str += str + cabac_get_bit(ctx, 27 + 3), str == 0) { // L0_16x16 or L1_16x16
                str += str + cabac_get_bit(ctx, 27 + 5);
                mb_type = str + 1;
            } else {
                str = 0;
                str += str + cabac_get_bit(ctx, 27 + 4);
                str += str + cabac_get_bit(ctx, 27 + 5);
                str += str + cabac_get_bit(ctx, 27 + 5);
                str += str + cabac_get_bit(ctx, 27 + 5);
                if (str == 13) { // I macroblock
                    mb_type = read_I_mb_type_cabac(mb, sh, 32, ctx) + 23;
                } else if (!(str & 8) || str == 14 || str == 15) { // mb_type 3..10, L1_L0_8x16 or 8x8
                    mb_type = str2B_mb_type[str];
                } else {
                    str += str + cabac_get_bit(ctx, 27 + 5);
                    mb_type = str2B_mb_type[str];
                }
            }
        }
        return mb_type;
    #else
        return read_ue(br);
    #endif
}

int CAFUNC(read_P_sub_mb_type,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading sub_mb_type P slice\n");
        #endif
        // maxBinIdxCtx=2 ctxIdxOffset=21
        int sub_mb_type = 0, str = 0;
        if (str += str + cabac_get_bit(ctx, 21), str == 1) {
            sub_mb_type = 0;
        } else {
            if (str += str + cabac_get_bit(ctx, 21 + 1), str == 0) {
                sub_mb_type = 2;
            } else {
                str += str + cabac_get_bit(ctx, 21 + 2);
                sub_mb_type = 5 - str;
            }
        }
    #else
        int sub_mb_type = read_ue(br);
    #endif

    return sub_mb_type;
}

int CAFUNC(read_B_sub_mb_type,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading sub_mb_type B slice\n");
        #endif
        // maxBinIdxCtx=3 ctxIdxOffset=36
        static const int str2B_sub_mb_type[12] = {
              3,  4,  5,  6,
             -1, -1, 11, 12,
              7,  8,  9, 10,
        };
        int sub_mb_type = 0, str = 0;
        if (str += str + cabac_get_bit(ctx, 36), str == 0) {
            sub_mb_type = 0;
        } else {
            str = 0;
            if (str += str + cabac_get_bit(ctx, 36 + 1), str == 0) {
                str += str + cabac_get_bit(ctx, 36 + 3);
                sub_mb_type = str + 1;
            } else {
                str = 0;
                str += str + cabac_get_bit(ctx, 36 + 2);
                str += str + cabac_get_bit(ctx, 36 + 3);
                str += str + cabac_get_bit(ctx, 36 + 3);
                if (!(str & 4) || str == 7) {
                    sub_mb_type = str2B_sub_mb_type[str];
                } else {
                    str += str + cabac_get_bit(ctx, 36 + 3);
                    sub_mb_type = str2B_sub_mb_type[str];
                }
            }
        }
    #else
        int sub_mb_type = read_ue(br);
    #endif

    return sub_mb_type;
}

int CAFUNC(read_ref_idx,
    Macroblock *mb, int list, int pos4x4, int num_ref_active_minus1, SliceHeader *sh, Undo264Context *ctx) {

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading ref_idx_l%d\n", list);
        #endif

        int ref_idx = 0, str = 0;
        static int ctxIdxInc[16] = {0, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

        Neighbors n = derive_neighbors_4x4(mb, pos4x4, ctx);

        bool condTermA = true, condTermB = true;
        bool refIdxZeroA = false, refIdxZeroB = false;
        bool predModeEqualA = false, predModeEqualB = false;
        int aType = MB_TYPE_SKIP, bType = MB_TYPE_SKIP;
        if (n.a.av) {
            aType = ctx->mb_metadata[mb->mbAddr + n.a.mb_off].mb_type;
            MotionInfo motion_info_A = ctx->curr_pic->motion_info[mb->mbAddr + n.a.mb_off][n.a.idx];
            refIdxZeroA = motion_info_A.mvs[list].ref_idx <= 0;
            predModeEqualA = motion_info_A.mvs[list].ref_idx >= 0;
        }
        if (n.b.av) {
            bType = ctx->mb_metadata[mb->mbAddr + n.b.mb_off].mb_type;
            MotionInfo motion_info_B = ctx->curr_pic->motion_info[mb->mbAddr + n.b.mb_off][n.b.idx];
            refIdxZeroB = motion_info_B.mvs[list].ref_idx <= 0;
            predModeEqualB = motion_info_B.mvs[list].ref_idx >= 0;
        }

        /* historical note
         * These two lines were the last bug fixes. when I wrote them down, I prayed to the h264 gods,
         * ran it, and it finally worked.
         * The last bug was that a neighbouring 8x8 Direct partition would infer condTermN = true,
         * whereas the spec (9.3.3.1.1.6, predModeFlagN derivation) says that you should not infer true for Direct.
         * This required adding sub_mb_type to mb_metadata.
         *
         * After days of debugging CABAC painfully and mechanically, efforts finally paid and this was insanely gratifying.
         *
         * oh wow you're still reading this! you'd better go read mvpred.h to learn something about over-duplication
         * I'm still hyped because I'm writing this while watching the working decoded video over and over again asfdk;ljasf
         * so aslkdf sorry if i'm writing bullshit asklfj sakf saklf but my hands asldkfj are shaking ahdhahahahahah
         * i'm so fucking happy
         *
         *
         * mathis
         */
        bool direct8x8subPart_A = (IS_B_SLICE(sh->slice_type) && IS_8x8(aType) && ctx->mb_metadata[mb->mbAddr + n.a.mb_off].sub_mb_type[map_4x4[n.a.idx] / 4] == 0);
        bool direct8x8subPart_B = (IS_B_SLICE(sh->slice_type) && IS_8x8(bType) && ctx->mb_metadata[mb->mbAddr + n.b.mb_off].sub_mb_type[map_4x4[n.b.idx] / 4] == 0);

        condTermA = n.a.av && !IS_SKIP(aType) && !IS_DIRECT(aType) && !IS_INTRA(aType) && !direct8x8subPart_A && predModeEqualA && !refIdxZeroA;
        condTermB = n.b.av && !IS_SKIP(bType) && !IS_DIRECT(bType) && !IS_INTRA(bType) && !direct8x8subPart_B && predModeEqualB && !refIdxZeroB;

        ctxIdxInc[0] = condTermA + 2*condTermB;

        while (str += str + cabac_get_bit(ctx, 54 + ctxIdxInc[ref_idx]),
               str & 1) {
            ref_idx++;
        }

        return ref_idx;
    #else
        return read_te(ctx->br, num_ref_active_minus1);
    #endif
}

int CAFUNC(read_mvd,
    Macroblock *mb, int list, int xy, int pos4x4, SliceHeader *sh, Undo264Context *ctx) {

    #if CABAC
    #if CABAC_LOG
        fprintf(ctx->log_file, "\nreading mvd_l%d[%d]\n", list, xy);
    #endif
    static int ctxIdxInc[9] = {0, 3, 4, 5, 6, 6, 6, 6, 6};
    int mvd = 0, str = 0;


    Neighbors n = derive_neighbors_4x4(mb, pos4x4, ctx);

    bool predModeEqualA = n.a.av && ctx->curr_pic->motion_info[mb->mbAddr + n.a.mb_off][n.a.idx].mvs[list].ref_idx >= 0;
    bool predModeEqualB = n.b.av && ctx->curr_pic->motion_info[mb->mbAddr + n.b.mb_off][n.b.idx].mvs[list].ref_idx >= 0;

    MacroblockMetadata metaA = n.a.av ? ctx->mb_metadata[mb->mbAddr + n.a.mb_off] : (MacroblockMetadata) {};
    MacroblockMetadata metaB = n.b.av ? ctx->mb_metadata[mb->mbAddr + n.b.mb_off] : (MacroblockMetadata) {};

    int absMvdCompA = (n.a.av && !IS_SKIP(metaA.mb_type) && !IS_INTRA(metaA.mb_type) && predModeEqualA)
                      * (_abs(metaA.mvd[list][n.a.idx][xy]));
    int absMvdCompB = (n.b.av && !IS_SKIP(metaB.mb_type) && !IS_INTRA(metaB.mb_type) && predModeEqualB)
                      * (_abs(metaB.mvd[list][n.b.idx][xy]));

    int inc = 0;
    inc += 2 * (absMvdCompA + absMvdCompB > 32);
    inc += (inc == 0) * (absMvdCompA + absMvdCompB >= 3);
    ctxIdxInc[0] = inc;
    int ctxIdx = 40 + xy*7; // 40 for x, 47 for y
    while (str += str + cabac_get_bit(ctx, ctxIdx + ctxIdxInc[mvd]),
           (str & 1) && str != 0x1FF) {
        mvd++;
    }
    if (str == 0x1FF) mvd = 9;


    if (mvd == 9) {
        int k = 3;
        int bin;
        int symbol = 0, bin_symbol = 0;
        int val = 0;
        do {
            bin = cabac_get_bit_bypass(ctx);
            if (bin == 1) {
                symbol += 1 << k;
                k++;
            }
        } while (bin != 0);
        while (k--) {
            if (cabac_get_bit_bypass(ctx) == 1) {
                bin_symbol |= 1 << k;
            }
        }
        mvd += symbol + bin_symbol;
    }
    if (mvd != 0) {
        int signBit = cabac_get_bit_bypass(ctx);
        mvd *= (1 - 2*signBit);
    }

    return mvd;

    #else
        return read_se(ctx->br);
    #endif
}


int CAFUNC(read_intra_chroma_pred_mode,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading intra chroma pred mode\n");
        #endif
        // maxBinIdxCtx=1 ctxIdxOffset=64
        int intra_chroma_pred_mode = 0, str = 0;
        int inc = (mb->has_mb_a && !IS_INTER(ctx->curr_pic->mb_types[mb->mbAddr + mb->mb_a_off]) &&
                    !IS_PCM(ctx->curr_pic->mb_types[mb->mbAddr + mb->mb_a_off]) && ctx->mb_metadata[mb->mbAddr + mb->mb_a_off].intra_chroma_pred_mode != 0) +
                  (mb->has_mb_b && !IS_INTER(ctx->curr_pic->mb_types[mb->mbAddr + mb->mb_b_off]) &&
                    !IS_PCM(ctx->curr_pic->mb_types[mb->mbAddr + mb->mb_b_off]) && ctx->mb_metadata[mb->mbAddr + mb->mb_b_off].intra_chroma_pred_mode != 0);
        static int incs[3] = {3, 3, 3};
        incs[0] = inc;
        while (str += str + cabac_get_bit(ctx, 64 + incs[intra_chroma_pred_mode]),
               (str & 1) && str != 7) {
            intra_chroma_pred_mode++;
        }
        if (str == 7) intra_chroma_pred_mode = 3;
        return intra_chroma_pred_mode;
    #else
        return read_ue(ctx->br);
    #endif
}

int CAFUNC(read_coded_block_pattern,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading coded_block_pattern\n");
        #endif
        // prefix : maxBinIdxCtx=3 ctxIdxOffset=73
        // suffix : maxBinIdxCtx=1 ctxIdxOffset=77
        if (ctx->prf->total_frames == 6 && mb->mbAddr == 3900) {
            printf("%d %d %d %d\n", p_state_idx[73], p_state_idx[74], p_state_idx[75], p_state_idx[76]);
        }
        int cbp_luma = 0, inc = 0;
        MacroblockMetadata metaA = mb->has_mb_a ? ctx->mb_metadata[mb->mbAddr - 1] :(MacroblockMetadata) {};
        MacroblockMetadata metaB = mb->has_mb_b ? ctx->mb_metadata[mb->mbAddr + mb->mb_b_off] : (MacroblockMetadata) {};

        inc = (mb->has_mb_a && !IS_PCM(metaA.mb_type) && (IS_SKIP(metaA.mb_type) || ((metaA.cbp_luma >> 1 & 1) == 0)))
                + 2 * (mb->has_mb_b && !IS_PCM(metaB.mb_type) && (IS_SKIP(metaB.mb_type) || ((metaB.cbp_luma >> 2 & 1) == 0)));
        cbp_luma += cabac_get_bit(ctx, 73 + inc) << 0; // blkIdx = 0

        inc = ((cbp_luma >> 0 & 1) == 0)
                + 2 * (mb->has_mb_b && !IS_PCM(metaB.mb_type) && (IS_SKIP(metaB.mb_type) || ((metaB.cbp_luma >> 3 & 1) == 0)));
        cbp_luma += cabac_get_bit(ctx, 73 + inc) << 1; // blkIdx = 1

        inc = (mb->has_mb_a && !IS_PCM(metaA.mb_type) && (IS_SKIP(metaA.cbp_luma) || ((metaA.cbp_luma >> 3 & 1) == 0)))
                + 2 * ((cbp_luma >> 0 & 1) == 0);
        cbp_luma += cabac_get_bit(ctx, 73 + inc) << 2; // blkIdx = 2

        inc = ((cbp_luma >> 2 & 1) == 0) + 2*((cbp_luma >> 1 & 1) == 0);
        cbp_luma += cabac_get_bit(ctx, 73 + inc) << 3; // blkIdx = 3

        int cbp_chroma = 0, str = 0;
        static int ctxIdxInc[2];
        inc = (mb->has_mb_a && IS_PCM(metaA.mb_type) ||
              ((mb->has_mb_a && !IS_SKIP(metaA.mb_type) &&
                metaA.cbp_chroma != 0)))
             + 2 * (mb->has_mb_b && IS_PCM(metaB.mb_type) ||
                   (mb->has_mb_b && !IS_SKIP(metaB.mb_type) &&
                    metaB.cbp_chroma != 0));
        ctxIdxInc[0] = inc;
        inc = (mb->has_mb_a && IS_PCM(metaA.mb_type) ||
              ((mb->has_mb_a && !IS_SKIP(metaA.mb_type) &&
                metaA.cbp_chroma == 2)))
             + 4 + 2 * (mb->has_mb_b && IS_PCM(metaB.mb_type) ||
                   (mb->has_mb_b && !IS_SKIP(metaB.mb_type) &&
                    metaB.cbp_chroma == 2));
        ctxIdxInc[1] = inc;

        while (str += str + cabac_get_bit(ctx, 77 + ctxIdxInc[cbp_chroma]),
               (str & 1) && str != 3) {
            cbp_chroma++;
        }
        if (str == 3) cbp_chroma = 2;

        return cbp_luma | (cbp_chroma << 4);
    #else
        return map_coded_block_pattern(read_ue(ctx->br), sh->sps->chroma_format_idc, IS_INTRA(mb->mb_type));
    #endif
}

int CAFUNC(read_mb_qp_delta,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    #if CABAC
        #if CABAC_LOG
            fprintf(ctx->log_file, "\nreading mb_qp_delta\n");
        #endif
        // maxBinIdxCtx=2 ctxIdxOffset=60
        int val = 0, str = 0;
        MacroblockMetadata prevMeta = mb->mbAddr > 0 ? ctx->mb_metadata[mb->mbAddr - 1] : ctx->mb_metadata[0];
        int inc = !((mb->mbAddr == 0 || IS_SKIP(prevMeta.mb_type)) ||
                    (IS_PCM(prevMeta.mb_type)) ||
                    (!IS_INTRA16x16(prevMeta.mb_type) && prevMeta.cbp_chroma == 0 && prevMeta.cbp_luma == 0) ||
                    (ctx->prevMb->mb_qp_delta == 0));
        static int incs[3] = {0, 2, 3};
        incs[0] = inc;
        while (str += str + cabac_get_bit(ctx, 60 + incs[_clip3(0, 2, val)]),
               str & 1) {
            val++;
        }
        return (-1 +
            2*(val & 1)) * ((val + 1) / 2);
    #else
        return read_se(ctx->br);
    #endif
}


/* 7.3.5.1 */
void CAFUNC(read_mb_pred,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

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
                    (n.a.av && IS_INTER(mb_a_type) && sh->pps->constrained_intra_pred_flag) ||
                    (n.b.av && IS_INTER(mb_b_type) && sh->pps->constrained_intra_pred_flag));

                if (dcPredModePredictedFlag || !IS_INTRANxN(mb_a_type)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA4x4(mb_a_type)
                        ? ctx->mb_metadata[mb->mbAddr + n.a.mb_off].intra_NxN_pred_mode[n.a.idx]
                        : ctx->mb_metadata[mb->mbAddr + n.a.mb_off].intra_NxN_pred_mode[map_4x4[n.a.idx] / 4];
                }
                if (dcPredModePredictedFlag || !IS_INTRANxN(mb_b_type)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA4x4(mb_b_type)
                        ? ctx->mb_metadata[mb->mbAddr + n.b.mb_off].intra_NxN_pred_mode[n.b.idx]
                        : ctx->mb_metadata[mb->mbAddr + n.b.mb_off].intra_NxN_pred_mode[map_4x4[n.b.idx] / 4];
                }

                int predIntraMode = _min(intraModeA, intraModeB);

                #if CABAC
                    #if CABAC_LOG
                        fprintf(ctx->log_file, "\nreading prev_intra4x4_pred_mode_flag\n");
                    #endif
                    int8_t prev_intra4x4_pred_mode_flag = cabac_get_bit(ctx, 68);
                #else
                    uint8_t prev_intra4x4_pred_mode_flag = read_u(br, 1);
                #endif
                if (!prev_intra4x4_pred_mode_flag) {
                    #if CABAC
                        #if CABAC_LOG
                            fprintf(ctx->log_file, "\nreading rem_intra4x4_pred_mode\n");
                        #endif
                        uint8_t rem_intra4x4_pred_mode = 0;
                        rem_intra4x4_pred_mode += cabac_get_bit(ctx, 69);
                        rem_intra4x4_pred_mode += cabac_get_bit(ctx, 69) << 1;
                        rem_intra4x4_pred_mode += cabac_get_bit(ctx, 69) << 2;
                    #else
                        uint8_t rem_intra4x4_pred_mode = read_u(br, 3);
                    #endif
                    ctx->mb_metadata[mbAddr].intra_NxN_pred_mode[blkIdx] = rem_intra4x4_pred_mode < predIntraMode
                        ? rem_intra4x4_pred_mode
                        : rem_intra4x4_pred_mode+1;
                } else {
                    ctx->mb_metadata[mbAddr].intra_NxN_pred_mode[blkIdx] = predIntraMode;
                }
            }
        }
        else if (IS_INTRA8x8(mb->mb_type)) {
            for (int i8x8 = 0; i8x8 < 4; i8x8++) {
                Neighbors n = derive_neighbors_2x2(mb, i8x8, ctx);

                int aType = n.a.av ? currPic->mb_types[mb->mbAddr + n.a.mb_off] : mb->mb_type;
                int bType = n.b.av ? currPic->mb_types[mb->mbAddr + n.b.mb_off] : mb->mb_type;

                bool dcModePredicted = (!n.a.av || !n.b.av ||
                    (n.a.av && IS_INTER(currPic->mb_types[mb->mbAddr + n.a.mb_off]) && sh->pps->constrained_intra_pred_flag) ||
                    (n.b.av && IS_INTER(currPic->mb_types[mb->mbAddr + n.b.mb_off]) && sh->pps->constrained_intra_pred_flag));

                int intraModeA, intraModeB;

                if (dcModePredicted || !IS_INTRANxN(aType)) {
                    intraModeA = DC_PRED;
                } else {
                    intraModeA = IS_INTRA8x8(aType)
                        ? ctx->mb_metadata[mb->mbAddr + n.a.mb_off].intra_NxN_pred_mode[n.a.idx]
                        : ctx->mb_metadata[mb->mbAddr + n.a.mb_off].intra_NxN_pred_mode[map_4x4[n.a.idx * 4 + 1]];
                }
                if (dcModePredicted || !IS_INTRANxN(bType)) {
                    intraModeB = DC_PRED;
                } else {
                    intraModeB = IS_INTRA8x8(bType)
                        ? ctx->mb_metadata[mb->mbAddr + n.b.mb_off].intra_NxN_pred_mode[n.b.idx]
                        : ctx->mb_metadata[mb->mbAddr + n.b.mb_off].intra_NxN_pred_mode[map_4x4[n.b.idx * 4 + 2]];
                }

                int predMode = _min(intraModeA, intraModeB);


                #if CABAC
                    #if CABAC_LOG
                        fprintf(ctx->log_file, "\nreading prev_intra8x8_pred_mode_flag\n");
                    #endif
                    int8_t prev_intra8x8_pred_mode_flag = cabac_get_bit(ctx, 68);
                #else
                    uint8_t prev_intra8x8_pred_mode_flag = read_u(br, 1);
                #endif
                if (!prev_intra8x8_pred_mode_flag) {
                    #if CABAC
                        #if CABAC_LOG
                            fprintf(ctx->log_file, "\nreading rem_intra8x8_pred_mode\n");
                        #endif
                        uint8_t rem_intra8x8_pred_mode = 0;
                        rem_intra8x8_pred_mode += cabac_get_bit(ctx, 69);
                        rem_intra8x8_pred_mode += cabac_get_bit(ctx, 69) << 1;
                        rem_intra8x8_pred_mode += cabac_get_bit(ctx, 69) << 2;
                    #else
                        uint8_t rem_intra8x8_pred_mode = read_u(br, 3);
                    #endif
                    ctx->mb_metadata[mbAddr].intra_NxN_pred_mode[i8x8] = rem_intra8x8_pred_mode < predMode
                        ? rem_intra8x8_pred_mode
                        : rem_intra8x8_pred_mode + 1;
                } else {
                    ctx->mb_metadata[mbAddr].intra_NxN_pred_mode[i8x8] = predMode;
                }
            }
        } else if (IS_INTRA16x16(mb->mb_type)) {
            ctx->mb_metadata[mbAddr].intra_16x16_pred_mode = mb->u.i.mb_info.pred_mode;
        }

        if (sh->sps->chroma_format_idc == 1 || sh->sps->chroma_format_idc == 2) {
            int pred_mode = CACALL(read_intra_chroma_pred_mode, mb, sh, ctx);
            ctx->mb_metadata[mbAddr].intra_chroma_pred_mode = pred_mode;
        }
    } else if (!IS_DIRECT(mb->mb_type)) {
        int type = mb->u.pb.mb_info.type;
        int w = mb->u.pb.mb_info.mb_part_width;
        int h = mb->u.pb.mb_info.mb_part_height;

        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            int pos4x4 = part * ((w == 8) * 2 + (h == 8) * 8);
            if (sh->num_ref_idx_l0_active_minus1 > 0 &&
                ((part == 0 && (type & MB_TYPE_P0L0)) ||
                 (part == 1 && (type & MB_TYPE_P1L0)))) {
                int ref_idx = CACALL(read_ref_idx, mb, L0, pos4x4, sh->num_ref_idx_l0_active_minus1, sh, ctx);
                mb->u.pb.ref_idx[L0][part] = ref_idx;
                for (int blk = 0; blk < (w >> 2) * (h >> 2); blk++) {
                    int blkIdx = (w == 16) * (part * 8 + blk) +
                                 (w ==  8) * (part * 2 + blk + 2*(blk/2));
                    ctx->curr_pic->motion_info[mb->mbAddr][blkIdx].mvs[L0].ref_idx = ref_idx;
                }
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            int pos4x4 = part * ((w == 8) * 2 + (h == 8) * 8);
            if (sh->num_ref_idx_l1_active_minus1 > 0 &&
                ((part == 0 && (type & MB_TYPE_P0L1)) ||
                 (part == 1 && (type & MB_TYPE_P1L1)))) {
                int ref_idx = CACALL(read_ref_idx, mb, L1, pos4x4, sh->num_ref_idx_l1_active_minus1, sh, ctx);
                mb->u.pb.ref_idx[L1][part] = ref_idx;
                for (int blk = 0; blk < (w >> 2) * (h >> 2); blk++) {
                    int blkIdx = (w == 16) * (part * 8 + blk) +
                                 (w ==  8) * (part * 2 + blk + 2*(blk/2));
                    ctx->curr_pic->motion_info[mb->mbAddr][blkIdx].mvs[L1].ref_idx = ref_idx;
                }
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            int pos4x4 = part * ((w == 8) * 2 + (h == 8) * 8);
            if ((part == 0 && (type & MB_TYPE_P0L0)) ||
                (part == 1 && (type & MB_TYPE_P1L0))) {
                for (int xy = 0; xy < 2; xy++) {
                    int mvd = CACALL(read_mvd, mb, L0, xy, pos4x4, sh, ctx);
                    mb->u.pb.mvd[L0][part][0][xy] = mvd;
                    for (int blk = 0; blk < (w >> 2) * (h >> 2); blk++) {
                        int blkIdx = (w == 16) * (part * 8 + blk) +
                                     (w ==  8) * (part * 2 + blk + 2*(blk/2));
                        ctx->mb_metadata[mb->mbAddr].mvd[L0][blkIdx][xy] = mvd;
                    }
                }
            }
        }
        for (int part = 0; part < mb->u.pb.mb_info.part_count; part++) {
            int pos4x4 = part * ((w == 8) * 2 + (h == 8) * 8);
            if ((part == 0 && (type & MB_TYPE_P0L1)) ||
                (part == 1 && (type & MB_TYPE_P1L1))) {
                for (int xy = 0; xy < 2; xy++) {
                    int mvd =  CACALL(read_mvd, mb, L1, xy, pos4x4, sh, ctx);
                    mb->u.pb.mvd[L1][part][0][xy] = mvd;
                    for (int blk = 0; blk < (w >> 2) * (h >> 2); blk++) {
                        int blkIdx = (w == 16) * (part * 8 + blk) +
                                     (w ==  8) * (part * 2 + blk + 2*(blk/2));
                        ctx->mb_metadata[mb->mbAddr].mvd[L1][blkIdx][xy] = mvd;
                    }
                }
            }
        }
    }
}


void CAFUNC(read_sub_mb_pred,
    Macroblock *mb, SliceHeader *sh, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    for (int part = 0; part < 4; part++) {
        int sub_mb_type = IS_P_SLICE(sh->slice_type)
            ? CACALL(read_P_sub_mb_type, mb, sh, ctx)
            : CACALL(read_B_sub_mb_type, mb, sh, ctx);
        mb->u.pb.sub_mb_info[part] = IS_P_SLICE(sh->slice_type)
            ? p_sub_mb_type_info[sub_mb_type]
            : b_sub_mb_type_info[sub_mb_type];
        ctx->mb_metadata[mb->mbAddr].sub_mb_type[part] = sub_mb_type;
    }
    for (int part = 0; part < 4; part++) {
        int w = mb->u.pb.sub_mb_info[part].mb_part_width;
        int h = mb->u.pb.sub_mb_info[part].mb_part_height;
        int pos4x4 = map_4x4[part * 4];
        if (sh->num_ref_idx_l0_active_minus1 > 0 && !(mb->mb_type & MB_TYPE_REF0) &&
            !(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L0)) {
            int ref_idx = CACALL(read_ref_idx, mb, L0, pos4x4, sh->num_ref_idx_l0_active_minus1, sh, ctx);
            mb->u.pb.ref_idx[L0][part] = ref_idx;
            for (int blk = 0; blk < 4; blk++) {
                int blkIdx = pos4x4 + blk + 2*(blk/2);
                ctx->curr_pic->motion_info[mb->mbAddr][blkIdx].mvs[L0].ref_idx = ref_idx;
            }
        }
    }
    for (int part = 0; part < 4; part++) {
        int w = mb->u.pb.sub_mb_info[part].mb_part_width;
        int h = mb->u.pb.sub_mb_info[part].mb_part_height;
        int pos4x4 = map_4x4[part * 4];
        if (sh->num_ref_idx_l1_active_minus1 > 0 && !(mb->mb_type & MB_TYPE_REF0) &&
            !(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L1)) {
            int ref_idx = CACALL(read_ref_idx, mb, L1, pos4x4, sh->num_ref_idx_l1_active_minus1, sh, ctx);
            mb->u.pb.ref_idx[L1][part] = ref_idx;
            for (int blk = 0; blk < 4; blk++) {
                int blkIdx = pos4x4 + blk + 2*(blk/2);
                ctx->curr_pic->motion_info[mb->mbAddr][blkIdx].mvs[L1].ref_idx = ref_idx;
            }
        }
    }
    for (int part = 0; part < 4; part++) {
        int w = mb->u.pb.sub_mb_info[part].mb_part_width;
        int h = mb->u.pb.sub_mb_info[part].mb_part_height;
        int pos4x4 = map_4x4[part * 4];
        if (!(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L0)) {
            for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                for (int xy = 0; xy < 2; xy++) {
                    int mvd = CACALL(read_mvd, mb, L0, xy, pos4x4, sh, ctx);
                    mb->u.pb.mvd[L0][part][subPart][xy] = mvd;
                    for (int blk = 0; blk < (w >> 2) * (h >> 2); blk++) {
                        int blkIdx = (w == 8 && h == 8) * map_4x4[map_4x4[pos4x4] + blk] +
                                     (w == 8 && h == 4) * (pos4x4 + blk + subPart*4) +
                                     (w == 4 && h == 8) * (pos4x4 + (blk%2)*4 + subPart) +
                                     (w == 4 && h == 4) * (pos4x4 + subPart + 2*(subPart/2));
                        ctx->mb_metadata[mb->mbAddr].mvd[L0][blkIdx][xy] = mvd;
                    }
                }
            }
        }
    }
    for (int part = 0; part < 4; part++) {
        int w = mb->u.pb.sub_mb_info[part].mb_part_width;
        int h = mb->u.pb.sub_mb_info[part].mb_part_height;
        int pos4x4 = map_4x4[part * 4];
        if (!(mb->u.pb.sub_mb_info[part].type & SUB_MB_TYPE_DIRECT && IS_B_SLICE(sh->slice_type)) &&
            (mb->u.pb.sub_mb_info[part].type & MB_TYPE_P0L1)) {
            for (int subPart = 0; subPart < mb->u.pb.sub_mb_info[part].part_count; subPart++) {
                for (int xy = 0; xy < 2; xy++) {
                    int mvd = CACALL(read_mvd, mb, L1, xy, pos4x4, sh, ctx);
                    mb->u.pb.mvd[L1][part][subPart][xy] = mvd;
                    for (int blk = 0; blk < (w >> 2) * (h >> 2); blk++) {
                        int blkIdx = (w == 8 && h == 8) * map_4x4[map_4x4[pos4x4] + blk] +
                                     (w == 8 && h == 4) * (pos4x4 + blk + subPart*4) +
                                     (w == 4 && h == 8) * (pos4x4 + (blk%2)*4 + subPart) +
                                     (w == 4 && h == 4) * (pos4x4 + subPart + 2*(subPart/2));
                        ctx->mb_metadata[mb->mbAddr].mvd[L1][blkIdx][xy] = mvd;
                    }
                }
            }
        }
    }
}









/* 7.3.5 */
void CAFUNC(read_macroblock,
    Macroblock *mb, SliceHeader *sh, NalUnit *nal_unit, Undo264Context *ctx) {

    BitReader *br = ctx->br;



    PPS *pps = sh->pps;
    SPS *sps = sh->sps;


    #if CABAC
        int mb_type;
        if (IS_I_SLICE(sh->slice_type))      mb_type = read_I_mb_type_cabac(mb, sh, 3, ctx);
        else if (IS_P_SLICE(sh->slice_type)) mb_type = read_P_mb_type_cabac(mb, sh, ctx);
        else if (IS_B_SLICE(sh->slice_type)) mb_type = read_B_mb_type_cabac(mb, sh, ctx);
    #else
        uint32_t mb_type = read_ue(br);
    #endif


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
    mb->t_8x8_flag = 0;
    memset(meta->coded_block_flag, 0, 14 * 16);
    memset(meta->mvd, 0, 2 * 16 * 2 * sizeof(int16_t));
    for (int blk = 0; blk < 16; blk++) {
        ctx->curr_pic->motion_info[mb->mbAddr][blk].mvs[L0].ref_idx = 0;
        ctx->curr_pic->motion_info[mb->mbAddr][blk].mvs[L1].ref_idx = 0;
    }





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

        #if CABAC
            cabac_init_engine(ctx);
        #endif
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
                #if CABAC
                    #if CABAC_LOG
                        fprintf(ctx->log_file, "\nreading transform_8x8_flag\n");
                    #endif
                    int inc = (mb->has_mb_a && ctx->mb_metadata[mb->mbAddr - 1].t_8x8_flag) +
                              (mb->has_mb_b && ctx->mb_metadata[mb->mbAddr + mb->mb_b_off].t_8x8_flag);
                    mb->t_8x8_flag = cabac_get_bit(ctx, 399 + inc);
                #else
                    mb->t_8x8_flag = read_u(br, 1);
                #endif
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
            int cbp = CACALL(read_coded_block_pattern, mb, sh, ctx);

            cbp_luma   = cbp % 16;
            cbp_chroma = cbp / 16;
            mb->residuals.cbp_chroma = cbp_chroma;
            mb->residuals.cbp_luma = cbp_luma;
            meta->cbp_luma = cbp_luma;
            meta->cbp_chroma = cbp_chroma;

            if (cbp_luma > 0 && pps->transform_8x8_mode_flag &&
                !IS_INTRA4x4(type) && noSubMbPartSizeLessThan8x8 &&
                (!IS_DIRECT(type) || sps->direct_8x8_inference_flag)) {

                #if CABAC
                    #if CABAC_LOG
                        fprintf(ctx->log_file, "\nreading transform_8x8_flag\n");
                    #endif
                    int inc = (mb->has_mb_a && ctx->mb_metadata[mb->mbAddr - 1].t_8x8_flag) +
                              (mb->has_mb_b && ctx->mb_metadata[mb->mbAddr + mb->mb_b_off].t_8x8_flag);
                    mb->t_8x8_flag = cabac_get_bit(ctx, 399 + inc);
                #else
                    mb->t_8x8_flag = read_u(br, 1);
                #endif
                meta->t_8x8_flag = mb->t_8x8_flag;
            }
        }

        if (cbp_luma > 0 || cbp_chroma > 0 || IS_INTRA16x16(type)) {

            mb->mb_qp_delta = CACALL(read_mb_qp_delta, mb, sh, ctx);

            mb->QPY = mb->mbAddr == sh->first_mb
                ? _clip3(0, 51, (pps->pic_init_qp + sh->slice_qp_delta + mb->mb_qp_delta + 52) % 52)
                : _clip3(0, 51, (ctx->prevMb->QPY + mb->mb_qp_delta + 52) % 52);


            int qPi = _clip3(0, 51, mb->QPY + pps->chroma_qp_index_offset);
            mb->QPC = QPcTable[qPi];


            meta->QPY = mb->QPY;
            meta->QPC = mb->QPC;


            CACALL(read_residual, mb, type, mb->t_8x8_flag, 0, 15, cbp_luma, cbp_chroma, sh, ctx);
        } else {
            mb->QPY = mb->mbAddr == sh->first_mb
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
    SliceHeader *sh, NalUnit *nal_unit, Undo264Context *ctx) {

    BitReader *br = ctx->br;

    #if CABAC_LOG
        fprintf(ctx->log_file, "\n\nREADING SLICE %lu\n", ctx->prf->total_frames);
    #endif


    PPS *pps = sh->pps;
    SPS *sps = sh->sps;


    #if CABAC
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);
        }
        cabac_init(ctx);
    #endif


    ctx->current_slice->num_mbs = 0;

    int mbaff_frame_flag = sh->sps->mb_aff_flag;
    int currMbAddr = sh->first_mb * (1 + mbaff_frame_flag);

    int moreDataFlag = 1;
    int prevMbSkipped = 0;

    int mb_skip_flag = 0;

    do {
        if (!IS_I_SLICE(sh->slice_type) && !IS_SI_SLICE(sh->slice_type)) {
            // preprocessor trick : for CAVLC, loop 0...mb_skip_run-1, for CABAC decode one skipped macroblock at a time per slice loop
            #if CABAC
                #if CABAC_LOG
                    fprintf(ctx->log_file, "\n\nREADING MACROBLOCK %d (slice %d)\n", currMbAddr, ctx->prf->total_frames);
                #endif
                Macroblock *mb = ctx->currMb;
                reset_mb(mb, currMbAddr, ctx);
                derive_macroblock_neighbors(mb, true, sh->first_mb, ctx);
                int inc = (mb->has_mb_a && ctx->mb_metadata[currMbAddr - 1].mb_skip_flag == 0) +
                          (mb->has_mb_b && ctx->mb_metadata[currMbAddr + mb->mb_b_off].mb_skip_flag == 0);
                #if CABAC_LOG
                    fprintf(ctx->log_file, "\nreading mb_skip_flag\n");
                #endif
                mb_skip_flag = IS_P_SLICE(sh->slice_type) ? cabac_get_bit(ctx, 11+inc) : cabac_get_bit(ctx, 24+inc);
                ctx->mb_metadata[currMbAddr].mb_skip_flag = mb_skip_flag;
                moreDataFlag = !mb_skip_flag;

                if (mb_skip_flag) {
            #else
                uint32_t mb_skip_run = read_ue(br);
                prevMbSkipped = mb_skip_run > 0;
                // decode skipped macroblocks
                for (int mbAddr = currMbAddr; mbAddr < currMbAddr + mb_skip_run; mbAddr++) {
                    Macroblock *mb = ctx->currMb;
                    reset_mb(mb, mbAddr, ctx);
                    derive_macroblock_neighbors(mb, true, sh->first_mb, ctx);
            #endif
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
                    ctx->current_slice->num_mbs++;
                }

                #if !CABAC
                    currMbAddr += mb_skip_run;

                    if (mb_skip_run > 0) {
                        moreDataFlag = more_rbsp_data(br) && currMbAddr < ctx->num_mbs;
                    }
                #endif
        }
        if (moreDataFlag) {
            if (mbaff_frame_flag && (currMbAddr%2 == 0 ||
                (currMbAddr%2 == 1 && prevMbSkipped))) {

                int mb_field_decoding_flag = read_u(br, 1);
            }

            profiler_start_mb(ctx->prf);

            Macroblock *mb = ctx->currMb;

            reset_mb(mb, currMbAddr, ctx);
            derive_macroblock_neighbors(mb, true, sh->first_mb, ctx);

            CACALL(read_macroblock, mb, sh, nal_unit, ctx);
            if (IS_I_SLICE(sh->slice_type)) {
                decode_i_macroblock(mb, ctx->current_slice, ctx);
            } else if (IS_P_SLICE(sh->slice_type)) {
                decode_p_macroblock(mb, ctx->current_slice, ctx);
            } else if (IS_B_SLICE(sh->slice_type)) {
                decode_b_macroblock(mb, ctx->current_slice, ctx);
            }

            ctx->current_slice->num_mbs++;

            profiler_end_mb(ctx->prf);
        }


        if (!pps->cabac_flag) {
            moreDataFlag = more_rbsp_data(br);
        } else {
            if (!IS_I_SLICE(sh->slice_type) && !IS_SI_SLICE(sh->slice_type)) {
                prevMbSkipped = mb_skip_flag;
            }
            #if CABAC_LOG
                fprintf(ctx->log_file, "\nreading end_of_slice_flag\n");
            #endif
            int end_of_slice_flag = cabac_get_bit_term(ctx);
            moreDataFlag = !end_of_slice_flag;
        }

        if (moreDataFlag) {
            currMbAddr++;
            if (currMbAddr >= ctx->num_mbs) {
                moreDataFlag = false;
            }
        }
    } while (moreDataFlag);
}