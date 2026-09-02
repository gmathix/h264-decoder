//
// Created by gmathix on 3/20/26.
//

#ifndef TOY_H264_DECODER_H
#define TOY_H264_DECODER_H



#include "global.h"


typedef struct Undo264Context {
    bool initialized;

    const uint8_t *data;
    size_t size;
    size_t global_bit_offset;


    int maxFrameNum;
    int maxLongTermFrameIdx;

    struct CabacContext *cactx;
    struct BitReader    *br;
    struct ParamSets    *ps;
    struct Picture      *curr_pic;
    struct Slice        *current_slice;
    struct DPB          *dpb;
    struct Profiler     *prf;
    struct PicturePool  *pool;
    struct DSPContext   *dsp;

    bool pic_pool_initialized;


    // scaling lists and levelScale tables
    bool seqScalingMatrixPresent;
    bool useDefaultList4x4[6];
    bool useDefaultList8x8[2];

    // sequence-level scaling lists
    int16_t seqScalingList4x4[6][16];
    int16_t seqScalingList8x8[2][64];

    int16_t scalingList4x4[6][16];
    int16_t scalingList8x8[2][64];

    int16_t (*levelScale4x4) [52][4][4];
    int16_t (*levelScale8x8) [52][8][8];



    /* metadata */

    bool mb_metadata_initialized;
    int  num_mbs;

    struct MacroblockMetadata *mb_metadata;
    uint8_t (*total_coeffs)           [24]; // 16 values for luma, 4 values for Cb, 4 values for Cr


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





    /* helper inter pred buffers  */

    /* reference sample buffers for every MB partition dimension
     * needs 5 extra pixel length for qpel */
    uint8_t buf_16x16 [(16+5) * (16+5)];
    uint8_t buf_16x8  [(16+5) * (8+5)];
    uint8_t buf_8x8   [(8+5)  * (8+5)];
    uint8_t buf_8x4   [(8+5)  * (4+5)];
    uint8_t buf_4x4   [(4+5)  * (4+5)];
    uint8_t buf_4x2   [(4+5)  * (2+5)];
    uint8_t buf_2x2   [(2+5)  * (2+5)];
    uint8_t *mc_scratch_buffers[64]; // mc_scratch_buffers[(W * H) / 4 - 1] = &buf_WxH (same as buf_HxW)

    /* temp buffers for bipred sample accumulation */
    uint8_t temp_bi_buf_16x16 [2 * 256];
    uint8_t temp_bi_buf_16x8  [2 * 128];
    uint8_t temp_bi_buf_8x8   [2 *  64];
    uint8_t temp_bi_buf_8x4   [2 *  32];
    uint8_t temp_bi_buf_4x4   [2 *  16];
    uint8_t temp_bi_buf_4x2   [2 *   8];
    uint8_t temp_bi_buf_2x2   [2 *   2];
    uint8_t *mc_temp_bi_buffers[64]; // same idea as mc_scratch_buffers

    /* temp buffers for qpel horizontal/vertical pass pre-computation */
    int16_t qpel_pass_buf_16 [16+5];
    int16_t qpel_pass_buf_8  [ 8+5];
    int16_t qpel_pass_buf_4  [ 4+5];
    int16_t *qpel_pass_buffers[4];


    struct Macroblock *scratchMb;
    struct Macroblock *currMb;
    int8_t prevQPY;


    char *out_path;
    char *log_path;
    FILE *out_file;
    FILE *log_file;
    bool dump_monochrome;

} Undo264Context ;


Undo264Context *decoder_init(const uint8_t *data, size_t size, char *out_path, char *log_path, bool dump_monochrome);
void decoder_run(Undo264Context *ctx);
void decoder_free_metadata(Undo264Context *ctx);
void decoder_alloc_metadata(Undo264Context *ctx);
void decoder_free(Undo264Context *ctx);



#endif //TOY_H264_DECODER_H