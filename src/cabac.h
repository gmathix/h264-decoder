//
// Created by gmathix on 7/16/26.
//

#ifndef H264_DECODER_CABAC_H
#define H264_DECODER_CABAC_H


#include "mb.h"



extern const int8_t  cabac_init_vars[4][1024][2];
extern const int16_t range_tab_lps[64][4];
extern const int8_t  trans_idx_lps[64];
extern const int8_t  trans_idx_mps[64];

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




void cabac_init(Undo264Context *ctx);
int cabac_get_bit(Undo264Context *ctx, int ctxIdx);
int cabac_get_bit_term(Undo264Context *ctx, int ctxIdx);
int cabac_get_bit_bypass(Undo264Context *ctx);
void  residual_block_cabac   (Macroblock *mb, int blkIdx, int iCbCr, BlockType blockType, int16_t *coeffLevel, uint8_t (*total_coeffs_table)[16], int startIdx, int endIdx, int maxNumCoeff, bool isLuma, struct SliceHeader *sh, Undo264Context *ctx);


#endif //H264_DECODER_CABAC_H