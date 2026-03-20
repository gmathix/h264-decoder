//
// Created by gmathix on 3/12/26.
//

#ifndef TOY_H264_NAL_H
#define TOY_H264_NAL_H


#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "ps.h"


int dispatch_nal_unit(NalUnit *nal_unit, ParamSets *ps);



#endif //TOY_H264_NAL_H