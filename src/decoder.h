//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_DECODER_H
#define TOY_H264_DECODER_H

#include "common.h"
#include "ps.h"



typedef struct CodecContext {
    bool initialized;

    const uint8_t   *data;
    size_t     size;
    BitReader *br;

    ParamSets *ps;

} CodecContext ;


CodecContext *decoder_init(const uint8_t *data, size_t size);
void decoder_run(CodecContext *context);



#endif //TOY_H264_DECODER_H