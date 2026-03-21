//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_PREDUTIL_H
#define TOY_H264_PREDUTIL_H



/* prediction modes */

// intra
#define VERT_PRED              0
#define HOR_PRED               1
#define DC_PRED                2
#define PLANE_PRED             3
#define DIAG_DOWN_LEFT_PRED    3
#define DIAG_DOWN_RIGHT_PRED   4
#define VERT_RIGHT_PRED        5
#define HOR_DOWN_PRED          6
#define VERT_LEFT_PRED         7
#define HOR_UP_PRED            8

#define DC_PRED8x8             0
#define HOR_PRED8x8            1
#define VERT_PRED8x8           2
#define PLANE_PRED8x8          3


// inter
#define PRED_L0               10
#define PRED_L1               11
#define PRED_DIRECT           12
#define PRED_BIDIR            13





#endif //TOY_H264_PREDUTIL_H