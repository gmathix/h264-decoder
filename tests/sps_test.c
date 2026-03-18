//
// Created by gmathix on 3/18/26.
//


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/annexb.h"
#include "../src/bitreader.h"
#include "../src/common.h"
#include "../src/ps.h"

int main(void) {
    char *path = "../videos/output.h264";
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("file not found : %s", path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    uint8_t *data = malloc(size);

    fread(data, 1, size, f);
    fclose(f);


    BitReader br = make_br(data, size);


    /* count and store nals */
    int num_nals = count_nals(data, size);
    NalUnit *nals = malloc(num_nals * sizeof(NalUnit));
    fill_nal_units(data, size, nals, num_nals);

    ParamSets ps;

    for (int i = 0; i < num_nals; i++) {
        NalUnit nal = nals[i];
        if (nal.type == NAL_SPS) {
            BitReader nbr = make_br(nal.data, nal.size);

            int ret = decode_sps(&nbr, &ps);
        }
    }


    free(data);
    free(nals);

    return 0;
}
