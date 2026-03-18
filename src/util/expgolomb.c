//
// Created by gmathix on 3/18/26.
//

#include "../expgolomb.h"



static const uint8_t cbp_tableA_intra[48] = {
    47, 31, 15,  0, 23, 27, 29, 30,
     7, 11, 13, 14, 39, 43, 45, 46,
    16,  3,  5, 10, 12, 19, 21, 26,
    28, 35, 37, 42, 44,  1,  2,  4,
     8, 17, 18, 20, 24,  6,  9, 22,
    25, 32, 33, 34, 36, 40, 38, 41
};

static const uint8_t cbp_tableA_inter[48] = {
    0, 16,  1,  2,  4,  8, 32,  3,
    5, 10, 12, 15, 47,  7, 11, 13,
   14,  6,  9, 31, 35, 37, 42, 44,
   33, 34, 36, 40, 38, 41, 43, 45,
   46, 17, 18, 20, 24, 19, 21, 26,
   28, 23, 27, 29, 30, 22, 25, 39
};

static const uint8_t cbp_tableB_intra[16] = {
    15,  0,  7, 11, 13, 14,  3,  5,
    10, 12,  1,  2,  4,  8,  6,  9
};

static const uint8_t cbp_tableB_inter[16] = {
    0,  1,  2,  4,  8,  3,  5, 10,
   12, 15,  7, 11, 13, 14,  6,  9
};


uint32_t read_u(BitReader *br, int n) {
    return bitreader_read_bits(br, n);
}

/* parsing process described in §9.1 */
uint32_t read_ue(BitReader *br) {
    int leadingZeroBits = -1;
    for (uint32_t b = 0; !b; leadingZeroBits++)
        b = bitreader_read_bits(br, 1);

    uint32_t codeNum = (1<<leadingZeroBits) - 1 + bitreader_read_bits(br, leadingZeroBits);

    return codeNum;
}

int32_t read_se(BitReader *br) {
    int32_t ue = (int32_t) read_ue(br);

    int sign = (ue%2 == 0) ? -1 : 1;
    return sign * (ue+1) / 2;
}

uint32_t read_te(BitReader *br, int max) {
    if (max == 1) return !read_u(br, 1);
    return read_ue(br);
}

uint32_t map_coded_block_pattern(uint32_t codeNum, int chromaArrayType, int isIntra) {
    if (chromaArrayType == 1 || chromaArrayType == 2) {
        if (codeNum > 47) return -1;
        return isIntra
            ? cbp_tableA_intra[codeNum]
            : cbp_tableA_inter[codeNum];
    }
    if (chromaArrayType == 0 || chromaArrayType == 3) {
        if (codeNum > 15) return -1;
        return isIntra
            ? cbp_tableB_intra[codeNum]
            : cbp_tableB_inter[codeNum];
    }
    return -1;
}