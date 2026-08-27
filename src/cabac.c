//
// Created by gmathix on 7/16/26.
//

#include "cabac.h"

#include <stdalign.h>

#include "tests/profiler.h"
#include "util/mbutil.h"


int8_t p_state_idx[1024];
int8_t val_mps[1024];


void cabac_init_ctx_vars(Slice *slice, int idc) {
    int qpy = slice->sh->pps->pic_init_qp + slice->sh->slice_qp_delta;

    for (int idx = 0; idx < 1024; idx++) {
        int8_t m = cabac_init_vars[idc][idx][0];
        int8_t n = cabac_init_vars[idc][idx][1];

        int8_t pStateIdx, valMPS;
        int8_t preCtxState = _clip3(1, 126, ((m * _clip3(0, 51, qpy)) >> 4) + n);
        if (preCtxState <= 63) {
            pStateIdx = (int8_t) (63 - preCtxState);
            valMPS = 0;
        } else {
            pStateIdx = (int8_t) (preCtxState - 64);
            valMPS = 1;
        }

        p_state_idx[idx] = pStateIdx;
        val_mps[idx] = valMPS;
    }

    p_state_idx[276] = 63;
    val_mps[276] = 0;
}

void cabac_init(Undo264Context *ctx) {
    Slice *slice = ctx->current_slice;

    int idc = IS_I_SLICE(slice->sh->slice_type) ? 0 : slice->sh->cabac_init_idc + 1;

    cabac_init_ctx_vars(slice, idc);
    cabac_init_engine(ctx);
}

void cabac_init_engine(Undo264Context *ctx) {
    ctx->cactx->codIRange       = 510;
    ctx->cactx->codIOffset      = (int16_t) read_u(ctx->br, 9);
    ctx->cactx->p_state_idx_ptr = p_state_idx;
    ctx->cactx->val_mps_ptr     = val_mps;
}




int cabac_get_bit(Undo264Context *ctx, int ctxIdx) {
    CabacContext *cactx = ctx->cactx;


    int qCodIRangeIdx = (cactx->codIRange >> 6) & 3;
    int8_t pStateIdx = p_state_idx[ctxIdx];
    int8_t valMPS = val_mps[ctxIdx];
    int16_t codIRangeLPS = range_tab_lps[pStateIdx][qCodIRangeIdx];

    int prevrange = cactx->codIRange;
    int prevoff = cactx->codIOffset;
    int p = pStateIdx;
    int val = valMPS;

    #if CABAC_LOG
        fprintf(ctx->log_file, "ctxIdx:%d range:%d offset:%d state:%d mps:%d ", ctxIdx, prevrange, prevoff, p, val);
        fflush(ctx->log_file);
    #endif

    cactx->codIRange -= codIRangeLPS;

    int bin;

    if (cactx->codIOffset >= cactx->codIRange) {
        bin = 1 - valMPS;
        cactx->codIOffset -= cactx->codIRange;
        cactx->codIRange = codIRangeLPS;
        if (pStateIdx == 0) {
            val_mps[ctxIdx] = 1 - valMPS;
        }
        #if CABAC_LOG
            fprintf(ctx->log_file, "trans:%d ",trans_idx_lps[pStateIdx]);
            fflush(ctx->log_file);
        #endif
        p_state_idx[ctxIdx] = trans_idx_lps[pStateIdx];
    } else {
        bin = valMPS;
        #if CABAC_LOG
                fprintf(ctx->log_file, "trans:%d ",trans_idx_mps[pStateIdx]);
            fflush(ctx->log_file);
        #endif
        p_state_idx[ctxIdx] = trans_idx_mps[pStateIdx];
    }

    while (cactx->codIRange < 256) {
        cactx->codIRange <<= 1;
        cactx->codIOffset = (cactx->codIOffset << 1) | read_u(ctx->br, 1);
    }

    #if CABAC_LOG
        fprintf(ctx->log_file, "   bit:%d\n", bin);
        fflush(ctx->log_file);
    #endif



    return bin;
}

int cabac_get_bit_term(Undo264Context *ctx) {
    CabacContext *cactx = ctx->cactx;

    int prevrange = cactx->codIRange;
    int prevoff = cactx->codIOffset;

    cactx->codIRange -= 2;


    int bin;
    if (cactx->codIOffset >= cactx->codIRange) {
        bin = 1;
    } else {
        bin = 0;
        while (cactx->codIRange < 256) {
            cactx->codIRange <<= 1;
            cactx->codIOffset = (cactx->codIOffset << 1) | read_u(ctx->br, 1);
        }
    }


    #if CABAC_LOG
        // fprintf(ctx->log_file, "range:%d offset:%d   bit:%d\n", prevrange, prevoff, bin);
    #endif


    return bin;
}

int cabac_get_bit_bypass(Undo264Context *ctx) {
    CabacContext *cactx = ctx->cactx;

    #if CABAC_LOG
        fprintf(ctx->log_file, "range:%d offset:%d ", cactx->codIRange, cactx->codIOffset);
        fflush(ctx->log_file);
    #endif

    cactx->codIOffset = (cactx->codIOffset << 1) | read_u(ctx->br, 1);
    if (cactx->codIOffset >= cactx->codIRange) {
        cactx->codIOffset -= cactx->codIRange;
        #if CABAC_LOG
            fprintf(ctx->log_file, "  bit:1\n");
            fflush(ctx->log_file);
        #endif
        return 1;
    }

    #if CABAC_LOG
        fprintf(ctx->log_file, "  bit:0\n");
        fflush(ctx->log_file);
    #endif
    return 0;
}


int read_coded_block_flag(Macroblock *mb, int blkIdx, int iCbCr, BlockType blockType, Undo264Context *ctx) {
    static const int16_t ctxIdxOffset[14] = {85, 85, 85, 85, 85, 1012, 460, 460, 460, 1012, 472, 472, 472, 1012};
    static const int8_t  ctxIdxCatOffset[14] = {0, 4, 8, 12, 16, 0, 0, 4, 8, 4, 0, 4, 8, 8};

    #if CABAC_LOG
        fprintf(ctx->log_file, "\nreading coded_block_flag\n");
        fflush(ctx->log_file);
    #endif
    if (blockType == CHROMA_AC_LEVEL || blockType == LUMA_LEVEL_8x8) {
        blkIdx = map_4x4[blkIdx * 4];
    }
    Neighbors n = derive_neighbors_4x4(mb, blkIdx, ctx);
    bool blockA_av = false, blockB_av = false;

    int blockTypeA = blockType;
    int blockTypeB = blockType;

    MacroblockMetadata metaA = n.a.av ? ctx->mb_metadata[mb->mbAddr + n.a.mb_off] : ctx->mb_metadata[mb->mbAddr];
    MacroblockMetadata metaB = n.b.av ? ctx->mb_metadata[mb->mbAddr + n.b.mb_off] : ctx->mb_metadata[mb->mbAddr];
    if (blockType == LUMA_INTRA_16x16_DC_LEVEL || blockType == CB_INTRA_16x16_DC_LEVEL || blockType == CR_INTRA_16x16_DC_LEVEL) {
        blockA_av = mb->has_mb_a && IS_INTRA16x16(ctx->mb_metadata[mb->mbAddr + mb->mb_a_off].mb_type);
        blockB_av = mb->has_mb_b && IS_INTRA16x16(ctx->mb_metadata[mb->mbAddr + mb->mb_b_off].mb_type);
    } else if (blockType == LUMA_INTRA_16x16_AC_LEVEL || blockType == LUMA_LEVEL_4x4 || blockType == LUMA_LEVEL_8x8) {
        blockA_av = n.a.av && !IS_SKIP(metaA.mb_type) && !IS_PCM(metaA.mb_type) && (metaA.cbp_luma >> (map_4x4[n.a.idx] >> 2) & 1) != 0;
        blockB_av = n.b.av && !IS_SKIP(metaB.mb_type) && !IS_PCM(metaB.mb_type) && (metaB.cbp_luma >> (map_4x4[n.b.idx] >> 2) & 1) != 0;
        if (blockA_av && ctx->mb_metadata[mb->mbAddr + n.a.mb_off].t_8x8_flag) blockTypeA = LUMA_LEVEL_8x8;
        if (blockB_av && ctx->mb_metadata[mb->mbAddr + n.b.mb_off].t_8x8_flag) blockTypeB = LUMA_LEVEL_8x8;
    } else if (blockType == CHROMA_DC_LEVEL) {
        blockA_av = n.a.av && !IS_SKIP(metaA.mb_type) && !IS_PCM(metaA.mb_type) && metaA.cbp_chroma != 0;
        blockB_av = n.b.av && !IS_SKIP(metaB.mb_type) && !IS_PCM(metaB.mb_type) && metaB.cbp_chroma != 0;
    } else if (blockType == CHROMA_AC_LEVEL) {
        blockA_av = n.a.av && !IS_SKIP(metaA.mb_type) && !IS_PCM(metaA.mb_type) && metaA.cbp_chroma == 2;
        blockB_av = n.b.av && !IS_SKIP(metaB.mb_type) && !IS_PCM(metaB.mb_type) && metaB.cbp_chroma == 2;
    } else {
        // the rest belongs to 4:2:2 or 4:4:4 stuff so not planning to implement them yet
    }

    bool condTermA, condTermB;
    if ((!n.a.av && IS_INTER(mb->mb_type)) ||
        (n.a.av && !blockA_av && !IS_PCM(ctx->mb_metadata[mb->mbAddr + n.a.mb_off].mb_type))) {
        condTermA = false;
    } else if ((!n.a.av && IS_INTRA(mb->mb_type)) ||
                (IS_PCM(ctx->mb_metadata[mb->mbAddr + n.a.mb_off].mb_type))) {
        condTermA = true;
    } else {
        condTermA = ctx->mb_metadata[mb->mbAddr + n.a.mb_off].coded_block_flag[blockTypeA][n.a.idx] >> iCbCr & 1;
    }
    if ((!n.b.av && IS_INTER(mb->mb_type)) ||
        (n.b.av && !blockB_av && !IS_PCM(ctx->mb_metadata[mb->mbAddr + n.b.mb_off].mb_type))) {
        condTermB = false;
    } else if ((!n.b.av && IS_INTRA(mb->mb_type)) ||
                (IS_PCM(ctx->mb_metadata[mb->mbAddr + n.b.mb_off].mb_type))) {
        condTermB = true;
    } else {
        condTermB = ctx->mb_metadata[mb->mbAddr + n.b.mb_off].coded_block_flag[blockTypeB][n.b.idx] >> iCbCr & 1;
    }

    int ctxIdx = ctxIdxOffset[blockType] + ctxIdxCatOffset[blockType] + condTermA + 2*condTermB;

    return cabac_get_bit(ctx, ctxIdx);
}

int read_significant_coeff_flag(Macroblock *mb, int coeffIdx, BlockType blockType, Undo264Context *ctx) {
    static const int16_t ctxIdxOffset[14] = {105, 105, 105, 105, 105, 402, 484, 484, 484, 660, 528, 528, 528, 718};
    static const int8_t ctxIdxCatOffset[14] = {0, 15, 29, 44, 47, 0, 0, 15, 29, 0, 0, 15, 29, 0};
    static const alignas(64) int8_t ctxIdxInc[2][64] = {
        {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
            32, 33, 34, 35, 36 ,37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
            48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
        }, {
            0, 1, 2, 3, 4, 5, 5, 4, 4, 3, 3, 4, 4, 4, 5, 5,
            4, 4, 4, 4, 3, 3, 6, 7, 7, 7, 8, 9, 10, 9, 8, 7,
            7, 6, 11, 12, 13, 11, 6, 7, 8, 9, 14, 10, 9, 8, 6, 11,
            12, 13, 11, 6, 9, 14, 10, 9, 11, 12, 13, 11, 14, 10, 12, -1
        }
    };
    // 9.3.3.1.3 uses a computed ctxIdxInc for CHROMA_DC, but in 4:2:0 it will always equal to coeffIdx
    // so we use a separate ctxIdx table only for LUMA_LEVEL_8x8
    int ctxIdx = ctxIdxOffset[blockType] + ctxIdxCatOffset[blockType] + ctxIdxInc[blockType == LUMA_LEVEL_8x8][coeffIdx];


    #if CABAC_LOG
        fprintf(ctx->log_file, "\nreading significant_coeff_flag\n");
    #endif

    return cabac_get_bit(ctx, ctxIdx);
}

int read_last_significant_coeff_flag(Macroblock *mb, int coeffIdx, BlockType blockType, Undo264Context *ctx) {
    static const int16_t ctxIdxOffset[14] = {166, 166, 166, 166, 166, 417, 572, 572, 572, 690, 616, 616, 616, 748};
    static const int8_t ctxIdxCatOffset[14] = {0, 15, 29, 44, 47, 0, 0, 15, 29, 0, 0, 15, 29, 0};
    static const alignas(64) int8_t ctxIdxInc[2][64] = {
        {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
            32, 33, 34, 35, 36 ,37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
            48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
        }, {
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8
        }
    };

    int ctxIdx = ctxIdxOffset[blockType] + ctxIdxCatOffset[blockType] + ctxIdxInc[blockType == LUMA_LEVEL_8x8][coeffIdx];

    #if CABAC_LOG
        fprintf(ctx->log_file, "\nreading last_significant_coeff_flag\n");
    #endif

    return cabac_get_bit(ctx, ctxIdx);
}

int read_coeff_abs_level(Macroblock *mb, int coeffIdx, int numDecodAbsEq1, int numDecodAbsGt1, BlockType blockType, Undo264Context *ctx) {
    static const int16_t ctxIdxOffset[14] = {227, 227, 227, 227, 227, 426, 952, 952, 952, 708, 982, 982, 982, 766}; // for prefix
    static const int8_t ctxIdxCatOffset[14] = {0, 10, 20, 30, 39, 0, 0, 10, 20, 0, 0, 10, 20, 0};

    int ctxIdx = ctxIdxOffset[blockType] + ctxIdxCatOffset[blockType];

    #if CABAC_LOG
        fprintf(ctx->log_file, "\ndecoding coeff_abs_level\n");
    #endif

    int val = 0;
    long str = 0;
    int inc0 = (numDecodAbsGt1 == 0) * _min(4, 1 + numDecodAbsEq1);
    int inc1 = 5 + _min(4 - (blockType == CHROMA_DC_LEVEL), numDecodAbsGt1);
    while (str += str + cabac_get_bit(ctx, ctxIdx + (val == 0)*inc0 + (val > 0)*inc1),
           (str & 1) && str != 0x3FFF) {
        val++;
    }
    if (str == 0x3FFF) {
        val = 14;

        int k = 0;
        int bin;
        int symbol = 0, bin_symbol = 0;

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
        val += symbol + bin_symbol;

        return val + 1;
    } else {
        return val + 1;
    }
}

int read_coeff_sign_flag(Macroblock *mb, int blkIdx, BlockType blockType, Undo264Context *ctx) {
    // uses bypass, no ctxIdx to compute
    #if CABAC_LOG
        fprintf(ctx->log_file, "\nreading coeff_sign_flag\n");
    #endif
    return cabac_get_bit_bypass(ctx);
}

void residual_block_cabac  (Macroblock *mb, int blkIdx, int iCbCr, BlockType blockType, int16_t *coeffLevel,
    int startIdx, int endIdx, int maxNumCoeff, bool isLuma, struct SliceHeader *sh, Undo264Context *ctx) {



    int codedBlockFlag = 1;
    if (maxNumCoeff != 64) {
        codedBlockFlag = read_coded_block_flag(mb, blkIdx, iCbCr, blockType, ctx);
    }
    #if CABAC_LOG
        fprintf(ctx->log_file, "%s\n", blockType_to_string(blockType));
    #endif


    // store coded_block_flag
    if (blockType == LUMA_INTRA_16x16_DC_LEVEL || blockType == CB_INTRA_16x16_DC_LEVEL || blockType == CR_INTRA_16x16_DC_LEVEL) {
        memset(ctx->mb_metadata[mb->mbAddr].coded_block_flag[blockType], codedBlockFlag, 16);
        if (blockType == LUMA_INTRA_16x16_DC_LEVEL) memset(&ctx->total_coeffs[mb->mbAddr][0], codedBlockFlag, 16);
    } else if (blockType == LUMA_INTRA_16x16_AC_LEVEL || blockType == LUMA_LEVEL_4x4) {
        ctx->mb_metadata[mb->mbAddr].coded_block_flag[LUMA_INTRA_16x16_AC_LEVEL][blkIdx] = codedBlockFlag;
        ctx->mb_metadata[mb->mbAddr].coded_block_flag[LUMA_LEVEL_4x4][blkIdx] = codedBlockFlag;

        // exclusively for the deblocking filter here, just say that this block has nonzero coeffs or not
        if (blockType == LUMA_LEVEL_4x4) ctx->total_coeffs[mb->mbAddr][blkIdx] = codedBlockFlag;
    } else if (blockType == CHROMA_DC_LEVEL) {
        for (int i = 0; i < 16; i++) {
            ctx->mb_metadata[mb->mbAddr].coded_block_flag[blockType][i] |= codedBlockFlag << iCbCr;
        }
    } else if (blockType == CHROMA_AC_LEVEL || blockType == LUMA_LEVEL_8x8) {
        for (int i = 0; i < 4; i++) {
            ctx->mb_metadata[mb->mbAddr].coded_block_flag[blockType][map_4x4[blkIdx*4] + i + (i >= 2)*2] |= codedBlockFlag << iCbCr;
            if (blockType == LUMA_LEVEL_8x8) ctx->total_coeffs[mb->mbAddr][map_4x4[blkIdx*4] + i + (i >= 2)*2] = codedBlockFlag;
        }
    } else {
        // the rest belongs to 4:2:2 or 4:4:4 stuff so not planning to implement them yet
    }
    memset(coeffLevel, 0, maxNumCoeff * sizeof(int16_t));

    // fine as long as the decoder is not multithreaded
    static int8_t  significant_coeff_flag[64];
    static int8_t  last_significant_coeff_flag[64];
    static int16_t coeff_abs_level[64];
    static int8_t  coeff_sign_flag[64];

    int numDecodAbsEq1 = 0;
    int numDecodAbsGt1 = 0;

    debugging = 1;

    if (codedBlockFlag) {
        int numCoeff = endIdx + 1;
        int i = startIdx;
        while (i < numCoeff - 1) {
            significant_coeff_flag[i] = read_significant_coeff_flag(mb, i, blockType, ctx);
            if (significant_coeff_flag[i]) {
                last_significant_coeff_flag[i] = read_last_significant_coeff_flag(mb, i, blockType, ctx);
                if (last_significant_coeff_flag[i]) {
                    numCoeff = i + 1;
                }
            }
            i++;
        }

        coeff_abs_level[numCoeff - 1] = read_coeff_abs_level(mb, blkIdx, numDecodAbsEq1, numDecodAbsGt1, blockType, ctx);
        coeff_sign_flag[numCoeff - 1] = read_coeff_sign_flag(mb, blkIdx, blockType, ctx);
        coeffLevel[numCoeff - 1] = coeff_abs_level[numCoeff - 1] * (1 - 2*coeff_sign_flag[numCoeff - 1]);
        numDecodAbsEq1 += coeff_abs_level[numCoeff-1] == 1;
        numDecodAbsGt1 += coeff_abs_level[numCoeff-1] > 1;
        for (int i = numCoeff - 2; i >= startIdx; i--) {
            if (significant_coeff_flag[i]) {
                coeff_abs_level[i] = read_coeff_abs_level(mb, blkIdx, numDecodAbsEq1, numDecodAbsGt1, blockType, ctx);
                coeff_sign_flag[i] = read_coeff_sign_flag(mb, blkIdx, blockType, ctx);
                coeffLevel[i] = coeff_abs_level[i] * (1 - 2*coeff_sign_flag[i]);
                numDecodAbsEq1 += coeff_abs_level[i] == 1;
                numDecodAbsGt1 += coeff_abs_level[i] > 1;
            }
        }
    }
}





// tables 9-12 to 9-33
// [I_slice / 1+cabac_init_idc][ctxIdx][m,n]
// the fucking typo at ctxIdx 541 written as 641 made my python parser shit himself (thanks h264 committee)
const int8_t cabac_init_vars[4][1024][2] = {
    { // I and SI slices
        {  20, -15}, {   2,  54}, {   3,  74}, {  20, -15}, {   2,  54}, {   3,  74}, { -28, 127}, { -23, 104},
        {  -6,  53}, {  -1,  54}, {   7,  51}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
        {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
        {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
        {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
        {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
        {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
        {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,  41}, {   0,  63}, {   0,  63}, {   0,  63},
        {  -9,  83}, {   4,  86}, {   0,  97}, {  -7,  72}, {  13,  41}, {   3,  62}, {   0,  11}, {   1,  55},
        {   0,  69}, { -17, 127}, { -13, 102}, {   0,  82}, {  -7,  74}, { -21, 107}, { -27, 127}, { -31, 127},
        { -24, 127}, { -18,  95}, { -27, 127}, { -21, 114}, { -30, 127}, { -17, 123}, { -12, 115}, { -16, 122},
        { -11, 115}, { -12,  63}, {  -2,  68}, { -15,  84}, { -13, 104}, {  -3,  70}, {  -8,  93}, { -10,  90},
        { -30, 127}, {  -1,  74}, {  -6,  97}, {  -7,  91}, { -20, 127}, {  -4,  56}, {  -5,  82}, {  -7,  76},
        { -22, 125}, {  -7,  93}, { -11,  87}, {  -3,  77}, {  -5,  71}, {  -4,  63}, {  -4,  68}, { -12,  84},
        {  -7,  62}, {  -7,  65}, {   8,  61}, {   5,  56}, {  -2,  66}, {   1,  64}, {   0,  61}, {  -2,  78},
        {   1,  50}, {   7,  52}, {  10,  35}, {   0,  44}, {  11,  38}, {   1,  45}, {   0,  46}, {   5,  44},
        {  31,  17}, {   1,  51}, {   7,  50}, {  28,  19}, {  16,  33}, {  14,  62}, { -13, 108}, { -15, 100},
        { -13, 101}, { -13,  91}, { -12,  94}, { -10,  88}, { -16,  84}, { -10,  86}, {  -7,  83}, { -13,  87},
        { -19,  94}, {   1,  70}, {   0,  72}, {  -5,  74}, {  18,  59}, {  -8, 102}, { -15, 100}, {   0,  95},
        {  -4,  75}, {   2,  72}, { -11,  75}, {  -3,  71}, {  15,  46}, { -13,  69}, {   0,  62}, {   0,  65},
        {  21,  37}, { -15,  72}, {   9,  57}, {  16,  54}, {   0,  62}, {  12,  72}, {  24,   0}, {  15,   9},
        {   8,  25}, {  13,  18}, {  15,   9}, {  13,  19}, {  10,  37}, {  12,  18}, {   6,  29}, {  20,  33},
        {  15,  30}, {   4,  45}, {   1,  58}, {   0,  62}, {   7,  61}, {  12,  38}, {  11,  45}, {  15,  39},
        {  11,  42}, {  13,  44}, {  16,  45}, {  12,  41}, {  10,  49}, {  30,  34}, {  18,  42}, {  10,  55},
        {  17,  51}, {  17,  46}, {   0,  89}, {  26, -19}, {  22, -17}, {  26, -17}, {  30, -25}, {  28, -20},
        {  33, -23}, {  37, -27}, {  33, -23}, {  40, -28}, {  38, -17}, {  33, -11}, {  40, -15}, {  41,  -6},
        {  38,   1}, {  41,  17}, {  30,  -6}, {  27,   3}, {  26,  22}, {  37, -16}, {  35,  -4}, {  38,  -8},
        {  38,  -3}, {  37,   3}, {  38,   5}, {  42,   0}, {  35,  16}, {  39,  22}, {  14,  48}, {  27,  37},
        {  21,  60}, {  12,  68}, {   2,  97}, {  -3,  71}, {  -6,  42}, {  -5,  50}, {  -3,  54}, {  -2,  62},
        {   0,  58}, {   1,  63}, {  -2,  72}, {  -1,  74}, {  -9,  91}, {  -5,  67}, {  -5,  27}, {  -3,  39},
        {  -2,  44}, {   0,  46}, { -16,  64}, {  -8,  68}, { -10,  78}, {  -6,  77}, { -10,  86}, { -12,  92},
        { -15,  55}, { -10,  60}, {  -6,  62}, {  -4,  65}, { -12,  73}, {  -8,  76}, {  -7,  80}, {  -9,  88},
        { -17, 110}, { -11,  97}, { -20,  84}, { -11,  79}, {  -6,  73}, {  -4,  74}, { -13,  86}, { -13,  96},
        { -11,  97}, { -19, 117}, {  -8,  78}, {  -5,  33}, {  -4,  48}, {  -2,  53}, {  -3,  62}, { -13,  71},
        { -10,  79}, { -12,  86}, { -13,  90}, { -14,  97}, {   0,   0}, {  -6,  93}, {  -6,  84}, {  -8,  79},
        {   0,  66}, {  -1,  71}, {   0,  62}, {  -2,  60}, {  -2,  59}, {  -5,  75}, {  -3,  62}, {  -4,  58},
        {  -9,  66}, {  -1,  79}, {   0,  71}, {   3,  68}, {  10,  44}, {  -7,  62}, {  15,  36}, {  14,  40},
        {  16,  27}, {  12,  29}, {   1,  44}, {  20,  36}, {  18,  32}, {   5,  42}, {   1,  48}, {  10,  62},
        {  17,  46}, {   9,  64}, { -12, 104}, { -11,  97}, { -16,  96}, {  -7,  88}, {  -8,  85}, {  -7,  85},
        {  -9,  85}, { -13,  88}, {   4,  66}, {  -3,  77}, {  -3,  76}, {  -6,  76}, {  10,  58}, {  -1,  76},
        {  -1,  83}, {  -7,  99}, { -14,  95}, {   2,  95}, {   0,  76}, {  -5,  74}, {   0,  70}, { -11,  75},
        {   1,  68}, {   0,  65}, { -14,  73}, {   3,  62}, {   4,  62}, {  -1,  68}, { -13,  75}, {  11,  55},
        {   5,  64}, {  12,  70}, {  15,   6}, {   6,  19}, {   7,  16}, {  12,  14}, {  18,  13}, {  13,  11},
        {  13,  15}, {  15,  16}, {  12,  23}, {  13,  23}, {  15,  20}, {  14,  26}, {  14,  44}, {  17,  40},
        {  17,  47}, {  24,  17}, {  21,  21}, {  25,  22}, {  31,  27}, {  22,  29}, {  19,  35}, {  14,  50},
        {  10,  57}, {   7,  63}, {  -2,  77}, {  -4,  82}, {  -3,  94}, {   9,  69}, { -12, 109}, {  36, -35},
        {  36, -34}, {  32, -26}, {  37, -30}, {  44, -32}, {  34, -18}, {  34, -15}, {  40, -15}, {  33,  -7},
        {  35,  -5}, {  33,   0}, {  38,   2}, {  33,  13}, {  23,  35}, {  13,  58}, {  29,  -3}, {  26,   0},
        {  22,  30}, {  31,  -7}, {  35, -15}, {  34,  -3}, {  34,   3}, {  36,  -1}, {  34,   5}, {  32,  11},
        {  35,   5}, {  34,  12}, {  39,  11}, {  30,  29}, {  34,  26}, {  29,  39}, {  19,  66}, {  31,  21},
        {  31,  31}, {  25,  50}, { -17, 120}, { -20, 112}, { -18, 114}, { -11,  85}, { -15,  92}, { -14,  89},
        { -26,  71}, { -15,  81}, { -14,  80}, {   0,  68}, { -14,  70}, { -24,  56}, { -23,  68}, { -24,  50},
        { -11,  74}, {  23, -13}, {  26, -13}, {  40, -15}, {  49, -14}, {  44,   3}, {  45,   6}, {  44,  34},
        {  33,  54}, {  19,  82}, {  -3,  75}, {  -1,  23}, {   1,  34}, {   1,  43}, {   0,  54}, {  -2,  55},
        {   0,  61}, {   1,  64}, {   0,  68}, {  -9,  92}, { -14, 106}, { -13,  97}, { -15,  90}, { -12,  90},
        { -18,  88}, { -10,  73}, {  -9,  79}, { -14,  86}, { -10,  73}, { -10,  70}, { -10,  69}, {  -5,  66},
        {  -9,  64}, {  -5,  58}, {   2,  59}, {  21, -10}, {  24, -11}, {  28,  -8}, {  28,  -1}, {  29,   3},
        {  29,   9}, {  35,  20}, {  29,  36}, {  14,  67}, { -17, 123}, { -12, 115}, { -16, 122}, { -11, 115},
        { -12,  63}, {  -2,  68}, { -15,  84}, { -13, 104}, {  -3,  70}, {  -8,  93}, { -10,  90}, { -30, 127},
        { -17, 123}, { -12, 115}, { -16, 122}, { -11, 115}, { -12,  63}, {  -2,  68}, { -15,  84}, { -13, 104},
        {  -3,  70}, {  -8,  93}, { -10,  90}, { -30, 127}, {  -7,  93}, { -11,  87}, {  -3,  77}, {  -5,  71},
        {  -4,  63}, {  -4,  68}, { -12,  84}, {  -7,  62}, {  -7,  65}, {   8,  61}, {   5,  56}, {  -2,  66},
        {   1,  64}, {   0,  61}, {  -2,  78}, {   1,  50}, {   7,  52}, {  10,  35}, {   0,  44}, {  11,  38},
        {   1,  45}, {   0,  46}, {   5,  44}, {  31,  17}, {   1,  51}, {   7,  50}, {  28,  19}, {  16,  33},
        {  14,  62}, { -13, 108}, { -15, 100}, { -13, 101}, { -13,  91}, { -12,  94}, { -10,  88}, { -16,  84},
        { -10,  86}, {  -7,  83}, { -13,  87}, { -19,  94}, {   1,  70}, {   0,  72}, {  -5,  74}, {  18,  59},
        {  -7,  93}, { -11,  87}, {  -3,  77}, {  -5,  71}, {  -4,  63}, {  -4,  68}, { -12,  84}, {  -7,  62},
        {  -7,  65}, {   8,  61}, {   5,  56}, {  -2,  66}, {   1,  64}, {   0,  61}, {  -2,  78}, {   1,  50},
        {   7,  52}, {  10,  35}, {   0,  44}, {  11,  38}, {   1,  45}, {   0,  46}, {   5,  44}, {  31,  17},
        {   1,  51}, {   7,  50}, {  28,  19}, {  16,  33}, {  14,  62}, { -13, 108}, { -15, 100}, { -13, 101},
        { -13,  91}, { -12,  94}, { -10,  88}, { -16,  84}, { -10,  86}, {  -7,  83}, { -13,  87}, { -19,  94},
        {   1,  70}, {   0,  72}, {  -5,  74}, {  18,  59}, {  24,   0}, {  15,   9}, {   8,  25}, {  13,  18},
        {  15,   9}, {  13,  19}, {  10,  37}, {  12,  18}, {   6,  29}, {  20,  33}, {  15,  30}, {   4,  45},
        {   1,  58}, {   0,  62}, {   7,  61}, {  12,  38}, {  11,  45}, {  15,  39}, {  11,  42}, {  13,  44},
        {  16,  45}, {  12,  41}, {  10,  49}, {  30,  34}, {  18,  42}, {  10,  55}, {  17,  51}, {  17,  46},
        {   0,  89}, {  26, -19}, {  22, -17}, {  26, -17}, {  30, -25}, {  28, -20}, {  33, -23}, {  37, -27},
        {  33, -23}, {  40, -28}, {  38, -17}, {  33, -11}, {  40, -15}, {  41,  -6}, {  38,   1}, {  41,  17},
        {  24,   0}, {  15,   9}, {   8,  25}, {  13,  18}, {  15,   9}, {  13,  19}, {  10,  37}, {  12,  18},
        {   6,  29}, {  20,  33}, {  15,  30}, {   4,  45}, {   1,  58}, {   0,  62}, {   7,  61}, {  12,  38},
        {  11,  45}, {  15,  39}, {  11,  42}, {  13,  44}, {  16,  45}, {  12,  41}, {  10,  49}, {  30,  34},
        {  18,  42}, {  10,  55}, {  17,  51}, {  17,  46}, {   0,  89}, {  26, -19}, {  22, -17}, {  26, -17},
        {  30, -25}, {  28, -20}, {  33, -23}, {  37, -27}, {  33, -23}, {  40, -28}, {  38, -17}, {  33, -11},
        {  40, -15}, {  41,  -6}, {  38,   1}, {  41,  17}, { -17, 120}, { -20, 112}, { -18, 114}, { -11,  85},
        { -15,  92}, { -14,  89}, { -26,  71}, { -15,  81}, { -14,  80}, {   0,  68}, { -14,  70}, { -24,  56},
        { -23,  68}, { -24,  50}, { -11,  74}, { -14, 106}, { -13,  97}, { -15,  90}, { -12,  90}, { -18,  88},
        { -10,  73}, {  -9,  79}, { -14,  86}, { -10,  73}, { -10,  70}, { -10,  69}, {  -5,  66}, {  -9,  64},
        {  -5,  58}, {   2,  59}, {  23, -13}, {  26, -13}, {  40, -15}, {  49, -14}, {  44,   3}, {  45,   6},
        {  44,  34}, {  33,  54}, {  19,  82}, {  21, -10}, {  24, -11}, {  28,  -8}, {  28,  -1}, {  29,   3},
        {  29,   9}, {  35,  20}, {  29,  36}, {  14,  67}, {  -3,  75}, {  -1,  23}, {   1,  34}, {   1,  43},
        {   0,  54}, {  -2,  55}, {   0,  61}, {   1,  64}, {   0,  68}, {  -9,  92}, { -17, 120}, { -20, 112},
        { -18, 114}, { -11,  85}, { -15,  92}, { -14,  89}, { -26,  71}, { -15,  81}, { -14,  80}, {   0,  68},
        { -14,  70}, { -24,  56}, { -23,  68}, { -24,  50}, { -11,  74}, { -14, 106}, { -13,  97}, { -15,  90},
        { -12,  90}, { -18,  88}, { -10,  73}, {  -9,  79}, { -14,  86}, { -10,  73}, { -10,  70}, { -10,  69},
        {  -5,  66}, {  -9,  64}, {  -5,  58}, {   2,  59}, {  23, -13}, {  26, -13}, {  40, -15}, {  49, -14},
        {  44,   3}, {  45,   6}, {  44,  34}, {  33,  54}, {  19,  82}, {  21, -10}, {  24, -11}, {  28,  -8},
        {  28,  -1}, {  29,   3}, {  29,   9}, {  35,  20}, {  29,  36}, {  14,  67}, {  -3,  75}, {  -1,  23},
        {   1,  34}, {   1,  43}, {   0,  54}, {  -2,  55}, {   0,  61}, {   1,  64}, {   0,  68}, {  -9,  92},
        {  -6,  93}, {  -6,  84}, {  -8,  79}, {   0,  66}, {  -1,  71}, {   0,  62}, {  -2,  60}, {  -2,  59},
        {  -5,  75}, {  -3,  62}, {  -4,  58}, {  -9,  66}, {  -1,  79}, {   0,  71}, {   3,  68}, {  10,  44},
        {  -7,  62}, {  15,  36}, {  14,  40}, {  16,  27}, {  12,  29}, {   1,  44}, {  20,  36}, {  18,  32},
        {   5,  42}, {   1,  48}, {  10,  62}, {  17,  46}, {   9,  64}, { -12, 104}, { -11,  97}, { -16,  96},
        {  -7,  88}, {  -8,  85}, {  -7,  85}, {  -9,  85}, { -13,  88}, {   4,  66}, {  -3,  77}, {  -3,  76},
        {  -6,  76}, {  10,  58}, {  -1,  76}, {  -1,  83}, {  -6,  93}, {  -6,  84}, {  -8,  79}, {   0,  66},
        {  -1,  71}, {   0,  62}, {  -2,  60}, {  -2,  59}, {  -5,  75}, {  -3,  62}, {  -4,  58}, {  -9,  66},
        {  -1,  79}, {   0,  71}, {   3,  68}, {  10,  44}, {  -7,  62}, {  15,  36}, {  14,  40}, {  16,  27},
        {  12,  29}, {   1,  44}, {  20,  36}, {  18,  32}, {   5,  42}, {   1,  48}, {  10,  62}, {  17,  46},
        {   9,  64}, { -12, 104}, { -11,  97}, { -16,  96}, {  -7,  88}, {  -8,  85}, {  -7,  85}, {  -9,  85},
        { -13,  88}, {   4,  66}, {  -3,  77}, {  -3,  76}, {  -6,  76}, {  10,  58}, {  -1,  76}, {  -1,  83},
        {  15,   6}, {   6,  19}, {   7,  16}, {  12,  14}, {  18,  13}, {  13,  11}, {  13,  15}, {  15,  16},
        {  12,  23}, {  13,  23}, {  15,  20}, {  14,  26}, {  14,  44}, {  17,  40}, {  17,  47}, {  24,  17},
        {  21,  21}, {  25,  22}, {  31,  27}, {  22,  29}, {  19,  35}, {  14,  50}, {  10,  57}, {   7,  63},
        {  -2,  77}, {  -4,  82}, {  -3,  94}, {   9,  69}, { -12, 109}, {  36, -35}, {  36, -34}, {  32, -26},
        {  37, -30}, {  44, -32}, {  34, -18}, {  34, -15}, {  40, -15}, {  33,  -7}, {  35,  -5}, {  33,   0},
        {  38,   2}, {  33,  13}, {  23,  35}, {  13,  58}, {  15,   6}, {   6,  19}, {   7,  16}, {  12,  14},
        {  18,  13}, {  13,  11}, {  13,  15}, {  15,  16}, {  12,  23}, {  13,  23}, {  15,  20}, {  14,  26},
        {  14,  44}, {  17,  40}, {  17,  47}, {  24,  17}, {  21,  21}, {  25,  22}, {  31,  27}, {  22,  29},
        {  19,  35}, {  14,  50}, {  10,  57}, {   7,  63}, {  -2,  77}, {  -4,  82}, {  -3,  94}, {   9,  69},
        { -12, 109}, {  36, -35}, {  36, -34}, {  32, -26}, {  37, -30}, {  44, -32}, {  34, -18}, {  34, -15},
        {  40, -15}, {  33,  -7}, {  35,  -5}, {  33,   0}, {  38,   2}, {  33,  13}, {  23,  35}, {  13,  58},
        {  -3,  71}, {  -6,  42}, {  -5,  50}, {  -3,  54}, {  -2,  62}, {   0,  58}, {   1,  63}, {  -2,  72},
        {  -1,  74}, {  -9,  91}, {  -5,  67}, {  -5,  27}, {  -3,  39}, {  -2,  44}, {   0,  46}, { -16,  64},
        {  -8,  68}, { -10,  78}, {  -6,  77}, { -10,  86}, { -12,  92}, { -15,  55}, { -10,  60}, {  -6,  62},
        {  -4,  65}, { -12,  73}, {  -8,  76}, {  -7,  80}, {  -9,  88}, { -17, 110}, {  -3,  71}, {  -6,  42},
        {  -5,  50}, {  -3,  54}, {  -2,  62}, {   0,  58}, {   1,  63}, {  -2,  72}, {  -1,  74}, {  -9,  91},
        {  -5,  67}, {  -5,  27}, {  -3,  39}, {  -2,  44}, {   0,  46}, { -16,  64}, {  -8,  68}, { -10,  78},
        {  -6,  77}, { -10,  86}, { -12,  92}, { -15,  55}, { -10,  60}, {  -6,  62}, {  -4,  65}, { -12,  73},
        {  -8,  76}, {  -7,  80}, {  -9,  88}, { -17, 110}, {  -3,  70}, {  -8,  93}, { -10,  90}, { -30, 127},
        {  -3,  70}, {  -8,  93}, { -10,  90}, { -30, 127}, {  -3,  70}, {  -8,  93}, { -10,  90}, { -30, 127},
    },
    { // cabac_init_idc = 0
        {  20, -15}, {   2,  54}, {   3,  74}, {  20, -15}, {   2,  54}, {   3,  74}, { -28, 127}, { -23, 104},
        {  -6,  53}, {  -1,  54}, {   7,  51}, {  23,  33}, {  23,   2}, {  21,   0}, {   1,   9}, {   0,  49},
        { -37, 118}, {   5,  57}, { -13,  78}, { -11,  65}, {   1,  62}, {  12,  49}, {  -4,  73}, {  17,  50},
        {  18,  64}, {   9,  43}, {  29,   0}, {  26,  67}, {  16,  90}, {   9, 104}, { -46, 127}, { -20, 104},
        {   1,  67}, { -13,  78}, { -11,  65}, {   1,  62}, {  -6,  86}, { -17,  95}, {  -6,  61}, {   9,  45},
        {  -3,  69}, {  -6,  81}, { -11,  96}, {   6,  55}, {   7,  67}, {  -5,  86}, {   2,  88}, {   0,  58},
        {  -3,  76}, { -10,  94}, {   5,  54}, {   4,  69}, {  -3,  81}, {   0,  88}, {  -7,  67}, {  -5,  74},
        {  -4,  74}, {  -5,  80}, {  -7,  72}, {   1,  58}, {   0,  41}, {   0,  63}, {   0,  63}, {   0,  63},
        {  -9,  83}, {   4,  86}, {   0,  97}, {  -7,  72}, {  13,  41}, {   3,  62}, {   0,  45}, {  -4,  78},
        {  -3,  96}, { -27, 126}, { -28,  98}, { -25, 101}, { -23,  67}, { -28,  82}, { -20,  94}, { -16,  83},
        { -22, 110}, { -21,  91}, { -18, 102}, { -13,  93}, { -29, 127}, {  -7,  92}, {  -5,  89}, {  -7,  96},
        { -13, 108}, {  -3,  46}, {  -1,  65}, {  -1,  57}, {  -9,  93}, {  -3,  74}, {  -9,  92}, {  -8,  87},
        { -23, 126}, {   5,  54}, {   6,  60}, {   6,  59}, {   6,  69}, {  -1,  48}, {   0,  68}, {  -4,  69},
        {  -8,  88}, {  -2,  85}, {  -6,  78}, {  -1,  75}, {  -7,  77}, {   2,  54}, {   5,  50}, {  -3,  68},
        {   1,  50}, {   6,  42}, {  -4,  81}, {   1,  63}, {  -4,  70}, {   0,  67}, {   2,  57}, {  -2,  76},
        {  11,  35}, {   4,  64}, {   1,  61}, {  11,  35}, {  18,  25}, {  12,  24}, {  13,  29}, {  13,  36},
        { -10,  93}, {  -7,  73}, {  -2,  73}, {  13,  46}, {   9,  49}, {  -7, 100}, {   9,  53}, {   2,  53},
        {   5,  53}, {  -2,  61}, {   0,  56}, {   0,  56}, { -13,  63}, {  -5,  60}, {  -1,  62}, {   4,  57},
        {  -6,  69}, {   4,  57}, {  14,  39}, {   4,  51}, {  13,  68}, {   3,  64}, {   1,  61}, {   9,  63},
        {   7,  50}, {  16,  39}, {   5,  44}, {   4,  52}, {  11,  48}, {  -5,  60}, {  -1,  59}, {   0,  59},
        {  22,  33}, {   5,  44}, {  14,  43}, {  -1,  78}, {   0,  60}, {   9,  69}, {  11,  28}, {   2,  40},
        {   3,  44}, {   0,  49}, {   0,  46}, {   2,  44}, {   2,  51}, {   0,  47}, {   4,  39}, {   2,  62},
        {   6,  46}, {   0,  54}, {   3,  54}, {   2,  58}, {   4,  63}, {   6,  51}, {   6,  57}, {   7,  53},
        {   6,  52}, {   6,  55}, {  11,  45}, {  14,  36}, {   8,  53}, {  -1,  82}, {   7,  55}, {  -3,  78},
        {  15,  46}, {  22,  31}, {  -1,  84}, {  25,   7}, {  30,  -7}, {  28,   3}, {  28,   4}, {  32,   0},
        {  34,  -1}, {  30,   6}, {  30,   6}, {  32,   9}, {  31,  19}, {  26,  27}, {  26,  30}, {  37,  20},
        {  28,  34}, {  17,  70}, {   1,  67}, {   5,  59}, {   9,  67}, {  16,  30}, {  18,  32}, {  18,  35},
        {  22,  29}, {  24,  31}, {  23,  38}, {  18,  43}, {  20,  41}, {  11,  63}, {   9,  59}, {   9,  64},
        {  -1,  94}, {  -2,  89}, {  -9, 108}, {  -6,  76}, {  -2,  44}, {   0,  45}, {   0,  52}, {  -3,  64},
        {  -2,  59}, {  -4,  70}, {  -4,  75}, {  -8,  82}, { -17, 102}, {  -9,  77}, {   3,  24}, {   0,  42},
        {   0,  48}, {   0,  55}, {  -6,  59}, {  -7,  71}, { -12,  83}, { -11,  87}, { -30, 119}, {   1,  58},
        {  -3,  29}, {  -1,  36}, {   1,  38}, {   2,  43}, {  -6,  55}, {   0,  58}, {   0,  64}, {  -3,  74},
        { -10,  90}, {   0,  70}, {  -4,  29}, {   5,  31}, {   7,  42}, {   1,  59}, {  -2,  58}, {  -3,  72},
        {  -3,  81}, { -11,  97}, {   0,  58}, {   8,   5}, {  10,  14}, {  14,  18}, {  13,  27}, {   2,  40},
        {   0,  58}, {  -3,  70}, {  -6,  79}, {  -8,  85}, {   0,   0}, { -13, 106}, { -16, 106}, { -10,  87},
        { -21, 114}, { -18, 110}, { -14,  98}, { -22, 110}, { -21, 106}, { -18, 103}, { -21, 107}, { -23, 108},
        { -26, 112}, { -10,  96}, { -12,  95}, {  -5,  91}, {  -9,  93}, { -22,  94}, {  -5,  86}, {   9,  67},
        {  -4,  80}, { -10,  85}, {  -1,  70}, {   7,  60}, {   9,  58}, {   5,  61}, {  12,  50}, {  15,  50},
        {  18,  49}, {  17,  54}, {  10,  41}, {   7,  46}, {  -1,  51}, {   7,  49}, {   8,  52}, {   9,  41},
        {   6,  47}, {   2,  55}, {  13,  41}, {  10,  44}, {   6,  50}, {   5,  53}, {  13,  49}, {   4,  63},
        {   6,  64}, {  -2,  69}, {  -2,  59}, {   6,  70}, {  10,  44}, {   9,  31}, {  12,  43}, {   3,  53},
        {  14,  34}, {  10,  38}, {  -3,  52}, {  13,  40}, {  17,  32}, {   7,  44}, {   7,  38}, {  13,  50},
        {  10,  57}, {  26,  43}, {  14,  11}, {  11,  14}, {   9,  11}, {  18,  11}, {  21,   9}, {  23,  -2},
        {  32, -15}, {  32, -15}, {  34, -21}, {  39, -23}, {  42, -33}, {  41, -31}, {  46, -28}, {  38, -12},
        {  21,  29}, {  45, -24}, {  53, -45}, {  48, -26}, {  65, -43}, {  43, -19}, {  39, -10}, {  30,   9},
        {  18,  26}, {  20,  27}, {   0,  57}, { -14,  82}, {  -5,  75}, { -19,  97}, { -35, 125}, {  27,   0},
        {  28,   0}, {  31,  -4}, {  27,   6}, {  34,   8}, {  30,  10}, {  24,  22}, {  33,  19}, {  22,  32},
        {  26,  31}, {  21,  41}, {  26,  44}, {  23,  47}, {  16,  65}, {  14,  71}, {   8,  60}, {   6,  63},
        {  17,  65}, {  21,  24}, {  23,  20}, {  26,  23}, {  27,  32}, {  28,  23}, {  28,  24}, {  23,  40},
        {  24,  32}, {  28,  29}, {  23,  42}, {  19,  57}, {  22,  53}, {  22,  61}, {  11,  86}, {  12,  40},
        {  11,  51}, {  14,  59}, {  -4,  79}, {  -7,  71}, {  -5,  69}, {  -9,  70}, {  -8,  66}, { -10,  68},
        { -19,  73}, { -12,  69}, { -16,  70}, { -15,  67}, { -20,  62}, { -19,  70}, { -16,  66}, { -22,  65},
        { -20,  63}, {   9,  -2}, {  26,  -9}, {  33,  -9}, {  39,  -7}, {  41,  -2}, {  45,   3}, {  49,   9},
        {  45,  27}, {  36,  59}, {  -6,  66}, {  -7,  35}, {  -7,  42}, {  -8,  45}, {  -5,  48}, { -12,  56},
        {  -6,  60}, {  -5,  62}, {  -8,  66}, {  -8,  76}, {  -5,  85}, {  -6,  81}, { -10,  77}, {  -7,  81},
        { -17,  80}, { -18,  73}, {  -4,  74}, { -10,  83}, {  -9,  71}, {  -9,  67}, {  -1,  61}, {  -8,  66},
        { -14,  66}, {   0,  59}, {   2,  59}, {  21, -13}, {  33, -14}, {  39,  -7}, {  46,  -2}, {  51,   2},
        {  60,   6}, {  61,  17}, {  55,  34}, {  42,  62}, {  -7,  92}, {  -5,  89}, {  -7,  96}, { -13, 108},
        {  -3,  46}, {  -1,  65}, {  -1,  57}, {  -9,  93}, {  -3,  74}, {  -9,  92}, {  -8,  87}, { -23, 126},
        {  -7,  92}, {  -5,  89}, {  -7,  96}, { -13, 108}, {  -3,  46}, {  -1,  65}, {  -1,  57}, {  -9,  93},
        {  -3,  74}, {  -9,  92}, {  -8,  87}, { -23, 126}, {  -2,  85}, {  -6,  78}, {  -1,  75}, {  -7,  77},
        {   2,  54}, {   5,  50}, {  -3,  68}, {   1,  50}, {   6,  42}, {  -4,  81}, {   1,  63}, {  -4,  70},
        {   0,  67}, {   2,  57}, {  -2,  76}, {  11,  35}, {   4,  64}, {   1,  61}, {  11,  35}, {  18,  25},
        {  12,  24}, {  13,  29}, {  13,  36}, { -10,  93}, {  -7,  73}, {  -2,  73}, {  13,  46}, {   9,  49},
        {  -7, 100}, {   9,  53}, {   2,  53}, {   5,  53}, {  -2,  61}, {   0,  56}, {   0,  56}, { -13,  63},
        {  -5,  60}, {  -1,  62}, {   4,  57}, {  -6,  69}, {   4,  57}, {  14,  39}, {   4,  51}, {  13,  68},
        {  -2,  85}, {  -6,  78}, {  -1,  75}, {  -7,  77}, {   2,  54}, {   5,  50}, {  -3,  68}, {   1,  50},
        {   6,  42}, {  -4,  81}, {   1,  63}, {  -4,  70}, {   0,  67}, {   2,  57}, {  -2,  76}, {  11,  35},
        {   4,  64}, {   1,  61}, {  11,  35}, {  18,  25}, {  12,  24}, {  13,  29}, {  13,  36}, { -10,  93},
        {  -7,  73}, {  -2,  73}, {  13,  46}, {   9,  49}, {  -7, 100}, {   9,  53}, {   2,  53}, {   5,  53},
        {  -2,  61}, {   0,  56}, {   0,  56}, { -13,  63}, {  -5,  60}, {  -1,  62}, {   4,  57}, {  -6,  69},
        {   4,  57}, {  14,  39}, {   4,  51}, {  13,  68}, {  11,  28}, {   2,  40}, {   3,  44}, {   0,  49},
        {   0,  46}, {   2,  44}, {   2,  51}, {   0,  47}, {   4,  39}, {   2,  62}, {   6,  46}, {   0,  54},
        {   3,  54}, {   2,  58}, {   4,  63}, {   6,  51}, {   6,  57}, {   7,  53}, {   6,  52}, {   6,  55},
        {  11,  45}, {  14,  36}, {   8,  53}, {  -1,  82}, {   7,  55}, {  -3,  78}, {  15,  46}, {  22,  31},
        {  -1,  84}, {  25,   7}, {  30,  -7}, {  28,   3}, {  28,   4}, {  32,   0}, {  34,  -1}, {  30,   6},
        {  30,   6}, {  32,   9}, {  31,  19}, {  26,  27}, {  26,  30}, {  37,  20}, {  28,  34}, {  17,  70},
        {  11,  28}, {   2,  40}, {   3,  44}, {   0,  49}, {   0,  46}, {   2,  44}, {   2,  51}, {   0,  47},
        {   4,  39}, {   2,  62}, {   6,  46}, {   0,  54}, {   3,  54}, {   2,  58}, {   4,  63}, {   6,  51},
        {   6,  57}, {   7,  53}, {   6,  52}, {   6,  55}, {  11,  45}, {  14,  36}, {   8,  53}, {  -1,  82},
        {   7,  55}, {  -3,  78}, {  15,  46}, {  22,  31}, {  -1,  84}, {  25,   7}, {  30,  -7}, {  28,   3},
        {  28,   4}, {  32,   0}, {  34,  -1}, {  30,   6}, {  30,   6}, {  32,   9}, {  31,  19}, {  26,  27},
        {  26,  30}, {  37,  20}, {  28,  34}, {  17,  70}, {  -4,  79}, {  -7,  71}, {  -5,  69}, {  -9,  70},
        {  -8,  66}, { -10,  68}, { -19,  73}, { -12,  69}, { -16,  70}, { -15,  67}, { -20,  62}, { -19,  70},
        { -16,  66}, { -22,  65}, { -20,  63}, {  -5,  85}, {  -6,  81}, { -10,  77}, {  -7,  81}, { -17,  80},
        { -18,  73}, {  -4,  74}, { -10,  83}, {  -9,  71}, {  -9,  67}, {  -1,  61}, {  -8,  66}, { -14,  66},
        {   0,  59}, {   2,  59}, {   9,  -2}, {  26,  -9}, {  33,  -9}, {  39,  -7}, {  41,  -2}, {  45,   3},
        {  49,   9}, {  45,  27}, {  36,  59}, {  21, -13}, {  33, -14}, {  39,  -7}, {  46,  -2}, {  51,   2},
        {  60,   6}, {  61,  17}, {  55,  34}, {  42,  62}, {  -6,  66}, {  -7,  35}, {  -7,  42}, {  -8,  45},
        {  -5,  48}, { -12,  56}, {  -6,  60}, {  -5,  62}, {  -8,  66}, {  -8,  76}, {  -4,  79}, {  -7,  71},
        {  -5,  69}, {  -9,  70}, {  -8,  66}, { -10,  68}, { -19,  73}, { -12,  69}, { -16,  70}, { -15,  67},
        { -20,  62}, { -19,  70}, { -16,  66}, { -22,  65}, { -20,  63}, {  -5,  85}, {  -6,  81}, { -10,  77},
        {  -7,  81}, { -17,  80}, { -18,  73}, {  -4,  74}, { -10,  83}, {  -9,  71}, {  -9,  67}, {  -1,  61},
        {  -8,  66}, { -14,  66}, {   0,  59}, {   2,  59}, {   9,  -2}, {  26,  -9}, {  33,  -9}, {  39,  -7},
        {  41,  -2}, {  45,   3}, {  49,   9}, {  45,  27}, {  36,  59}, {  21, -13}, {  33, -14}, {  39,  -7},
        {  46,  -2}, {  51,   2}, {  60,   6}, {  61,  17}, {  55,  34}, {  42,  62}, {  -6,  66}, {  -7,  35},
        {  -7,  42}, {  -8,  45}, {  -5,  48}, { -12,  56}, {  -6,  60}, {  -5,  62}, {  -8,  66}, {  -8,  76},
        { -13, 106}, { -16, 106}, { -10,  87}, { -21, 114}, { -18, 110}, { -14,  98}, { -22, 110}, { -21, 106},
        { -18, 103}, { -21, 107}, { -23, 108}, { -26, 112}, { -10,  96}, { -12,  95}, {  -5,  91}, {  -9,  93},
        { -22,  94}, {  -5,  86}, {   9,  67}, {  -4,  80}, { -10,  85}, {  -1,  70}, {   7,  60}, {   9,  58},
        {   5,  61}, {  12,  50}, {  15,  50}, {  18,  49}, {  17,  54}, {  10,  41}, {   7,  46}, {  -1,  51},
        {   7,  49}, {   8,  52}, {   9,  41}, {   6,  47}, {   2,  55}, {  13,  41}, {  10,  44}, {   6,  50},
        {   5,  53}, {  13,  49}, {   4,  63}, {   6,  64}, { -13, 106}, { -16, 106}, { -10,  87}, { -21, 114},
        { -18, 110}, { -14,  98}, { -22, 110}, { -21, 106}, { -18, 103}, { -21, 107}, { -23, 108}, { -26, 112},
        { -10,  96}, { -12,  95}, {  -5,  91}, {  -9,  93}, { -22,  94}, {  -5,  86}, {   9,  67}, {  -4,  80},
        { -10,  85}, {  -1,  70}, {   7,  60}, {   9,  58}, {   5,  61}, {  12,  50}, {  15,  50}, {  18,  49},
        {  17,  54}, {  10,  41}, {   7,  46}, {  -1,  51}, {   7,  49}, {   8,  52}, {   9,  41}, {   6,  47},
        {   2,  55}, {  13,  41}, {  10,  44}, {   6,  50}, {   5,  53}, {  13,  49}, {   4,  63}, {   6,  64},
        {  14,  11}, {  11,  14}, {   9,  11}, {  18,  11}, {  21,   9}, {  23,  -2}, {  32, -15}, {  32, -15},
        {  34, -21}, {  39, -23}, {  42, -33}, {  41, -31}, {  46, -28}, {  38, -12}, {  21,  29}, {  45, -24},
        {  53, -45}, {  48, -26}, {  65, -43}, {  43, -19}, {  39, -10}, {  30,   9}, {  18,  26}, {  20,  27},
        {   0,  57}, { -14,  82}, {  -5,  75}, { -19,  97}, { -35, 125}, {  27,   0}, {  28,   0}, {  31,  -4},
        {  27,   6}, {  34,   8}, {  30,  10}, {  24,  22}, {  33,  19}, {  22,  32}, {  26,  31}, {  21,  41},
        {  26,  44}, {  23,  47}, {  16,  65}, {  14,  71}, {  14,  11}, {  11,  14}, {   9,  11}, {  18,  11},
        {  21,   9}, {  23,  -2}, {  32, -15}, {  32, -15}, {  34, -21}, {  39, -23}, {  42, -33}, {  41, -31},
        {  46, -28}, {  38, -12}, {  21,  29}, {  45, -24}, {  53, -45}, {  48, -26}, {  65, -43}, {  43, -19},
        {  39, -10}, {  30,   9}, {  18,  26}, {  20,  27}, {   0,  57}, { -14,  82}, {  -5,  75}, { -19,  97},
        { -35, 125}, {  27,   0}, {  28,   0}, {  31,  -4}, {  27,   6}, {  34,   8}, {  30,  10}, {  24,  22},
        {  33,  19}, {  22,  32}, {  26,  31}, {  21,  41}, {  26,  44}, {  23,  47}, {  16,  65}, {  14,  71},
        {  -6,  76}, {  -2,  44}, {   0,  45}, {   0,  52}, {  -3,  64}, {  -2,  59}, {  -4,  70}, {  -4,  75},
        {  -8,  82}, { -17, 102}, {  -9,  77}, {   3,  24}, {   0,  42}, {   0,  48}, {   0,  55}, {  -6,  59},
        {  -7,  71}, { -12,  83}, { -11,  87}, { -30, 119}, {   1,  58}, {  -3,  29}, {  -1,  36}, {   1,  38},
        {   2,  43}, {  -6,  55}, {   0,  58}, {   0,  64}, {  -3,  74}, { -10,  90}, {  -6,  76}, {  -2,  44},
        {   0,  45}, {   0,  52}, {  -3,  64}, {  -2,  59}, {  -4,  70}, {  -4,  75}, {  -8,  82}, { -17, 102},
        {  -9,  77}, {   3,  24}, {   0,  42}, {   0,  48}, {   0,  55}, {  -6,  59}, {  -7,  71}, { -12,  83},
        { -11,  87}, { -30, 119}, {   1,  58}, {  -3,  29}, {  -1,  36}, {   1,  38}, {   2,  43}, {  -6,  55},
        {   0,  58}, {   0,  64}, {  -3,  74}, { -10,  90}, {  -3,  74}, {  -9,  92}, {  -8,  87}, { -23, 126},
        {  -3,  74}, {  -9,  92}, {  -8,  87}, { -23, 126}, {  -3,  74}, {  -9,  92}, {  -8,  87}, { -23, 126},
    },
    { // cabac_init_idc = 1
        {  20, -15}, {   2,  54}, {   3,  74}, {  20, -15}, {   2,  54}, {   3,  74}, { -28, 127}, { -23, 104},
        {  -6,  53}, {  -1,  54}, {   7,  51}, {  22,  25}, {  34,   0}, {  16,   0}, {  -2,   9}, {   4,  41},
        { -29, 118}, {   2,  65}, {  -6,  71}, { -13,  79}, {   5,  52}, {   9,  50}, {  -3,  70}, {  10,  54},
        {  26,  34}, {  19,  22}, {  40,   0}, {  57,   2}, {  41,  36}, {  26,  69}, { -45, 127}, { -15, 101},
        {  -4,  76}, {  -6,  71}, { -13,  79}, {   5,  52}, {   6,  69}, { -13,  90}, {   0,  52}, {   8,  43},
        {  -2,  69}, {  -5,  82}, { -10,  96}, {   2,  59}, {   2,  75}, {  -3,  87}, {  -3, 100}, {   1,  56},
        {  -3,  74}, {  -6,  85}, {   0,  59}, {  -3,  81}, {  -7,  86}, {  -5,  95}, {  -1,  66}, {  -1,  77},
        {   1,  70}, {  -2,  86}, {  -5,  72}, {   0,  61}, {   0,  41}, {   0,  63}, {   0,  63}, {   0,  63},
        {  -9,  83}, {   4,  86}, {   0,  97}, {  -7,  72}, {  13,  41}, {   3,  62}, {  13,  15}, {   7,  51},
        {   2,  80}, { -39, 127}, { -18,  91}, { -17,  96}, { -26,  81}, { -35,  98}, { -24, 102}, { -23,  97},
        { -27, 119}, { -24,  99}, { -21, 110}, { -18, 102}, { -36, 127}, {   0,  80}, {  -5,  89}, {  -7,  94},
        {  -4,  92}, {   0,  39}, {   0,  65}, { -15,  84}, { -35, 127}, {  -2,  73}, { -12, 104}, {  -9,  91},
        { -31, 127}, {   3,  55}, {   7,  56}, {   7,  55}, {   8,  61}, {  -3,  53}, {   0,  68}, {  -7,  74},
        {  -9,  88}, { -13, 103}, { -13,  91}, {  -9,  89}, { -14,  92}, {  -8,  76}, { -12,  87}, { -23, 110},
        { -24, 105}, { -10,  78}, { -20, 112}, { -17,  99}, { -78, 127}, { -70, 127}, { -50, 127}, { -46, 127},
        {  -4,  66}, {  -5,  78}, {  -4,  71}, {  -8,  72}, {   2,  59}, {  -1,  55}, {  -7,  70}, {  -6,  75},
        {  -8,  89}, { -34, 119}, {  -3,  75}, {  32,  20}, {  30,  22}, { -44, 127}, {   0,  54}, {  -5,  61},
        {   0,  58}, {  -1,  60}, {  -3,  61}, {  -8,  67}, { -25,  84}, { -14,  74}, {  -5,  65}, {   5,  52},
        {   2,  57}, {   0,  61}, {  -9,  69}, { -11,  70}, {  18,  55}, {  -4,  71}, {   0,  58}, {   7,  61},
        {   9,  41}, {  18,  25}, {   9,  32}, {   5,  43}, {   9,  47}, {   0,  44}, {   0,  51}, {   2,  46},
        {  19,  38}, {  -4,  66}, {  15,  38}, {  12,  42}, {   9,  34}, {   0,  89}, {   4,  45}, {  10,  28},
        {  10,  31}, {  33, -11}, {  52, -43}, {  18,  15}, {  28,   0}, {  35, -22}, {  38, -25}, {  34,   0},
        {  39, -18}, {  32, -12}, { 102, -94}, {   0,   0}, {  56, -15}, {  33,  -4}, {  29,  10}, {  37,  -5},
        {  51, -29}, {  39,  -9}, {  52, -34}, {  69, -58}, {  67, -63}, {  44,  -5}, {  32,   7}, {  55, -29},
        {  32,   1}, {   0,   0}, {  27,  36}, {  33, -25}, {  34, -30}, {  36, -28}, {  38, -28}, {  38, -27},
        {  34, -18}, {  35, -16}, {  34, -14}, {  32,  -8}, {  37,  -6}, {  35,   0}, {  30,  10}, {  28,  18},
        {  26,  25}, {  29,  41}, {   0,  75}, {   2,  72}, {   8,  77}, {  14,  35}, {  18,  31}, {  17,  35},
        {  21,  30}, {  17,  45}, {  20,  42}, {  18,  45}, {  27,  26}, {  16,  54}, {   7,  66}, {  16,  56},
        {  11,  73}, {  10,  67}, { -10, 116}, { -23, 112}, { -15,  71}, {  -7,  61}, {   0,  53}, {  -5,  66},
        { -11,  77}, {  -9,  80}, {  -9,  84}, { -10,  87}, { -34, 127}, { -21, 101}, {  -3,  39}, {  -5,  53},
        {  -7,  61}, { -11,  75}, { -15,  77}, { -17,  91}, { -25, 107}, { -25, 111}, { -28, 122}, { -11,  76},
        { -10,  44}, { -10,  52}, { -10,  57}, {  -9,  58}, { -16,  72}, {  -7,  69}, {  -4,  69}, {  -5,  74},
        {  -9,  86}, {   2,  66}, {  -9,  34}, {   1,  32}, {  11,  31}, {   5,  52}, {  -2,  55}, {  -2,  67},
        {   0,  73}, {  -8,  89}, {   3,  52}, {   7,   4}, {  10,   8}, {  17,   8}, {  16,  19}, {   3,  37},
        {  -1,  61}, {  -5,  73}, {  -1,  70}, {  -4,  78}, {   0,   0}, { -21, 126}, { -23, 124}, { -20, 110},
        { -26, 126}, { -25, 124}, { -17, 105}, { -27, 121}, { -27, 117}, { -17, 102}, { -26, 117}, { -27, 116},
        { -33, 122}, { -10,  95}, { -14, 100}, {  -8,  95}, { -17, 111}, { -28, 114}, {  -6,  89}, {  -2,  80},
        {  -4,  82}, {  -9,  85}, {  -8,  81}, {  -1,  72}, {   5,  64}, {   1,  67}, {   9,  56}, {   0,  69},
        {   1,  69}, {   7,  69}, {  -7,  69}, {  -6,  67}, { -16,  77}, {  -2,  64}, {   2,  61}, {  -6,  67},
        {  -3,  64}, {   2,  57}, {  -3,  65}, {  -3,  66}, {   0,  62}, {   9,  51}, {  -1,  66}, {  -2,  71},
        {  -2,  75}, {  -1,  70}, {  -9,  72}, {  14,  60}, {  16,  37}, {   0,  47}, {  18,  35}, {  11,  37},
        {  12,  41}, {  10,  41}, {   2,  48}, {  12,  41}, {  13,  41}, {   0,  59}, {   3,  50}, {  19,  40},
        {   3,  66}, {  18,  50}, {  19,  -6}, {  18,  -6}, {  14,   0}, {  26, -12}, {  31, -16}, {  33, -25},
        {  33, -22}, {  37, -28}, {  39, -30}, {  42, -30}, {  47, -42}, {  45, -36}, {  49, -34}, {  41, -17},
        {  32,   9}, {  69, -71}, {  63, -63}, {  66, -64}, {  77, -74}, {  54, -39}, {  52, -35}, {  41, -10},
        {  36,   0}, {  40,  -1}, {  30,  14}, {  28,  26}, {  23,  37}, {  12,  55}, {  11,  65}, {  37, -33},
        {  39, -36}, {  40, -37}, {  38, -30}, {  46, -33}, {  42, -30}, {  40, -24}, {  49, -29}, {  38, -12},
        {  40, -10}, {  38,  -3}, {  46,  -5}, {  31,  20}, {  29,  30}, {  25,  44}, {  12,  48}, {  11,  49},
        {  26,  45}, {  22,  22}, {  23,  22}, {  27,  21}, {  33,  20}, {  26,  28}, {  30,  24}, {  27,  34},
        {  18,  42}, {  25,  39}, {  18,  50}, {  12,  70}, {  21,  54}, {  14,  71}, {  11,  83}, {  25,  32},
        {  21,  49}, {  21,  54}, {  -5,  85}, {  -6,  81}, { -10,  77}, {  -7,  81}, { -17,  80}, { -18,  73},
        {  -4,  74}, { -10,  83}, {  -9,  71}, {  -9,  67}, {  -1,  61}, {  -8,  66}, { -14,  66}, {   0,  59},
        {   2,  59}, {  17, -10}, {  32, -13}, {  42,  -9}, {  49,  -5}, {  53,   0}, {  64,   3}, {  68,  10},
        {  66,  27}, {  47,  57}, {  -5,  71}, {   0,  24}, {  -1,  36}, {  -2,  42}, {  -2,  52}, {  -9,  57},
        {  -6,  63}, {  -4,  65}, {  -4,  67}, {  -7,  82}, {  -3,  81}, {  -3,  76}, {  -7,  72}, {  -6,  78},
        { -12,  72}, { -14,  68}, {  -3,  70}, {  -6,  76}, {  -5,  66}, {  -5,  62}, {   0,  57}, {  -4,  61},
        {  -9,  60}, {   1,  54}, {   2,  58}, {  17, -10}, {  32, -13}, {  42,  -9}, {  49,  -5}, {  53,   0},
        {  64,   3}, {  68,  10}, {  66,  27}, {  47,  57}, {   0,  80}, {  -5,  89}, {  -7,  94}, {  -4,  92},
        {   0,  39}, {   0,  65}, { -15,  84}, { -35, 127}, {  -2,  73}, { -12, 104}, {  -9,  91}, { -31, 127},
        {   0,  80}, {  -5,  89}, {  -7,  94}, {  -4,  92}, {   0,  39}, {   0,  65}, { -15,  84}, { -35, 127},
        {  -2,  73}, { -12, 104}, {  -9,  91}, { -31, 127}, { -13, 103}, { -13,  91}, {  -9,  89}, { -14,  92},
        {  -8,  76}, { -12,  87}, { -23, 110}, { -24, 105}, { -10,  78}, { -20, 112}, { -17,  99}, { -78, 127},
        { -70, 127}, { -50, 127}, { -46, 127}, {  -4,  66}, {  -5,  78}, {  -4,  71}, {  -8,  72}, {   2,  59},
        {  -1,  55}, {  -7,  70}, {  -6,  75}, {  -8,  89}, { -34, 119}, {  -3,  75}, {  32,  20}, {  30,  22},
        { -44, 127}, {   0,  54}, {  -5,  61}, {   0,  58}, {  -1,  60}, {  -3,  61}, {  -8,  67}, { -25,  84},
        { -14,  74}, {  -5,  65}, {   5,  52}, {   2,  57}, {   0,  61}, {  -9,  69}, { -11,  70}, {  18,  55},
        { -13, 103}, { -13,  91}, {  -9,  89}, { -14,  92}, {  -8,  76}, { -12,  87}, { -23, 110}, { -24, 105},
        { -10,  78}, { -20, 112}, { -17,  99}, { -78, 127}, { -70, 127}, { -50, 127}, { -46, 127}, {  -4,  66},
        {  -5,  78}, {  -4,  71}, {  -8,  72}, {   2,  59}, {  -1,  55}, {  -7,  70}, {  -6,  75}, {  -8,  89},
        { -34, 119}, {  -3,  75}, {  32,  20}, {  30,  22}, { -44, 127}, {   0,  54}, {  -5,  61}, {   0,  58},
        {  -1,  60}, {  -3,  61}, {  -8,  67}, { -25,  84}, { -14,  74}, {  -5,  65}, {   5,  52}, {   2,  57},
        {   0,  61}, {  -9,  69}, { -11,  70}, {  18,  55}, {   4,  45}, {  10,  28}, {  10,  31}, {  33, -11},
        {  52, -43}, {  18,  15}, {  28,   0}, {  35, -22}, {  38, -25}, {  34,   0}, {  39, -18}, {  32, -12},
        { 102, -94}, {   0,   0}, {  56, -15}, {  33,  -4}, {  29,  10}, {  37,  -5}, {  51, -29}, {  39,  -9},
        {  52, -34}, {  69, -58}, {  67, -63}, {  44,  -5}, {  32,   7}, {  55, -29}, {  32,   1}, {   0,   0},
        {  27,  36}, {  33, -25}, {  34, -30}, {  36, -28}, {  38, -28}, {  38, -27}, {  34, -18}, {  35, -16},
        {  34, -14}, {  32,  -8}, {  37,  -6}, {  35,   0}, {  30,  10}, {  28,  18}, {  26,  25}, {  29,  41},
        {   4,  45}, {  10,  28}, {  10,  31}, {  33, -11}, {  52, -43}, {  18,  15}, {  28,   0}, {  35, -22},
        {  38, -25}, {  34,   0}, {  39, -18}, {  32, -12}, { 102, -94}, {   0,   0}, {  56, -15}, {  33,  -4},
        {  29,  10}, {  37,  -5}, {  51, -29}, {  39,  -9}, {  52, -34}, {  69, -58}, {  67, -63}, {  44,  -5},
        {  32,   7}, {  55, -29}, {  32,   1}, {   0,   0}, {  27,  36}, {  33, -25}, {  34, -30}, {  36, -28},
        {  38, -28}, {  38, -27}, {  34, -18}, {  35, -16}, {  34, -14}, {  32,  -8}, {  37,  -6}, {  35,   0},
        {  30,  10}, {  28,  18}, {  26,  25}, {  29,  41}, {  -5,  85}, {  -6,  81}, { -10,  77}, {  -7,  81},
        { -17,  80}, { -18,  73}, {  -4,  74}, { -10,  83}, {  -9,  71}, {  -9,  67}, {  -1,  61}, {  -8,  66},
        { -14,  66}, {   0,  59}, {   2,  59}, {  -3,  81}, {  -3,  76}, {  -7,  72}, {  -6,  78}, { -12,  72},
        { -14,  68}, {  -3,  70}, {  -6,  76}, {  -5,  66}, {  -5,  62}, {   0,  57}, {  -4,  61}, {  -9,  60},
        {   1,  54}, {   2,  58}, {  17, -10}, {  32, -13}, {  42,  -9}, {  49,  -5}, {  53,   0}, {  64,   3},
        {  68,  10}, {  66,  27}, {  47,  57}, {  17, -10}, {  32, -13}, {  42,  -9}, {  49,  -5}, {  53,   0},
        {  64,   3}, {  68,  10}, {  66,  27}, {  47,  57}, {  -5,  71}, {   0,  24}, {  -1,  36}, {  -2,  42},
        {  -2,  52}, {  -9,  57}, {  -6,  63}, {  -4,  65}, {  -4,  67}, {  -7,  82}, {  -5,  85}, {  -6,  81},
        { -10,  77}, {  -7,  81}, { -17,  80}, { -18,  73}, {  -4,  74}, { -10,  83}, {  -9,  71}, {  -9,  67},
        {  -1,  61}, {  -8,  66}, { -14,  66}, {   0,  59}, {   2,  59}, {  -3,  81}, {  -3,  76}, {  -7,  72},
        {  -6,  78}, { -12,  72}, { -14,  68}, {  -3,  70}, {  -6,  76}, {  -5,  66}, {  -5,  62}, {   0,  57},
        {  -4,  61}, {  -9,  60}, {   1,  54}, {   2,  58}, {  17, -10}, {  32, -13}, {  42,  -9}, {  49,  -5},
        {  53,   0}, {  64,   3}, {  68,  10}, {  66,  27}, {  47,  57}, {  17, -10}, {  32, -13}, {  42,  -9},
        {  49,  -5}, {  53,   0}, {  64,   3}, {  68,  10}, {  66,  27}, {  47,  57}, {  -5,  71}, {   0,  24},
        {  -1,  36}, {  -2,  42}, {  -2,  52}, {  -9,  57}, {  -6,  63}, {  -4,  65}, {  -4,  67}, {  -7,  82},
        { -21, 126}, { -23, 124}, { -20, 110}, { -26, 126}, { -25, 124}, { -17, 105}, { -27, 121}, { -27, 117},
        { -17, 102}, { -26, 117}, { -27, 116}, { -33, 122}, { -10,  95}, { -14, 100}, {  -8,  95}, { -17, 111},
        { -28, 114}, {  -6,  89}, {  -2,  80}, {  -4,  82}, {  -9,  85}, {  -8,  81}, {  -1,  72}, {   5,  64},
        {   1,  67}, {   9,  56}, {   0,  69}, {   1,  69}, {   7,  69}, {  -7,  69}, {  -6,  67}, { -16,  77},
        {  -2,  64}, {   2,  61}, {  -6,  67}, {  -3,  64}, {   2,  57}, {  -3,  65}, {  -3,  66}, {   0,  62},
        {   9,  51}, {  -1,  66}, {  -2,  71}, {  -2,  75}, { -21, 126}, { -23, 124}, { -20, 110}, { -26, 126},
        { -25, 124}, { -17, 105}, { -27, 121}, { -27, 117}, { -17, 102}, { -26, 117}, { -27, 116}, { -33, 122},
        { -10,  95}, { -14, 100}, {  -8,  95}, { -17, 111}, { -28, 114}, {  -6,  89}, {  -2,  80}, {  -4,  82},
        {  -9,  85}, {  -8,  81}, {  -1,  72}, {   5,  64}, {   1,  67}, {   9,  56}, {   0,  69}, {   1,  69},
        {   7,  69}, {  -7,  69}, {  -6,  67}, { -16,  77}, {  -2,  64}, {   2,  61}, {  -6,  67}, {  -3,  64},
        {   2,  57}, {  -3,  65}, {  -3,  66}, {   0,  62}, {   9,  51}, {  -1,  66}, {  -2,  71}, {  -2,  75},
        {  19,  -6}, {  18,  -6}, {  14,   0}, {  26, -12}, {  31, -16}, {  33, -25}, {  33, -22}, {  37, -28},
        {  39, -30}, {  42, -30}, {  47, -42}, {  45, -36}, {  49, -34}, {  41, -17}, {  32,   9}, {  69, -71},
        {  63, -63}, {  66, -64}, {  77, -74}, {  54, -39}, {  52, -35}, {  41, -10}, {  36,   0}, {  40,  -1},
        {  30,  14}, {  28,  26}, {  23,  37}, {  12,  55}, {  11,  65}, {  37, -33}, {  39, -36}, {  40, -37},
        {  38, -30}, {  46, -33}, {  42, -30}, {  40, -24}, {  49, -29}, {  38, -12}, {  40, -10}, {  38,  -3},
        {  46,  -5}, {  31,  20}, {  29,  30}, {  25,  44}, {  19,  -6}, {  18,  -6}, {  14,   0}, {  26, -12},
        {  31, -16}, {  33, -25}, {  33, -22}, {  37, -28}, {  39, -30}, {  42, -30}, {  47, -42}, {  45, -36},
        {  49, -34}, {  41, -17}, {  32,   9}, {  69, -71}, {  63, -63}, {  66, -64}, {  77, -74}, {  54, -39},
        {  52, -35}, {  41, -10}, {  36,   0}, {  40,  -1}, {  30,  14}, {  28,  26}, {  23,  37}, {  12,  55},
        {  11,  65}, {  37, -33}, {  39, -36}, {  40, -37}, {  38, -30}, {  46, -33}, {  42, -30}, {  40, -24},
        {  49, -29}, {  38, -12}, {  40, -10}, {  38,  -3}, {  46,  -5}, {  31,  20}, {  29,  30}, {  25,  44},
        { -23, 112}, { -15,  71}, {  -7,  61}, {   0,  53}, {  -5,  66}, { -11,  77}, {  -9,  80}, {  -9,  84},
        { -10,  87}, { -34, 127}, { -21, 101}, {  -3,  39}, {  -5,  53}, {  -7,  61}, { -11,  75}, { -15,  77},
        { -17,  91}, { -25, 107}, { -25, 111}, { -28, 122}, { -11,  76}, { -10,  44}, { -10,  52}, { -10,  57},
        {  -9,  58}, { -16,  72}, {  -7,  69}, {  -4,  69}, {  -5,  74}, {  -9,  86}, { -23, 112}, { -15,  71},
        {  -7,  61}, {   0,  53}, {  -5,  66}, { -11,  77}, {  -9,  80}, {  -9,  84}, { -10,  87}, { -34, 127},
        { -21, 101}, {  -3,  39}, {  -5,  53}, {  -7,  61}, { -11,  75}, { -15,  77}, { -17,  91}, { -25, 107},
        { -25, 111}, { -28, 122}, { -11,  76}, { -10,  44}, { -10,  52}, { -10,  57}, {  -9,  58}, { -16,  72},
        {  -7,  69}, {  -4,  69}, {  -5,  74}, {  -9,  86}, {  -2,  73}, { -12, 104}, {  -9,  91}, { -31, 127},
        {  -2,  73}, { -12, 104}, {  -9,  91}, { -31, 127}, {  -2,  73}, { -12, 104}, {  -9,  91}, { -31, 127},
    },
    { // cabac_init_idc = 2
        {  20, -15}, {   2,  54}, {   3,  74}, {  20, -15}, {   2,  54}, {   3,  74}, { -28, 127}, { -23, 104},
        {  -6,  53}, {  -1,  54}, {   7,  51}, {  29,  16}, {  25,   0}, {  14,   0}, { -10,  51}, {  -3,  62},
        { -27,  99}, {  26,  16}, {  -4,  85}, { -24, 102}, {   5,  57}, {   6,  57}, { -17,  73}, {  14,  57},
        {  20,  40}, {  20,  10}, {  29,   0}, {  54,   0}, {  37,  42}, {  12,  97}, { -32, 127}, { -22, 117},
        {  -2,  74}, {  -4,  85}, { -24, 102}, {   5,  57}, {  -6,  93}, { -14,  88}, {  -6,  44}, {   4,  55},
        { -11,  89}, { -15, 103}, { -21, 116}, {  19,  57}, {  20,  58}, {   4,  84}, {   6,  96}, {   1,  63},
        {  -5,  85}, { -13, 106}, {   5,  63}, {   6,  75}, {  -3,  90}, {  -1, 101}, {   3,  55}, {  -4,  79},
        {  -2,  75}, { -12,  97}, {  -7,  50}, {   1,  60}, {   0,  41}, {   0,  63}, {   0,  63}, {   0,  63},
        {  -9,  83}, {   4,  86}, {   0,  97}, {  -7,  72}, {  13,  41}, {   3,  62}, {   7,  34}, {  -9,  88},
        { -20, 127}, { -36, 127}, { -17,  91}, { -14,  95}, { -25,  84}, { -25,  86}, { -12,  89}, { -17,  91},
        { -31, 127}, { -14,  76}, { -18, 103}, { -13,  90}, { -37, 127}, {  11,  80}, {   5,  76}, {   2,  84},
        {   5,  78}, {  -6,  55}, {   4,  61}, { -14,  83}, { -37, 127}, {  -5,  79}, { -11, 104}, { -11,  91},
        { -30, 127}, {   0,  65}, {  -2,  79}, {   0,  72}, {  -4,  92}, {  -6,  56}, {   3,  68}, {  -8,  71},
        { -13,  98}, {  -4,  86}, { -12,  88}, {  -5,  82}, {  -3,  72}, {  -4,  67}, {  -8,  72}, { -16,  89},
        {  -9,  69}, {  -1,  59}, {   5,  66}, {   4,  57}, {  -4,  71}, {  -2,  71}, {   2,  58}, {  -1,  74},
        {  -4,  44}, {  -1,  69}, {   0,  62}, {  -7,  51}, {  -4,  47}, {  -6,  42}, {  -3,  41}, {  -6,  53},
        {   8,  76}, {  -9,  78}, { -11,  83}, {   9,  52}, {   0,  67}, {  -5,  90}, {   1,  67}, { -15,  72},
        {  -5,  75}, {  -8,  80}, { -21,  83}, { -21,  64}, { -13,  31}, { -25,  64}, { -29,  94}, {   9,  75},
        {  17,  63}, {  -8,  74}, {  -5,  35}, {  -2,  27}, {  13,  91}, {   3,  65}, {  -7,  69}, {   8,  77},
        { -10,  66}, {   3,  62}, {  -3,  68}, { -20,  81}, {   0,  30}, {   1,   7}, {  -3,  23}, { -21,  74},
        {  16,  66}, { -23, 124}, {  17,  37}, {  44, -18}, {  50, -34}, { -22, 127}, {   4,  39}, {   0,  42},
        {   7,  34}, {  11,  29}, {   8,  31}, {   6,  37}, {   7,  42}, {   3,  40}, {   8,  33}, {  13,  43},
        {  13,  36}, {   4,  47}, {   3,  55}, {   2,  58}, {   6,  60}, {   8,  44}, {  11,  44}, {  14,  42},
        {   7,  48}, {   4,  56}, {   4,  52}, {  13,  37}, {   9,  49}, {  19,  58}, {  10,  48}, {  12,  45},
        {   0,  69}, {  20,  33}, {   8,  63}, {  35, -18}, {  33, -25}, {  28,  -3}, {  24,  10}, {  27,   0},
        {  34, -14}, {  52, -44}, {  39, -24}, {  19,  17}, {  31,  25}, {  36,  29}, {  24,  33}, {  34,  15},
        {  30,  20}, {  22,  73}, {  20,  34}, {  19,  31}, {  27,  44}, {  19,  16}, {  15,  36}, {  15,  36},
        {  21,  28}, {  25,  21}, {  30,  20}, {  31,  12}, {  27,  16}, {  24,  42}, {   0,  93}, {  14,  56},
        {  15,  57}, {  26,  38}, { -24, 127}, { -24, 115}, { -22,  82}, {  -9,  62}, {   0,  53}, {   0,  59},
        { -14,  85}, { -13,  89}, { -13,  94}, { -11,  92}, { -29, 127}, { -21, 100}, { -14,  57}, { -12,  67},
        { -11,  71}, { -10,  77}, { -21,  85}, { -16,  88}, { -23, 104}, { -15,  98}, { -37, 127}, { -10,  82},
        {  -8,  48}, {  -8,  61}, {  -8,  66}, {  -7,  70}, { -14,  75}, { -10,  79}, {  -9,  83}, { -12,  92},
        { -18, 108}, {  -4,  79}, { -22,  69}, { -16,  75}, {  -2,  58}, {   1,  58}, { -13,  78}, {  -9,  83},
        {  -4,  81}, { -13,  99}, { -13,  81}, {  -6,  38}, { -13,  62}, {  -6,  58}, {  -2,  59}, { -16,  73},
        { -10,  76}, { -13,  86}, {  -9,  83}, { -10,  87}, {   0,   0}, { -22, 127}, { -25, 127}, { -25, 120},
        { -27, 127}, { -19, 114}, { -23, 117}, { -25, 118}, { -26, 117}, { -24, 113}, { -28, 118}, { -31, 120},
        { -37, 124}, { -10,  94}, { -15, 102}, { -10,  99}, { -13, 106}, { -50, 127}, {  -5,  92}, {  17,  57},
        {  -5,  86}, { -13,  94}, { -12,  91}, {  -2,  77}, {   0,  71}, {  -1,  73}, {   4,  64}, {  -7,  81},
        {   5,  64}, {  15,  57}, {   1,  67}, {   0,  68}, { -10,  67}, {   1,  68}, {   0,  77}, {   2,  64},
        {   0,  68}, {  -5,  78}, {   7,  55}, {   5,  59}, {   2,  65}, {  14,  54}, {  15,  44}, {   5,  60},
        {   2,  70}, {  -2,  76}, { -18,  86}, {  12,  70}, {   5,  64}, { -12,  70}, {  11,  55}, {   5,  56},
        {   0,  69}, {   2,  65}, {  -6,  74}, {   5,  54}, {   7,  54}, {  -6,  76}, { -11,  82}, {  -2,  77},
        {  -2,  77}, {  25,  42}, {  17, -13}, {  16,  -9}, {  17, -12}, {  27, -21}, {  37, -30}, {  41, -40},
        {  42, -41}, {  48, -47}, {  39, -32}, {  46, -40}, {  52, -51}, {  46, -41}, {  52, -39}, {  43, -19},
        {  32,  11}, {  61, -55}, {  56, -46}, {  62, -50}, {  81, -67}, {  45, -20}, {  35,  -2}, {  28,  15},
        {  34,   1}, {  39,   1}, {  30,  17}, {  20,  38}, {  18,  45}, {  15,  54}, {   0,  79}, {  36, -16},
        {  37, -14}, {  37, -17}, {  32,   1}, {  34,  15}, {  29,  15}, {  24,  25}, {  34,  22}, {  31,  16},
        {  35,  18}, {  31,  28}, {  33,  41}, {  36,  28}, {  27,  47}, {  21,  62}, {  18,  31}, {  19,  26},
        {  36,  24}, {  24,  23}, {  27,  16}, {  24,  30}, {  31,  29}, {  22,  41}, {  22,  42}, {  16,  60},
        {  15,  52}, {  14,  60}, {   3,  78}, { -16, 123}, {  21,  53}, {  22,  56}, {  25,  61}, {  21,  33},
        {  19,  50}, {  17,  61}, {  -3,  78}, {  -8,  74}, {  -9,  72}, { -10,  72}, { -18,  75}, { -12,  71},
        { -11,  63}, {  -5,  70}, { -17,  75}, { -14,  72}, { -16,  67}, {  -8,  53}, { -14,  59}, {  -9,  52},
        { -11,  68}, {   9,  -2}, {  30, -10}, {  31,  -4}, {  33,  -1}, {  33,   7}, {  31,  12}, {  37,  23},
        {  31,  38}, {  20,  64}, {  -9,  71}, {  -7,  37}, {  -8,  44}, { -11,  49}, { -10,  56}, { -12,  59},
        {  -8,  63}, {  -9,  67}, {  -6,  68}, { -10,  79}, {  -3,  78}, {  -8,  74}, {  -9,  72}, { -10,  72},
        { -18,  75}, { -12,  71}, { -11,  63}, {  -5,  70}, { -17,  75}, { -14,  72}, { -16,  67}, {  -8,  53},
        { -14,  59}, {  -9,  52}, { -11,  68}, {   9,  -2}, {  30, -10}, {  31,  -4}, {  33,  -1}, {  33,   7},
        {  31,  12}, {  37,  23}, {  31,  38}, {  20,  64}, {  11,  80}, {   5,  76}, {   2,  84}, {   5,  78},
        {  -6,  55}, {   4,  61}, { -14,  83}, { -37, 127}, {  -5,  79}, { -11, 104}, { -11,  91}, { -30, 127},
        {  11,  80}, {   5,  76}, {   2,  84}, {   5,  78}, {  -6,  55}, {   4,  61}, { -14,  83}, { -37, 127},
        {  -5,  79}, { -11, 104}, { -11,  91}, { -30, 127}, {  -4,  86}, { -12,  88}, {  -5,  82}, {  -3,  72},
        {  -4,  67}, {  -8,  72}, { -16,  89}, {  -9,  69}, {  -1,  59}, {   5,  66}, {   4,  57}, {  -4,  71},
        {  -2,  71}, {   2,  58}, {  -1,  74}, {  -4,  44}, {  -1,  69}, {   0,  62}, {  -7,  51}, {  -4,  47},
        {  -6,  42}, {  -3,  41}, {  -6,  53}, {   8,  76}, {  -9,  78}, { -11,  83}, {   9,  52}, {   0,  67},
        {  -5,  90}, {   1,  67}, { -15,  72}, {  -5,  75}, {  -8,  80}, { -21,  83}, { -21,  64}, { -13,  31},
        { -25,  64}, { -29,  94}, {   9,  75}, {  17,  63}, {  -8,  74}, {  -5,  35}, {  -2,  27}, {  13,  91},
        {  -4,  86}, { -12,  88}, {  -5,  82}, {  -3,  72}, {  -4,  67}, {  -8,  72}, { -16,  89}, {  -9,  69},
        {  -1,  59}, {   5,  66}, {   4,  57}, {  -4,  71}, {  -2,  71}, {   2,  58}, {  -1,  74}, {  -4,  44},
        {  -1,  69}, {   0,  62}, {  -7,  51}, {  -4,  47}, {  -6,  42}, {  -3,  41}, {  -6,  53}, {   8,  76},
        {  -9,  78}, { -11,  83}, {   9,  52}, {   0,  67}, {  -5,  90}, {   1,  67}, { -15,  72}, {  -5,  75},
        {  -8,  80}, { -21,  83}, { -21,  64}, { -13,  31}, { -25,  64}, { -29,  94}, {   9,  75}, {  17,  63},
        {  -8,  74}, {  -5,  35}, {  -2,  27}, {  13,  91}, {   4,  39}, {   0,  42}, {   7,  34}, {  11,  29},
        {   8,  31}, {   6,  37}, {   7,  42}, {   3,  40}, {   8,  33}, {  13,  43}, {  13,  36}, {   4,  47},
        {   3,  55}, {   2,  58}, {   6,  60}, {   8,  44}, {  11,  44}, {  14,  42}, {   7,  48}, {   4,  56},
        {   4,  52}, {  13,  37}, {   9,  49}, {  19,  58}, {  10,  48}, {  12,  45}, {   0,  69}, {  20,  33},
        {   8,  63}, {  35, -18}, {  33, -25}, {  28,  -3}, {  24,  10}, {  27,   0}, {  34, -14}, {  52, -44},
        {  39, -24}, {  19,  17}, {  31,  25}, {  36,  29}, {  24,  33}, {  34,  15}, {  30,  20}, {  22,  73},
        {   4,  39}, {   0,  42}, {   7,  34}, {  11,  29}, {   8,  31}, {   6,  37}, {   7,  42}, {   3,  40},
        {   8,  33}, {  13,  43}, {  13,  36}, {   4,  47}, {   3,  55}, {   2,  58}, {   6,  60}, {   8,  44},
        {  11,  44}, {  14,  42}, {   7,  48}, {   4,  56}, {   4,  52}, {  13,  37}, {   9,  49}, {  19,  58},
        {  10,  48}, {  12,  45}, {   0,  69}, {  20,  33}, {   8,  63}, {  35, -18}, {  33, -25}, {  28,  -3},
        {  24,  10}, {  27,   0}, {  34, -14}, {  52, -44}, {  39, -24}, {  19,  17}, {  31,  25}, {  36,  29},
        {  24,  33}, {  34,  15}, {  30,  20}, {  22,  73}, {  -3,  78}, {  -8,  74}, {  -9,  72}, { -10,  72},
        { -18,  75}, { -12,  71}, { -11,  63}, {  -5,  70}, { -17,  75}, { -14,  72}, { -16,  67}, {  -8,  53},
        { -14,  59}, {  -9,  52}, { -11,  68}, {  -3,  78}, {  -8,  74}, {  -9,  72}, { -10,  72}, { -18,  75},
        { -12,  71}, { -11,  63}, {  -5,  70}, { -17,  75}, { -14,  72}, { -16,  67}, {  -8,  53}, { -14,  59},
        {  -9,  52}, { -11,  68}, {   9,  -2}, {  30, -10}, {  31,  -4}, {  33,  -1}, {  33,   7}, {  31,  12},
        {  37,  23}, {  31,  38}, {  20,  64}, {   9,  -2}, {  30, -10}, {  31,  -4}, {  33,  -1}, {  33,   7},
        {  31,  12}, {  37,  23}, {  31,  38}, {  20,  64}, {  -9,  71}, {  -7,  37}, {  -8,  44}, { -11,  49},
        { -10,  56}, { -12,  59}, {  -8,  63}, {  -9,  67}, {  -6,  68}, { -10,  79}, {  -3,  78}, {  -8,  74},
        {  -9,  72}, { -10,  72}, { -18,  75}, { -12,  71}, { -11,  63}, {  -5,  70}, { -17,  75}, { -14,  72},
        { -16,  67}, {  -8,  53}, { -14,  59}, {  -9,  52}, { -11,  68}, {  -3,  78}, {  -8,  74}, {  -9,  72},
        { -10,  72}, { -18,  75}, { -12,  71}, { -11,  63}, {  -5,  70}, { -17,  75}, { -14,  72}, { -16,  67},
        {  -8,  53}, { -14,  59}, {  -9,  52}, { -11,  68}, {   9,  -2}, {  30, -10}, {  31,  -4}, {  33,  -1},
        {  33,   7}, {  31,  12}, {  37,  23}, {  31,  38}, {  20,  64}, {   9,  -2}, {  30, -10}, {  31,  -4},
        {  33,  -1}, {  33,   7}, {  31,  12}, {  37,  23}, {  31,  38}, {  20,  64}, {  -9,  71}, {  -7,  37},
        {  -8,  44}, { -11,  49}, { -10,  56}, { -12,  59}, {  -8,  63}, {  -9,  67}, {  -6,  68}, { -10,  79},
        { -22, 127}, { -25, 127}, { -25, 120}, { -27, 127}, { -19, 114}, { -23, 117}, { -25, 118}, { -26, 117},
        { -24, 113}, { -28, 118}, { -31, 120}, { -37, 124}, { -10,  94}, { -15, 102}, { -10,  99}, { -13, 106},
        { -50, 127}, {  -5,  92}, {  17,  57}, {  -5,  86}, { -13,  94}, { -12,  91}, {  -2,  77}, {   0,  71},
        {  -1,  73}, {   4,  64}, {  -7,  81}, {   5,  64}, {  15,  57}, {   1,  67}, {   0,  68}, { -10,  67},
        {   1,  68}, {   0,  77}, {   2,  64}, {   0,  68}, {  -5,  78}, {   7,  55}, {   5,  59}, {   2,  65},
        {  14,  54}, {  15,  44}, {   5,  60}, {   2,  70}, { -22, 127}, { -25, 127}, { -25, 120}, { -27, 127},
        { -19, 114}, { -23, 117}, { -25, 118}, { -26, 117}, { -24, 113}, { -28, 118}, { -31, 120}, { -37, 124},
        { -10,  94}, { -15, 102}, { -10,  99}, { -13, 106}, { -50, 127}, {  -5,  92}, {  17,  57}, {  -5,  86},
        { -13,  94}, { -12,  91}, {  -2,  77}, {   0,  71}, {  -1,  73}, {   4,  64}, {  -7,  81}, {   5,  64},
        {  15,  57}, {   1,  67}, {   0,  68}, { -10,  67}, {   1,  68}, {   0,  77}, {   2,  64}, {   0,  68},
        {  -5,  78}, {   7,  55}, {   5,  59}, {   2,  65}, {  14,  54}, {  15,  44}, {   5,  60}, {   2,  70},
        {  17, -13}, {  16,  -9}, {  17, -12}, {  27, -21}, {  37, -30}, {  41, -40}, {  42, -41}, {  48, -47},
        {  39, -32}, {  46, -40}, {  52, -51}, {  46, -41}, {  52, -39}, {  43, -19}, {  32,  11}, {  61, -55},
        {  56, -46}, {  62, -50}, {  81, -67}, {  45, -20}, {  35,  -2}, {  28,  15}, {  34,   1}, {  39,   1},
        {  30,  17}, {  20,  38}, {  18,  45}, {  15,  54}, {   0,  79}, {  36, -16}, {  37, -14}, {  37, -17},
        {  32,   1}, {  34,  15}, {  29,  15}, {  24,  25}, {  34,  22}, {  31,  16}, {  35,  18}, {  31,  28},
        {  33,  41}, {  36,  28}, {  27,  47}, {  21,  62}, {  17, -13}, {  16,  -9}, {  17, -12}, {  27, -21},
        {  37, -30}, {  41, -40}, {  42, -41}, {  48, -47}, {  39, -32}, {  46, -40}, {  52, -51}, {  46, -41},
        {  52, -39}, {  43, -19}, {  32,  11}, {  61, -55}, {  56, -46}, {  62, -50}, {  81, -67}, {  45, -20},
        {  35,  -2}, {  28,  15}, {  34,   1}, {  39,   1}, {  30,  17}, {  20,  38}, {  18,  45}, {  15,  54},
        {   0,  79}, {  36, -16}, {  37, -14}, {  37, -17}, {  32,   1}, {  34,  15}, {  29,  15}, {  24,  25},
        {  34,  22}, {  31,  16}, {  35,  18}, {  31,  28}, {  33,  41}, {  36,  28}, {  27,  47}, {  21,  62},
        { -24, 115}, { -22,  82}, {  -9,  62}, {   0,  53}, {   0,  59}, { -14,  85}, { -13,  89}, { -13,  94},
        { -11,  92}, { -29, 127}, { -21, 100}, { -14,  57}, { -12,  67}, { -11,  71}, { -10,  77}, { -21,  85},
        { -16,  88}, { -23, 104}, { -15,  98}, { -37, 127}, { -10,  82}, {  -8,  48}, {  -8,  61}, {  -8,  66},
        {  -7,  70}, { -14,  75}, { -10,  79}, {  -9,  83}, { -12,  92}, { -18, 108}, { -24, 115}, { -22,  82},
        {  -9,  62}, {   0,  53}, {   0,  59}, { -14,  85}, { -13,  89}, { -13,  94}, { -11,  92}, { -29, 127},
        { -21, 100}, { -14,  57}, { -12,  67}, { -11,  71}, { -10,  77}, { -21,  85}, { -16,  88}, { -23, 104},
        { -15,  98}, { -37, 127}, { -10,  82}, {  -8,  48}, {  -8,  61}, {  -8,  66}, {  -7,  70}, { -14,  75},
        { -10,  79}, {  -9,  83}, { -12,  92}, { -18, 108}, {  -5,  79}, { -11, 104}, { -11,  91}, { -30, 127},
        {  -5,  79}, { -11, 104}, { -11,  91}, { -30, 127}, {  -5,  79}, { -11, 104}, { -11,  91}, { -30, 127},
    }
};

const int16_t range_tab_lps[64][4] = {
    {128, 176, 208, 240}, {128, 167, 197, 227},
    {128, 158, 187, 216}, {123, 150, 178, 205},
    {116, 142, 169, 195}, {111, 135, 160, 185},
    {105, 128, 152, 175}, {100, 122, 144, 166},
    { 95, 116, 137, 158}, { 90, 110, 130, 150},
    { 85, 104, 123, 142}, { 81,  99, 117, 135},
    { 77,  94, 111, 128}, { 73,  89, 105, 122},
    { 69,  85, 100, 116}, { 66,  80,  95, 110},
    { 62,  76,  90, 104}, { 59,  72,  86,  99},
    { 56,  69,  81,  94}, { 53,  65,  77,  89},
    { 51,  62,  73,  85}, { 48,  59,  69,  80},
    { 46,  56,  66,  76}, { 43,  53,  63,  72},
    { 41,  50,  59,  69}, { 39,  48,  56,  65},
    { 37,  45,  54,  62}, { 35,  43,  51,  59},
    { 33,  41,  48,  56}, { 32,  39,  46,  53},
    { 30,  37,  43,  50}, { 29,  35,  41,  48},
    { 27,  33,  39,  45}, { 26,  31,  37,  43},
    { 24,  30,  35,  41}, { 23,  28,  33,  39},
    { 22,  27,  32,  37}, { 21,  26,  30,  35},
    { 20,  24,  29,  33}, { 19,  23,  27,  31},
    { 18,  22,  26,  30}, { 17,  21,  25,  28},
    { 16,  20,  23,  27}, { 15,  19,  22,  25},
    { 14,  18,  21,  24}, { 14,  17,  20,  23},
    { 13,  16,  19,  22}, { 12,  15,  18,  21},
    { 12,  14,  17,  20}, { 11,  14,  16,  19},
    { 11,  13,  15,  18}, { 10,  12,  15,  17},
    { 10,  12,  14,  16}, {  9,  11,  13,  15},
    {  9,  11,  12,  14}, {  8,  10,  12,  14},
    {  8,   9,  11,  13}, {  7,   9,  11,  12},
    {  7,   9,  10,  12}, {  7,   8,  10,  11},
    {  6,   8,   9,  11}, {  6,   7,   9,  10},
    {  6,   7,   8,  9},  {  2,   2,   2,   2},
};

const int8_t trans_idx_lps[64] = {
    0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9, 11, 11, 12,
    13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
    24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
    33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63,
};

const int8_t trans_idx_mps[64] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63,
};