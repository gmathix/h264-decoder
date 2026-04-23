//
// Created by gmathix on 3/17/26.
//

#ifndef TOY_H264_ANNEXB_H
#define TOY_H264_ANNEXB_H

#include <stdlib.h>

#include "common.h"
#include "util/bitreader.h"


int     count_nals     (const uint8_t *buf, size_t size);
void    fill_nal_units (const uint8_t *buf, size_t size, NalUnit *p_nalArray, int maxNals);
uint8_t *nal_to_rbsp   (const uint8_t *buf, size_t size, size_t *p_rbspSize);
NalUnit *next_nal_unit  (BitReader *br);

#endif //TOY_H264_ANNEXB_H