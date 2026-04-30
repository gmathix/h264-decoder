//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_NAL_H
#define TOY_H264_NAL_H



#include "common.h"
#include "decoder.h"



int dispatch_nal_unit(NalUnit *nal_unit, struct CodecContext *ctx);



#endif //TOY_H264_NAL_H