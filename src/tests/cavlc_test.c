//
// Created by gmathix on 4/1/26.
//

#include "../vlc.h"
#include "../cavlc.h"


#include <assert.h>
#include <stdlib.h>

#include "time.h"

#define MAX_COEFF_TOKEN_BITS    16
#define MAX_TOTAL_ZEROS_BITS     9



const uint16_t test_coeff_token_lengths_02[62] ={
    /* 0 <= nC < 2 */
    1,
    6,  2,             8,  6,  3,         9,  8,  7,  5,    10,  9,  8,  6,
   11, 10,  9,  7,    13, 11, 10,  8,    13, 13, 11,  9,    13, 13, 13, 10,
   14, 14, 13, 11,    14, 14, 14, 13,    15, 15, 14, 14,    15, 15, 15, 14,
   16, 15, 15, 15,    16, 16, 16, 15,    16, 16, 16, 16,    16, 16, 16, 16
};
const int16_t test_coeff_token_bits_02[62]= {
    /* 0 <= nC < 2 */
    1,
    5,  1,             7,  4,  1,         7,  6,  5,  3,     7,  6,  5,  3,
    7,  6,  5,  4,    15,  6,  5,  4,    11, 14,  5,  4,     8, 10, 13,  4,
   15, 14,  9,  4,    11, 10, 13, 12,    15, 14,  9, 12,    11, 10, 13,  8,
   15,  1,  9, 12,    11, 14, 13,  8,     7, 10,  9, 12,     4,  6,  5,  8,
};




int main(void) {

    VLCTable vlc = make_vlc(30, 16, coeff_token_cr422_bits, coeff_token_cr422_lengths);

    build_vlc(&vlc);

    printf("lookup has %d entries (%dkb)\n", vlc.lookup_size, (vlc.lookup_size*sizeof(uint16_t)) / 1024);



    free(vlc.lookup_length);
    free(vlc.lookup_symbol);



    return 0;
}