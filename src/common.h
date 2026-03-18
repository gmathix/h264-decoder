//
// Created by gmathix on 3/13/26.
//

#ifndef TOY_H264_COMMON_H
#define TOY_H264_COMMON_H

#include <stdint.h>


/* Table 7.1 */
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


char *NalUnitTypeToString(uint8_t nal_unit_type);


typedef struct {
    const uint8_t *data; // pointer into original buffer (after start code)
    size_t        size;  // byte count AFTER emulation prevention removal
    uint8_t       ref_idc; // nal unit nal_ref_icd (bits 5-6 of first byte)
    uint8_t       type;  // nal_unit_type (low 5 bits of first byte)
} NalUnit;


#endif //TOY_H264_COMMON_H