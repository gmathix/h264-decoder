//
// Created by gmathix on 3/21/26.
//

#include "mb.h"
#include "util/mbutil.h"
#include "util/predutil.h"


/* table 7-11 */
const I_MbInfo i_mb_type_info[26] = {
    /*  0 */ {MB_TYPE_INTRA4x4,   -1,         -1,-1},
    /*  1 */ {MB_TYPE_INTRA16x16, VERT_PRED,  0, 0},
    /*  2 */ {MB_TYPE_INTRA16x16, HOR_PRED,   0, 0},
    /*  3 */ {MB_TYPE_INTRA16x16, DC_PRED,    0, 0},
    /*  4 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 0, 0},
    /*  5 */ {MB_TYPE_INTRA16x16, VERT_PRED,  1, 0},
    /*  6 */ {MB_TYPE_INTRA16x16, HOR_PRED,   1, 0},
    /*  7 */ {MB_TYPE_INTRA16x16, DC_PRED,    1, 0},
    /*  8 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 1, 0},
    /*  9 */ {MB_TYPE_INTRA16x16, VERT_PRED,  2, 0},
    /* 10 */ {MB_TYPE_INTRA16x16, HOR_PRED,   2, 0},
    /* 11 */ {MB_TYPE_INTRA16x16, DC_PRED,    2, 0},
    /* 12 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 2, 0},
    /* 13 */ {MB_TYPE_INTRA16x16, VERT_PRED,  0, 15},
    /* 14 */ {MB_TYPE_INTRA16x16, HOR_PRED,   0, 15},
    /* 15 */ {MB_TYPE_INTRA16x16, DC_PRED,    0, 15},
    /* 16 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 0, 15},
    /* 17 */ {MB_TYPE_INTRA16x16, VERT_PRED,  1, 15},
    /* 18 */ {MB_TYPE_INTRA16x16, HOR_PRED,   1, 15},
    /* 19 */ {MB_TYPE_INTRA16x16, DC_PRED,    1, 15},
    /* 20 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 1, 15},
    /* 21 */ {MB_TYPE_INTRA16x16, VERT_PRED,  2, 15},
    /* 22 */ {MB_TYPE_INTRA16x16, HOR_PRED,   2, 15},
    /* 23 */ {MB_TYPE_INTRA16x16, DC_PRED,    2, 15},
    /* 24 */ {MB_TYPE_INTRA16x16, PLANE_PRED, 2, 15},
    /* 25 */ {MB_TYPE_INTRA_PCM,  -1,        -1, -1 }
};


/* table 7-13 */
const P_MbInfo p_mb_type_info[5] = {
    /*  0 */{ MB_TYPE_16x16 | MB_TYPE_P0L0,                               1 },
    /*  1 */{ MB_TYPE_16x8  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2 },
    /*  2 */{ MB_TYPE_8x16  | MB_TYPE_P0L0 | MB_TYPE_P1L0,                2 },
    /*  3 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0,                4 },
    /*  4 */{ MB_TYPE_8x8   | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_REF0, 4 },
};



/* table 7-14 */
const B_MbInfo b_mb_type_info[23] = {
    /*  0 */{ MB_TYPE_DIRECT2 | MB_TYPE_L0L1,                                              1, },
    /*  1 */{ MB_TYPE_16x16   | MB_TYPE_P0L0,                                              1, },
    /*  2 */{ MB_TYPE_16x16   | MB_TYPE_P0L1,                                              1, },
    /*  3 */{ MB_TYPE_16x16   | MB_TYPE_P0L0 | MB_TYPE_P0L1,                               1, },
    /*  4 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, },
    /*  5 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0,                               2, },
    /*  6 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2, },
    /*  7 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L1,                               2, },
    /*  8 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2, },
    /*  9 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L1,                               2, },
    /* 10 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2, },
    /* 11 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0,                               2, },
    /* 12 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, },
    /* 13 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, },
    /* 14 */{ MB_TYPE_16x8    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, },
    /* 15 */{ MB_TYPE_8x16    | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1,                2, },
    /* 16 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2, },
    /* 17 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0,                2, },
    /* 18 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2, },
    /* 19 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L1,                2, },
    /* 20 */{ MB_TYPE_16x8    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2, },
    /* 21 */{ MB_TYPE_8x16    | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 2, },
    /* 22 */{ MB_TYPE_8x8     | MB_TYPE_P0L0 | MB_TYPE_P0L1 | MB_TYPE_P1L0 | MB_TYPE_P1L1, 4, },
};


