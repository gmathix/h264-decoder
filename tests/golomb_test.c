//
// Created by gmathix on 3/18/26.
//

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "../src/bitreader.h"
#include "../src/annexb.h"
#include "../src/expgolomb.h"


static BitReader make_br(const uint8_t *data, int size) {
    BitReader br;
    bitreader_init(&br, data, size);
    return br;
}


/* ========================= */
/*        read_ue tests      */
/* ========================= */
void test_read_ue() {
    // Bit patterns:
    // 1        -> 0
    // 010      -> 1
    // 011      -> 2
    // 00100    -> 3
    // 00101    -> 4

    uint8_t data[] = {
        0b10100110, // 1 | 010 | 011 | 0...
        0b01000010,  // continuation
        0b10000000
    };

    BitReader br = make_br(data, sizeof(data));

    assert(read_ue(&br) == 0);
    assert(read_ue(&br) == 1);
    assert(read_ue(&br) == 2);
    assert(read_ue(&br) == 3);
    assert(read_ue(&br) == 4);

    printf("read_ue OK\n");
}

/* ========================= */
/*        read_se tests      */
/* ========================= */
void test_read_se() {
    // ue values: 0,1,2,3,4,5
    // se mapping:
    // 0 -> 0
    // 1 -> 1
    // 2 -> -1
    // 3 -> 2
    // 4 -> -2
    // 5 -> 3

    uint8_t data[] = {
        0b10100110, // same sequence as before
        0b01000010,
        0b10011000
    };

    BitReader br = make_br(data, sizeof(data));

    assert(read_se(&br) == 0);
    assert(read_se(&br) == 1);
    assert(read_se(&br) == -1);
    assert(read_se(&br) == 2);
    assert(read_se(&br) == -2);
    assert(read_se(&br) == 3);

    printf("read_se OK\n");
}

/* ========================= */
/*        read_te tests      */
/* ========================= */
void test_read_te() {
    // Case max == 1 → single bit inversion
    {
        uint8_t data[] = {0b10000000}; // first bit = 1
        BitReader br = make_br(data, sizeof(data));

        // read_u = 1 → returns !1 = 0
        assert(read_te(&br, 1) == 0);
    }

    {
        uint8_t data[] = {0b00000000}; // first bit = 0
        BitReader br = make_br(data, sizeof(data));

        // read_u = 0 → returns !0 = 1
        assert(read_te(&br, 1) == 1);
    }

    // Case max > 1 → behaves like ue
    {
        uint8_t data[] = {0b10100000}; // ue = 0
        BitReader br = make_br(data, sizeof(data));

        assert(read_te(&br, 5) == 0);
    }

    printf("read_te OK\n");
}

/* ========================= */
/*   map_coded_block_pattern */
/* ========================= */

void test_cbp_mapping() {
    // Table A (chroma 1 or 2), intra
    assert(map_coded_block_pattern(0, 1, 1) == 47);
    assert(map_coded_block_pattern(47, 1, 1) == 41);

    // Table A, inter
    assert(map_coded_block_pattern(0, 1, 0) == 0);
    assert(map_coded_block_pattern(47, 1, 0) == 39);

    // Table B (chroma 0 or 3), intra
    assert(map_coded_block_pattern(0, 0, 1) == 15);
    assert(map_coded_block_pattern(15, 0, 1) == 9);

    // Table B, inter
    assert(map_coded_block_pattern(0, 0, 0) == 0);
    assert(map_coded_block_pattern(15, 0, 0) == 9);

    // Invalid codeNum
    assert(map_coded_block_pattern(48, 1, 1) == (uint32_t)-1);
    assert(map_coded_block_pattern(16, 0, 1) == (uint32_t)-1);

    printf("CBP mapping OK\n");
}


/* ========================= */
/*            main           */
/* ========================= */

int main() {
    test_read_ue();
    test_read_se();
    test_read_te();
    test_cbp_mapping();

    printf("All tests passed.\n");
    return 0;
}