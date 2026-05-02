//
// Created by gmathix on 4/30/26.
//

#ifndef TOY_H264_MV_H
#define TOY_H264_MV_H

#include <stdint.h>


typedef struct MotionVector {
    uint8_t ref_idx;
    int16_t x;
    int16_t y;
} MotionVector ;

static MotionVector MV_ZERO = {0, 0, 0};

#endif //TOY_H264_MV_H