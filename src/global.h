//
// Created by gmathix on 4/6/26.
//

#ifndef TOY_H264_GLOBAL_H
#define TOY_H264_GLOBAL_H



#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <limits.h>
#include <assert.h>


#define ALL_LOG     0

#define CAVLC_LOG   (ALL_LOG    |   0)
#define CABAC_LOG   (ALL_LOG    |   0) // be careful with this one, won't run at more than 2fps
#define NAL_LOG     (ALL_LOG    |   0)


#define always_inline inline __attribute__((always_inline))
#define OPTIMIZE_O0   __attribute__((optimize("O0")))
#define OPTIMIZE_O1   __attribute__((optimize("O1")))
#define OPTIMIZE_O2   __attribute__((optimize("O2")))
#define OPTIMIZE_O3   __attribute__((optimize("O3")))





#define MAX_NUM_REF_PICTURES        32
#define MAX_DPB_SIZE     33
#define L0  0
#define L1  1

#define MAX_U8 255



extern int debugging;
extern int frame_debug;
extern int mb_debug;
extern int nb_frames_before_stop;
extern int frame_num_debug;
extern int poc_debug;





enum NalUnitType : uint8_t {
    NAL_UNSPECIFIED = 0,
    NAL_CODED_SLICE_OF_NON_IDR_PICTURE = 1,
    NAL_CODED_SLICE_DATA_PARTITION_A = 2,
    NAL_CODED_SLICE_DATA_PARTITION_B = 3,
    NAL_CODED_SLICE_DATA_PARTITION_C = 4,
    NAL_CODED_SLICE_OF_IDR_PICTURE = 5,
    NAL_SEI = 6,
    NAL_SPS = 7,
    NAL_PPS = 8,
    NAL_AUD = 9,
    NAL_EOSEQ = 10,
    NAL_EOSTREAM = 11,
    NAL_FILLER_DATA = 12,
    NAL_SPS_EXTENSION = 13,
    NAL_PREFIX = 14,
    NAL_SUBSET_SPS = 15,

    // 16..18 : reserved
    NAL_RS16 = 16,
    NAL_RS17 = 17,
    NAL_RS18 = 18,

    NAL_CODED_SLICE_OF_AUX_CODED_PICTURE = 19,
    NAL_CODED_SLICE_EXTENSION = 20,

    // 21..23 : reserved
    NAL_RS21 = 21,
    NAL_RS22 = 22,
    NAL_RS23 = 23,

    // 24..31 : unspecified
    NAL_UNSPEC24 = 24,
    NAL_UNSPEC25 = 25,
    NAL_UNSPEC26 = 26,
    NAL_UNSPEC27 = 27,
    NAL_UNSPEC28 = 28,
    NAL_UNSPEC29 = 29,
    NAL_UNSPEC30 = 30,
    NAL_UNSPEC31 = 31,
} ;


enum Profile : uint8_t {
    PROFILE_CAVLC_444       =  44,
    PROFILE_BASELINE        =  66,
    PROFILE_MAIN            =  77,
    PROFILE_EXTENDED        =  88,
    PROFILE_HIGH            = 100,
    PROFILE_HIGH_10         = 110,
    PROFILE_HIGH_422        = 122,
    PROFILE_HIGH_PRED_444   = 244,
};

enum {
    MMCO_END_LOOP                   = 0,
    MMCO_MARK_SHORT_TERM_UNUSED     = 1,
    MMCO_MARK_LONG_TERM_UNUSED      = 2,
    MMCO_MARK_LONG_TERM_ASSIGN      = 3,
    MMCO_MARK_OOB_LONG_TERM_UNUSED  = 4,
    MMCO_RESET                      = 5,
    MMCO_MARK_CURR_AS_LONG_TERM     = 6
};


// this also directly maps to ctxBlockCat
typedef enum {
    LUMA_INTRA_16x16_DC_LEVEL,
    LUMA_INTRA_16x16_AC_LEVEL,
    LUMA_LEVEL_4x4,
    CHROMA_DC_LEVEL,
    CHROMA_AC_LEVEL,
    LUMA_LEVEL_8x8,
    CB_INTRA_16x16_DC_LEVEL,
    CB_INTRA_16x16_AC_LEVEL,
    CB_LEVEL_4x4,
    CB_LEVEL_8x8,
    CR_INTRA_16x16_DC_LEVEL,
    CR_INTRA_16x16_AC_LEVEL,
    CR_LEVEL_4x4,
    CR_LEVEL_8x8,
} BlockType ;

typedef enum {
    BLOCK_16x16, BLOCK_16x8, BLOCK_8x16,
    BLOCK_8x8, BLOCK_8x4, BLOCK_4x8,
    BLOCK_4x4, BLOCK_4x2, BLOCK_2x4, BLOCK_2x2
} BlockShape ;
#define NUM_BLOCK_SHAPES_LUMA 7
#define NUM_BLOCK_SHAPES_CHROMA 7
#define NUM_BLOCK_SHAPES 10

char* blockType_to_string(int bt);
extern char* profile_to_string(int profile);


typedef struct NalUnit {
    const uint8_t *data;   // pointer into original buffer (after start code)
    size_t        size;    // byte count AFTER emulation prevention removal
    uint8_t       ref_idc; // nal_ref_idc (bits 5-6 of first byte)
    uint8_t       type;    // nal_unit_type (low 5 bits of first byte)
} NalUnit;

typedef struct Coord {
    int16_t x;
    int16_t y;
} Coord ;



#endif //TOY_H264_GLOBAL_H