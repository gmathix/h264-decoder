//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_DECODER_H
#define TOY_H264_DECODER_H

#include "common.h"
#include "ps.h"
#include "tests/profiler.h"


extern int debugging;

typedef struct CodecContext {
    bool initialized;

    const uint8_t *data;
    size_t size;
    BitReader *br;
    size_t global_bit_offset;

    ParamSets *ps;

    struct Picture *current_pic;
    struct Slice   *current_slice;


    int16_t levelScaleTable[52][4][4];
    int16_t  **weightScaleMatrix;


    Profiler *prf;
    char *out_path;
    FILE *out_file;
} CodecContext ;


CodecContext *decoder_init(const uint8_t *data, size_t size, char *out_path);
void decoder_run(CodecContext *context);
void decoder_free(CodecContext *ctx);



#endif //TOY_H264_DECODER_H