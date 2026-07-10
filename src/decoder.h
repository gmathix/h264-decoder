//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_DECODER_H
#define TOY_H264_DECODER_H



#include "global.h"



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


    int *seq_scaling_lists[8];
    int scalingList4x4[6][16];
    int scalingList8x8[2][64];
    bool seqScalingListPresent;
    bool useDefaultList4x4[6];
    bool useDefaultList8x8[2];
    int16_t (*levelScale4x4) [52][4][4];
    int16_t (*levelScale8x8) [52][8][8];



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
    struct {
        bool is_active; // implicitMode == 1 || explicitMode == 1
        int logWD[3];
        int weight[2][3];
        int offset[2][3];

        /* parsing */
        unsigned luma_log2_weight_denom;
        unsigned chroma_log2_weight_denom;
        int luma_weight[2][MAX_NUM_REF_PICTURES];
        int luma_offset[2][MAX_NUM_REF_PICTURES];
        int chroma_weight[2][MAX_NUM_REF_PICTURES][2];
        int chroma_offset[2][MAX_NUM_REF_PICTURES][2];
    } wpred ;





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