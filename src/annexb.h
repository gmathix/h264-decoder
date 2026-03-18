//
// Created by gmathix on 3/17/26.
//

#ifndef TOY_H264_ANNEXB_H
#define TOY_H264_ANNEXB_H

#include <stdlib.h>

#include "common.h"

int  count_nals(const uint8_t *buf, size_t size);
void fill_nal_units(const uint8_t *buf, size_t size, NalUnit *nal_array, int max_nals);
uint8_t *nal_to_rbsp(const uint8_t *buf, size_t size, size_t *rbsp_size);


#endif //TOY_H264_ANNEXB_H