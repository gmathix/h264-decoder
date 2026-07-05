//
// Created by gmathix on 6/26/26.
//

#ifndef H264_DECODER_DEBLOCK_H
#define H264_DECODER_DEBLOCK_H



#include "mb.h"
#include "picture.h"
#include "tests/profiler.h"



extern const uint8_t alpha_table[52];
extern const uint8_t beta_table[52];
extern const uint8_t treshold_table[3][52];



void deblock_picture(Picture *pic, CodecContext *ctx);
void deblock_macroblock(Picture *pic, int mbAddr, CodecContext *ctx);



void filter_row_luma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int y, uint8_t samples[24][24],
    uint8_t bs_list[4], int stride, CodecContext *ctx);
void filter_col_luma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int x, uint8_t samples[24][24],
    uint8_t bs_list[4], int stride, CodecContext *ctx);
void filter_row_chroma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int y, uint8_t samples[16][16],
    const uint8_t bs_list[4], int stride, CodecContext *ctx);
void filter_col_chroma(Picture *pic, int mbAddr, int mbAddrN, uint8_t *dst, int x, uint8_t samples[16][16],
    const uint8_t bs_list[4], int stride, CodecContext *ctx);



/*
 * filter 4-pixel edges individually for luma
 */
void filter_4p_vert_edge_low_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t bS, uint8_t indexA, uint8_t beta, uint8_t samples[24][24]);
void filter_4p_hor_edge_low_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t bS, uint8_t indexA, uint8_t beta, uint8_t samples[24][24]);
void filter_4p_vert_edge_high_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t alpha, uint8_t beta, uint8_t samples[24][24]);
void filter_4p_hor_edge_high_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t alpha, uint8_t beta, uint8_t samples[24][24]);

/*
 * filter 2-pixel edges individually for chroma
 */
void filter_2p_vert_edge_low_bS_chroma(int y, int x, const int filter_flags[2],
    uint8_t bS, uint8_t indexA, uint8_t samples[16][16]);
void filter_2p_hor_edge_low_bS_chroma(int y, int x, const int filter_flags[2],
    uint8_t bS, uint8_t indexA, uint8_t samples[16][16]);
void filter_2p_vert_edge_high_bS_chroma(int y, int x, const int filter_flags[2], uint8_t samples[16][16]);
void filter_2p_hor_edge_high_bS_chroma(int y, int x, const int filter_flags[2], uint8_t samples[16][16]);


void derive_edge_bS_list(int mbAddr, int mbAddrN, int blkIdx, int blkIdxN, int blkIdx8x8, int blkIdx8x8N,
    bool mb_edge, bool vertical, uint8_t bS_list[4], CodecContext *ctx);
void derive_edge_treshold_luma(Picture *pic, int mbAddr, int mbAddrN, uint8_t bS, int y, int x, bool vertical,
    uint8_t *alpha, uint8_t *beta, int filter_flags[4], uint8_t *indexA,
    uint8_t samples[24][24], CodecContext *ctx);
void derive_edge_treshold_chroma(Picture *pic, int mbAddr, int mbAddrN, uint8_t bS, int y, int x, bool vertical,
    uint8_t *alpha, uint8_t *beta, int filter_flags[2], uint8_t *indexA,
    uint8_t samples[16][16], CodecContext *ctx);

static ALWAYS_INLINE void fetch_24x24_luma_block(uint8_t block[24][24], int pos, int stride, Picture *pic) {
    int yc, xc;
    int posy = pos / stride;
    int posx = pos % stride;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 24; x++) {
            yc = _clip3(0, pic->heightY - 1, posy - 4 + y);
            xc = _clip3(0, pic->widthY - 1, posx - 4 + x);
            block[y][x] = pic->luma[yc*stride + xc];
        }
    }
}
static ALWAYS_INLINE void fetch_16x16_chroma_block(uint8_t cb_block[16][16], uint8_t cr_block[16][16], int pos, int stride, Picture *pic) {
    int yc, xc;
    int posy = pos / stride;
    int posx = pos % stride;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            yc = _clip3(0, pic->heightC - 1, posy - 4 + y);
            xc = _clip3(0, pic->widthC - 1, posx - 4 + x);
            cb_block[y][x] = pic->cb[yc*stride + xc];
            cr_block[y][x] = pic->cr[yc*stride + xc];
        }
    }
}


#endif //H264_DECODER_DEBLOCK_H