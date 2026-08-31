//
// Created by gmathix on 7/16/26.
//

#ifndef H264_DECODER_CABAC_H
#define H264_DECODER_CABAC_H


#include "mb.h"



extern const int8_t  cabac_init_vars[4][1024][2];
extern const int16_t range_tab_lps[64][4];
extern const int8_t  trans_idx[2][64];

extern int8_t p_state_idx[1024];
extern int8_t val_mps[1024];

typedef struct CabacContext {
    int16_t codIRange;
    int16_t codIOffset;

    // pointers to arrays
    int8_t *p_state_idx_ptr;
    int8_t *val_mps_ptr;
} CabacContext ;





static CabacContext *make_cactx() {
    CabacContext *cactx = calloc(1, sizeof(CabacContext));
    return cactx;
}
static void free_cactx(CabacContext *cactx) {
    free(cactx);
}




void cabac_init(const Undo264Context *ctx);
void cabac_init_ctx_vars(Slice *slice, int idc);
void cabac_init_engine(const Undo264Context *ctx);


always_inline int renorm(int bin, CabacContext *cactx, const Undo264Context *ctx) {
    while (cactx->codIRange < 256) {
        cactx->codIRange <<= 1;
        cactx->codIOffset = (cactx->codIOffset << 1) | read_u(ctx->br, 1);
    }

    return bin;
}

always_inline int cabac_get_bit(const Undo264Context *ctx, int ctxIdx) {
    CabacContext *cactx = ctx->cactx;


    int qCodIRangeIdx = (cactx->codIRange >> 6) & 3;
    int8_t pStateIdx = p_state_idx[ctxIdx];
    int8_t valMPS = val_mps[ctxIdx];
    int16_t codIRangeLPS = range_tab_lps[pStateIdx][qCodIRangeIdx];

    cactx->codIRange -= codIRangeLPS;


    int bin;

    if (cactx->codIOffset >= cactx->codIRange) {
        bin = 1 - valMPS;
        cactx->codIOffset -= cactx->codIRange;
        cactx->codIRange = codIRangeLPS;
        if (pStateIdx == 0) {
            val_mps[ctxIdx] = 1 - valMPS;
        }
        p_state_idx[ctxIdx] = trans_idx[1][pStateIdx];
    } else {
        bin = valMPS;
        p_state_idx[ctxIdx] = trans_idx[0][pStateIdx];
    }


    return renorm(bin, cactx, ctx);
}



int cabac_get_bit_term(const Undo264Context *ctx);
int cabac_get_bit_bypass(const Undo264Context *ctx);
void  residual_block_cabac   (Macroblock *mb, int blkIdx, int iCbCr, BlockType blockType, int16_t *coeffLevel,
    int startIdx, int endIdx, int maxNumCoeff, bool isLuma,
    struct SliceHeader *sh, const Undo264Context *ctx);


#endif //H264_DECODER_CABAC_H