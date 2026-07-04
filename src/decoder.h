//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_DECODER_H
#define TOY_H264_DECODER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "mv.h"


typedef struct CodecContext {
    bool initialized;

    const uint8_t *data;
    size_t size;
    size_t global_bit_offset;


    int maxFrameNum;
    int maxLongTermFrameIdx;


    struct BitReader *br;
    struct ParamSets *ps;
    struct Picture   *curr_pic;
    struct Slice     *current_slice;
    struct DPB       *dpb;
    struct Profiler  *prf;


    int16_t levelScaleTable[52][4][4];
    int16_t  **weightScaleMatrix;



    /* metadata */

    bool mb_metadata_initialized;
    int  num_mbs;

    struct MacroblockMetadata *mb_metadata;
    uint8_t (*intra8x8_pred_modes) [ 4];
    uint8_t (*intra4x4_pred_modes) [16];
    uint8_t (*luma_total_coeffs)   [16];
    uint8_t (*cb_total_coeffs)     [16];
    uint8_t (*cr_total_coeffs)     [16];



    /* weighted prediction variables */
    bool wpred_active;; // implicitMode == 1 || explicitMode == 1
    int logWD[3], w0[3], w1[3], o0[3], o1[3];
    unsigned luma_log2_weight_denom;
    unsigned chroma_log2_weight_denom;
    int luma_weight_l0[MAX_NUM_REF_PICTURES];
    int luma_weight_l1[MAX_NUM_REF_PICTURES];
    int luma_offset_l0[MAX_NUM_REF_PICTURES];
    int luma_offset_l1[MAX_NUM_REF_PICTURES];
    int chroma_weight_l0[MAX_NUM_REF_PICTURES][2];
    int chroma_weight_l1[MAX_NUM_REF_PICTURES][2];
    int chroma_offset_l0[MAX_NUM_REF_PICTURES][2];
    int chroma_offset_l1[MAX_NUM_REF_PICTURES][2];





    /* helper buffers  */
    uint8_t ref_samples[9][9]; // used in inter_pred to store all the needed luma prediction samples for a 4x4 luma block


    struct Macroblock *prevMb;
    struct Macroblock *currMb;


    char *out_path;
    char *log_path;
    FILE *out_file;
    FILE *log_file;
    bool dump_monochrome;

} CodecContext ;


CodecContext *decoder_init(const uint8_t *data, size_t size, char *out_path, char *log_path, bool dump_monochrome);
void decoder_run(CodecContext *ctx);
void decoder_free_metadata(CodecContext *ctx);
void decoder_alloc_metadata(CodecContext *ctx);
void decoder_free(CodecContext *ctx);



#endif //TOY_H264_DECODER_H