//
// Created by gmathix on 4/30/26.
//

#ifndef TOY_H264_MV_H
#define TOY_H264_MV_H



#include "decoder.h"

#include <stdint.h>


typedef struct MotionVector {
    uint8_t ref_idx;
    int16_t x;
    int16_t y;
} MotionVector ;


#endif //TOY_H264_MV_H