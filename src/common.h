//
// Created by smegmuss on 3/13/26.
//

#ifndef TOY_H264_COMMON_H
#define TOY_H264_COMMON_H
#include <stddef.h>
#include <stdint.h>


/* Table 7.1 */
enum NalUnitType : uint8_t {
    UNSPECIFIED = 0,
    CODED_SLICE_OF_NON_IDR_PICTURE = 1,
    CODED_SLICE_DATA_PARTITION_A = 2,
    CODED_SLICE_DATA_PARTITION_B = 3,
    CODED_SLICE_DATA_PARTITION_C = 4,
    CODED_SLICE_OF_IDR_PICTURE = 5,
    SEI = 6,
    SPS = 7,
    PPS = 8,
    AUD = 9,
    EOSEQ = 10,
    EOSTREAM = 11,
    FILLER_DATA = 12,
    SPS_EXTENSION = 13,
    PREFIX = 14,
    SUBSET_SPS = 15,

    // 16..18 : reserved
    RS16 = 16,
    RS17 = 17,
    RS18 = 18,

    CODED_SLICE_OF_AUX_CODED_PICTURE = 19,
    CODED_SLICE_EXTENSION = 20,

    // 21..23 : reserved
    RS21 = 21,
    RS22 = 22,
    RS23 = 23,

    // 24..31 : unspecified
    UNSPEC24 = 24,
    UNSPEC25 = 25,
    UNSPEC26 = 26,
    UNSPEC27 = 27,
    UNSPEC28 = 28,
    UNSPEC29 = 29,
    UNSPEC30 = 30,
    UNSPEC31 = 31,
};

char *NalUnitTypeToString(uint8_t nal_unit_type);


typedef struct {
    const uint8_t *data; // pointer into original buffer (after start code)
    size_t        size; // byte count before emualtion prevention removal
    uint8_t       type; // nal_unit_type (low 5 bits of first byte)
    uint8_t       ref_idc; // nal unit nal_ref_icd (bits 5-6 of first byte)
} NalUnit;


#endif //TOY_H264_COMMON_H