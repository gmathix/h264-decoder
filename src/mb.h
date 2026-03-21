//
// Created by gmathix on 3/21/26.
//

#ifndef TOY_H264_MB_H
#define TOY_H264_MB_H

#include <stdint.h>


typedef struct I_MbInfo {
    uint32_t type;
    uint8_t pred_mode;
    uint8_t cbp_chroma;
    uint8_t cbp_luma;
} I_MbInfo ;

extern const I_MbInfo i_mb_type_info[26];


typedef struct P_MbInfo {
    uint32_t type;
    uint8_t part_count;
} P_MbInfo ;

extern const P_MbInfo p_mb_type_info[5];


typedef struct B_MbInfo {
    uint32_t type;
    uint8_t part_count;
} B_MbInfo ;

extern const B_MbInfo b_mb_type_info[23];

#endif //TOY_H264_MB_H