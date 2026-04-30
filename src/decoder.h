//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_DECODER_H
#define TOY_H264_DECODER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


extern int debugging;

typedef struct CodecContext {
    bool initialized;

    const uint8_t *data;
    size_t size;
    size_t global_bit_offset;


    int maxFrameNum;
    int maxLongTermFrameIdx;


    struct BitReader *br;
    struct ParamSets *ps;
    struct Picture   *current_pic;
    struct Slice     *current_slice;
    struct DPB       *dpb;
    struct Profiler  *prf;


    int16_t levelScaleTable[52][4][4];
    int16_t  **weightScaleMatrix;


    /* MB metadata */
    bool mb_metadata_initialized;
    int  num_mbs;

    int32_t  *mb_types;
    uint8_t (*intra8x8_pred_modes) [ 4];
    uint8_t (*intra4x4_pred_modes) [16];
    uint8_t (*luma_total_coeffs)   [16];
    uint8_t (*cb_total_coeffs)     [16];
    uint8_t (*cr_total_coeffs)     [16];

    struct Macroblock *prevMb;
    struct Macroblock *currMb;


    char *out_path;
    FILE *out_file;

} CodecContext ;


CodecContext *decoder_init(const uint8_t *data, size_t size, char *out_path);
void decoder_run(CodecContext *context);
void decoder_free(CodecContext *ctx);



#endif //TOY_H264_DECODER_H