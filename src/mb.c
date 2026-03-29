//
// Created by gmathix on 3/21/26.
//

#include "mb.h"

#include <threads.h>
#include <unistd.h>

#include "util/expgolomb.h"
#include "util/formulas.h"
#include "util/mbutil.h"
#include "util/predutil.h"


/* table 7-11 */
const I_MbInfo i_mb_type_info[26] = {
    /*  0 */ {MB_TYPE_INTRA4x4,   -1,        -1, -1, },
    /*  1 */ {MB_TYPE_INTRA16x16, VERT_PRED,  0,  0, },
    /*  2 */ {MB_TYPE_INTRA16x16, HOR_PRED,   0,  0, },
    /*  3 */ {MB_TYPE_INTRA16x16, DC_PRED,    0,  0, },
    /*  4 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 0,  0, },
    /*  5 */ {MB_TYPE_INTRA16x16, VERT_PRED,  1,  0, },
    /*  6 */ {MB_TYPE_INTRA16x16, HOR_PRED,   1,  0, },
    /*  7 */ {MB_TYPE_INTRA16x16, DC_PRED,    1,  0, },
    /*  8 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 1,  0, },
    /*  9 */ {MB_TYPE_INTRA16x16, VERT_PRED,  2,  0, },
    /* 10 */ {MB_TYPE_INTRA16x16, HOR_PRED,   2,  0, },
    /* 11 */ {MB_TYPE_INTRA16x16, DC_PRED,    2,  0, },
    /* 12 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 2,  0, },
    /* 13 */ {MB_TYPE_INTRA16x16, VERT_PRED,  0, 15, },
    /* 14 */ {MB_TYPE_INTRA16x16, HOR_PRED,   0, 15, },
    /* 15 */ {MB_TYPE_INTRA16x16, DC_PRED,    0, 15, },
    /* 16 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 0, 15, },
    /* 17 */ {MB_TYPE_INTRA16x16, VERT_PRED,  1, 15, },
    /* 18 */ {MB_TYPE_INTRA16x16, HOR_PRED,   1, 15, },
    /* 19 */ {MB_TYPE_INTRA16x16, DC_PRED,    1, 15, },
    /* 20 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 1, 15, },
    /* 21 */ {MB_TYPE_INTRA16x16, VERT_PRED,  2, 15, },
    /* 22 */ {MB_TYPE_INTRA16x16, HOR_PRED,   2, 15, },
    /* 23 */ {MB_TYPE_INTRA16x16, DC_PRED,    2, 15, },
    /* 24 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 2, 15, },
    /* 25 */ {MB_TYPE_INTRA_PCM,  -1,        -1, -1, }
};


/* table 7-13 */
const P_MbInfo p_mb_type_info[5] = {
    /*  0 */{ MB_TYPE_16x16 | MB_TYPE_P0L0,                               1, 16, 16},
    /*  1 */{ MB_TYPE_16x8  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2, 16,  8},
    /*  2 */{ MB_TYPE_8x16  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2,  8, 16},
    /*  3 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0,                4,  8,  8},
    /*  4 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_REF0, 4,  8,  8},
};

/* table 7-17 */
const P_MbInfo p_sub_mb_type_info[4] = {
    /*  0 */ {SUB_MB_TYPE_8x8 | MB_TYPE_P0L0, 1, 8, 8},
    /*  1 */ {SUB_MB_TYPE_8x4 | MB_TYPE_P0L0, 2, 8, 4},
    /*  2 */ {SUB_MB_TYPE_4x8 | MB_TYPE_P0L0, 2, 4, 8},
    /*  3 */ {SUB_MB_TYPE_4x4 | MB_TYPE_P0L0, 4, 4, 4},
};



/* table 7-14 */
const B_MbInfo b_mb_type_info[23] = {
    /*  0 */{ MB_TYPE_DIRECT2 | MB_TYPE_L0L1,                                              1,  8,  8},
    /*  1 */{ MB_TYPE_16x16   | MB_TYPE_P0L0,                                              1, 16, 16},
    /*  2 */{ MB_TYPE_16x16   | MB_TYPE_P0L1,                                              1, 16, 16},
    /*  3 */{ MB_TYPE_16x16   | MB_TYPE_P0L0 | MB_TYPE_P0L1,                               1, 16, 16},
    /*  4 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, 16, 16},
    /*  5 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, 16,  8},
    /*  6 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2,  8, 16},
    /*  7 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2, 16,  8},
    /*  8 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2,  8, 16},
    /*  9 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2, 16,  8},
    /* 10 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2,  8, 16},
    /* 11 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2, 16,  8},
    /* 12 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2,  8, 16},
    /* 13 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, 16,  8},
    /* 14 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2,  8, 16},
    /* 15 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, 16,  8},
    /* 16 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2,  8, 16},
    /* 17 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2, 16,  8},
    /* 18 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2,  8, 16},
    /* 19 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2, 16,  8},
    /* 20 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2,  8, 16},
    /* 21 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2, 16,  8},
    /* 22 */{ MB_TYPE_8x8     | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 4,  8,  8},
};


/* table 7-18 */
const B_MbInfo b_sub_mb_type_info[13] = {
    /*  0 */ {SUB_MB_TYPE_DIRECT | SUB_MB_TYPE_8x8 | PRED_DIRECT, 4, 4, 4},
    /*  1 */ {SUB_MB_TYPE_8x8    | MB_TYPE_P0L0,                     1, 8, 8},
    /*  2 */ {SUB_MB_TYPE_8x8    | MB_TYPE_P0L1,                     1, 8, 8},
    /*  3 */ {SUB_MB_TYPE_8x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      1, 8, 8},
    /*  4 */ {SUB_MB_TYPE_8x4    | MB_TYPE_P0L0,                     2, 8, 4},
    /*  5 */ {SUB_MB_TYPE_4x8    | MB_TYPE_P0L0,                     2, 4, 8},
    /*  6 */ {SUB_MB_TYPE_8x4    | MB_TYPE_P0L1,                     2, 8, 4},
    /*  7 */ {SUB_MB_TYPE_4x8    | MB_TYPE_P0L1,                     2, 4, 8},
    /*  8 */ {SUB_MB_TYPE_8x4    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      2, 8, 4},
    /*  9 */ {SUB_MB_TYPE_4x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      2, 4, 8},
    /* 10 */ {SUB_MB_TYPE_4x4    | MB_TYPE_P0L0,                     4, 4, 4},
    /* 11 */ {SUB_MB_TYPE_4x4    | MB_TYPE_P0L1,                     4, 4, 4},
    /* 12 */ {SUB_MB_TYPE_4x4    | MB_TYPE_P0L0 | MB_TYPE_P0L1,      4, 4, 4},

};



/* 7.3.5 */
void decode_macroblock(SliceHeader *s_h, NalUnit *nal_unit, BitReader *br, ParamSets *ps) {
    PPS *pps = ps->pps_list[s_h->pps_id];
    SPS *sps = ps->sps_list[pps->sps_id];

    uint32_t mb_type = read_ue(br);

    uint8_t pcm_samples_luma[256];
    uint8_t pcm_samples_chroma[256];


    uint8_t type = IS_I_SLICE(s_h->slice_type)
        ? i_mb_type_info[mb_type].type
        : IS_P_SLICE(s_h->slice_type)
            ? p_mb_type_info[mb_type].type
            : IS_B_SLICE(s_h->slice_type)
                ? b_mb_type_info[mb_type].type
                : -1;

    uint8_t pred_mode = IS_I_SLICE(s_h->slice_type)
        ? i_mb_type_info[mb_type].pred_mode
        : -1;

    int cbp_luma = IS_I_SLICE(s_h->slice_type)
        ? i_mb_type_info[mb_type].cbp_luma
        : -1;

    int cbp_chroma = IS_I_SLICE(s_h->slice_type)
        ? i_mb_type_info[mb_type].cbp_chroma
        : -1;


    printf("%s ", mb_type_to_string(type));


    if (type == MB_TYPE_INTRA_PCM) {
        while (!bitreader_byte_aligned(br)) {
            bitreader_skip_bits(br, 1);

        }
        for (int i = 0; i < 256; i++) {
            pcm_samples_luma[i] = read_u(br, 8);
        }
    } else {
        int transform_size_8x8_flag = 0;
        int noSubMbPartSizeLessThan8x8Flag = 1;
        if (type != MB_TYPE_INTRA4x4 && type != MB_TYPE_INTRA16x16) {

        } else {
            if (pps->transform_8x8_mode_flag && type == MB_TYPE_INTRA4x4) {
                transform_size_8x8_flag = read_u(br, 1);
            }
            mb_pred(type, s_h, nal_unit, br, ps);
        }

        if (!IS_INTRA16x16(type)) {
            int32_t cbp = map_coded_block_pattern(read_ue(br), sps->chroma_format_idc,
                IS_INTRA4x4(type));
            printf("cbp:%d", cbp);

            if (cbp_luma > 0 && transform_size_8x8_flag &&
                !IS_INTRA4x4(type) && noSubMbPartSizeLessThan8x8Flag &&
                (type != (MB_TYPE_DIRECT2 | MB_TYPE_L0L1) || sps->direct_8x8_inference_flag)) {

                int transform_size_8x8_flag = read_u(br, 1);
            }
        }

        if (IS_INTRA16x16(type)) {
            int32_t mb_qp_delta = read_se(br);
            printf("ASDFASDF\n");
            /* residual (0, 15) */
        }
    };
}


/* 7.3.5.1 */
void mb_pred(int type, SliceHeader *s_h, NalUnit *nal_unit, BitReader *br, ParamSets *ps) {
    if (type == MB_TYPE_INTRA4x4 ||
        type == MB_TYPE_INTRA8x8 ||
        type == MB_TYPE_INTRA16x16) {

        if (type == MB_TYPE_INTRA4x4) {
            uint8_t prev_intra4x4_pred_mode_flag[16];
            uint8_t rem_intra4x4_pred_mode[16];
            for (int luma4x4BlkIdx = 0; luma4x4BlkIdx < 16; luma4x4BlkIdx++) {
                prev_intra4x4_pred_mode_flag[luma4x4BlkIdx] = read_u(br, 1);
                if (!prev_intra4x4_pred_mode_flag[luma4x4BlkIdx]) {
                    rem_intra4x4_pred_mode[luma4x4BlkIdx] = read_u(br, 3);
                }
            }
        }

        if (type == MB_TYPE_INTRA8x8) {
            uint8_t prev_intra8x8_pred_mode_flag[16];
            uint8_t rem_intra8x8_pred_mode[16];
            for (int luma8x8BlkIdx = 0; luma8x8BlkIdx < 16; luma8x8BlkIdx++) {
                prev_intra8x8_pred_mode_flag[luma8x8BlkIdx] = read_u(br, 1);
                if (!prev_intra8x8_pred_mode_flag[luma8x8BlkIdx]) {
                    rem_intra8x8_pred_mode[luma8x8BlkIdx] = read_u(br, 3);
                }
            }
        }

        if (s_h->sps->chroma_format_idc == 1 || s_h->sps->chroma_format_idc == 2) {
            uint32_t intra_chroma_pred_mode = read_ue(br);
        }
    } else if (type != MB_TYPE_DIRECT2) {

    }
}


/* 7.3.5.2 */
void residual(int startIdx, int endIdx, SliceHeader *s_h, BitReader *br, ParamSets *ps) {
    /* fuck the table is too scary and it's 3 AM so i'll just fucking go to bed and do this shit tomorrow
     * why do i fucking need to implement a fucking 500 line grammar parser which i 99% dont need for now
     * just to fucking not desync because of the fucking 16x16 intra blocks
     * future me, if you made it past this project, i'm sofucking proud of you rn */

}