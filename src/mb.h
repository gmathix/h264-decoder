//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_MB_H
#define TOY_H264_MB_H


#include <stdint.h>

#include "nal.h"
#include "slice.h"
#include "util/formulas.h"


typedef struct I_MbInfo {
    int type;
    int pred_mode;
    int cbp_chroma;
    int cbp_luma;
} I_MbInfo ;
extern const I_MbInfo i_mb_type_info[26];


typedef struct P_MbInfo {
    int type;
    uint8_t  part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} P_MbInfo ;
extern const P_MbInfo p_mb_type_info[5];
extern const P_MbInfo p_sub_mb_type_info[4];


typedef struct B_MbInfo {
    int type;
    uint8_t  part_count;
    uint8_t  mb_part_width;
    uint8_t  mb_part_height;
} B_MbInfo ;
extern const B_MbInfo b_mb_type_info[23];
extern const B_MbInfo b_sub_mb_type_info[13];


extern const int sub_width_c_info[4];
extern const int sub_height_c_info[4];


extern const int QPcTable[52];
extern const int luma_location_diff[4][2];
extern const uint8_t block_mapping_4x4[16];


extern int16_t luma_4x4_blk_mb_neighbors        [16][4];
extern int8_t  luma_4x4_blk_neighbor_idx        [16][4];
extern int8_t  luma_4x4_blk_neighbor_coords  [16][4][2];

extern int16_t chroma_4x4_blk_mb_neighbors       [4][4];
extern int8_t  chroma_4x4_blk_neighbor_idx       [4][4];
extern int8_t  chroma_4x4_blk_neighbor_coords [4][4][2];

extern int     neighbor_tables_initialized;
void           init_neighbor_tables(CodecContext *ctx);





typedef struct {
    int cbp_luma;
    int cbp_chroma;

    int16_t luma_total_coeffs[16]; /* FIXME: CABAC uses total_coeffs per 8x8 block
                                        needs a total_coeff array for every union
                                        fine for now as CAVLC doesnt support 8x8 transforms at all */
    union {
        /* Intra 4x4 / 8x8 */
        struct {
            int16_t luma_4x4_coeffs[16][16];
            int16_t luma_8x8_coeffs[4][64]; // just to avoid shit in residual_luma
        };
        /* Intra 16x16 */
        struct {
            int16_t luma_16x16_DC[16];
            int16_t luma_16x16_AC[16][15];
        };
    };
    int16_t chroma_total_coeffs[2][4];
    int16_t chroma_DC[2][4];
    int16_t chroma_AC[2][4][15];

} MacroblockResiduals ;


typedef struct Macroblock {
    struct Picture *p_frame;

    uint32_t slice_type;
    int table_idx;
    int mb_type;
    int pred_mode;

    int mbAddr;
    int mb_y, mb_x;
    int mb_width_c;
    int mb_height_c;

    int has_mb_a, has_mb_b, has_mb_c, has_mb_d;
    struct Macroblock *p_mb_a, *p_mb_b, *p_mb_c, *p_mb_d;

    uint32_t intra_chroma_pred_mode;
    int32_t mb_qp_delta, QPY, QPC;

    uint8_t intra4x4_pred_mode[16];
    uint8_t intra8x8_pred_mode[4];

    MacroblockResiduals residuals;


} Macroblock ;




typedef struct Neighbors {
    Macroblock *p_mb;

    Macroblock *mb_a, *mb_b, *mb_c, *mb_d;
    int8_t      a_idx, b_idx, c_idx, d_idx;
    Coord       cA,    cB,    cC,    cD;
    bool        a_av,  b_av,  c_av,  d_av;
} Neighbors ;


ALWAYS_INLINE Neighbors derive_neighbors_4x4_luma(Macroblock *mb, int blkIdx, CodecContext *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }
    Neighbors n = {0};

    int mb_a_off = luma_4x4_blk_mb_neighbors[blkIdx][0];
    int mb_b_off = luma_4x4_blk_mb_neighbors[blkIdx][1];
    int mb_c_off = luma_4x4_blk_mb_neighbors[blkIdx][2];
    int mb_d_off = luma_4x4_blk_mb_neighbors[blkIdx][3];

    n.mb_a = mb + mb_a_off;
    n.mb_b = mb + mb_b_off;
    n.mb_c = mb + mb_c_off;
    n.mb_d = mb + mb_d_off;

    n.a_idx = luma_4x4_blk_neighbor_idx[blkIdx][0];
    n.b_idx = luma_4x4_blk_neighbor_idx[blkIdx][1];
    n.c_idx = luma_4x4_blk_neighbor_idx[blkIdx][2];
    n.d_idx = luma_4x4_blk_neighbor_idx[blkIdx][3];

    n.cA = (Coord){luma_4x4_blk_neighbor_coords[blkIdx][0][1], luma_4x4_blk_neighbor_coords[blkIdx][0][0]};
    n.cB = (Coord){luma_4x4_blk_neighbor_coords[blkIdx][1][1], luma_4x4_blk_neighbor_coords[blkIdx][1][0]};
    n.cC = (Coord){luma_4x4_blk_neighbor_coords[blkIdx][2][1], luma_4x4_blk_neighbor_coords[blkIdx][2][0]};
    n.cD = (Coord){luma_4x4_blk_neighbor_coords[blkIdx][3][1], luma_4x4_blk_neighbor_coords[blkIdx][3][0]};

    n.a_av = mb_a_off == 0 || mb->has_mb_a;
    n.b_av = mb_b_off == 0 || mb->has_mb_b;
    n.d_av = n.a_av && n.b_av;
    n.c_av = (mb_c_off == 0 || mb->has_mb_c || (n.b_av && blkIdx!=3 && blkIdx!=7 && blkIdx!=11 && blkIdx!=15))
        && n.c_idx != -1;

    return n;
}


ALWAYS_INLINE Neighbors derive_neighbors_4x4_chroma(Macroblock *mb, int blkIdx, CodecContext *ctx) {
    if (!neighbor_tables_initialized) {
        init_neighbor_tables(ctx);
    }

    Neighbors n = {0};

    int mb_a_off = chroma_4x4_blk_mb_neighbors[blkIdx][0];
    int mb_b_off = chroma_4x4_blk_mb_neighbors[blkIdx][1];
    int mb_c_off = chroma_4x4_blk_mb_neighbors[blkIdx][2];
    int mb_d_off = chroma_4x4_blk_mb_neighbors[blkIdx][3];

    n.mb_a = mb + mb_a_off;
    n.mb_b = mb + mb_b_off;
    n.mb_c = mb + mb_c_off;
    n.mb_d = mb + mb_d_off;

    n.a_idx = chroma_4x4_blk_neighbor_idx[blkIdx][0];
    n.b_idx = chroma_4x4_blk_neighbor_idx[blkIdx][1];
    n.c_idx = chroma_4x4_blk_neighbor_idx[blkIdx][2];
    n.d_idx = chroma_4x4_blk_neighbor_idx[blkIdx][3];

    n.cB = (Coord){chroma_4x4_blk_neighbor_coords[blkIdx][0][1], chroma_4x4_blk_neighbor_coords[blkIdx][0][0]};
    n.cB = (Coord){chroma_4x4_blk_neighbor_coords[blkIdx][1][1], chroma_4x4_blk_neighbor_coords[blkIdx][1][0]};
    n.cC = (Coord){chroma_4x4_blk_neighbor_coords[blkIdx][2][1], chroma_4x4_blk_neighbor_coords[blkIdx][2][0]};
    n.cD = (Coord){chroma_4x4_blk_neighbor_coords[blkIdx][3][1], chroma_4x4_blk_neighbor_coords[blkIdx][3][0]};

    n.a_av = mb_a_off == 0 || mb->has_mb_a;
    n.b_av = mb_b_off == 0 || mb->has_mb_b;
    n.d_av = n.a_av && n.b_av;
    n.c_av = (mb_c_off == 0 || mb->has_mb_c || (n.b_av && blkIdx!=3))
    && n.c_idx != -1;



    return n;
}



typedef void (*decode_macroblock_func) (Macroblock *mb, struct Slice *slice, CodecContext *ctx);
typedef void (*residual_func)          (Macroblock *mb, int blkIdx, int iCbCr, int pbt, int16_t coeffLevel[], uint8_t (*total_coeffs_table)[16],
                                            int startIdx, int endIdx, int maxNumCoeff, bool isLuma, SliceHeader *sh, CodecContext *ctx);



void  read_macroblock        (Macroblock *mb_array, int mbAddr, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx);
void  read_mb_pred           (Macroblock *mb_array, int mbAddr, int type, int pred_mode, SliceHeader *sh, NalUnit *nal_unit, CodecContext *ctx);
void  read_residual          (Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int startIdx, int endIdx, int cbp_luma, int cbp_chroma,
                                 residual_func residual_block, SliceHeader *sh, CodecContext *ctx);

void  read_residual_luma     (Macroblock *mb_array, int mbAddr, int type, int t_8x8_flag, int cbp_luma,
                                int startIdx, int endIdx,
                                residual_func residual_block, SliceHeader *sh, CodecContext *ctx);

/* not cabac until i'm nuts */
void  residual_block_cabac   (Macroblock *mb, int blkIdx, int iCbCr, int pbt, int16_t coeffLevel[], uint8_t (*total_coeffs_table)[16], int startIdx, int endIdx, int maxNumCoeff, bool isLuma, SliceHeader *sh, CodecContext *ctx);


void decode_i_macroblock(Macroblock *mb, struct Slice *slice, CodecContext *ctx);
void decode_p_macroblock(Macroblock *mb, struct Slice *slice, CodecContext *ctx);
void decode_b_macroblock(Macroblock *mb, struct Slice *slice, CodecContext *ctx);







static Coord inverse_mb_part_scan(uint8_t mbPartIdx, uint8_t mbPartWidth, uint8_t mbPartHeight) {
    return (Coord) {
        _inverse_raster_scan(
            mbPartIdx,
            mbPartWidth, mbPartHeight,
            16, 0),

        _inverse_raster_scan(
            mbPartIdx,
            mbPartWidth, mbPartHeight,
            16, 1)
        };
}

static Coord inverse_sub_mb_part_scan(int32_t mb_type, uint8_t mbPartIdx, uint8_t subMbPartIdx, uint8_t subMbPartWidth, uint8_t subMpPartHeight) {
    return (Coord) {
        (mb_type == 3 || mb_type == 4 || mb_type == 22)
            ? _inverse_raster_scan(subMbPartIdx, subMbPartWidth, subMpPartHeight, 8, 0)
            : _inverse_raster_scan(subMbPartIdx, 4, 4, 8, 0),

        (mb_type == 3 || mb_type == 4 || mb_type == 22)
            ? _inverse_raster_scan(subMbPartIdx, subMbPartWidth, subMpPartHeight, 8, 1)
            : _inverse_raster_scan(subMbPartIdx, 4, 4, 8, 1)
        };
}

#endif //TOY_H264_MB_H