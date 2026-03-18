//
// Created by gmathix on 3/18/26.
//


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/annexb.h"
#include "../src/util/bitreader.h"
#include "../src/common.h"
#include "../src/ps.h"

int main(void) {
    char *path = "../videos/output-nvenc.h264";
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
    int num_pps = 0;
    int num_sps = 0;


    for (int i = 0; i < num_nals; i++) {
        NalUnit nal = nals[i];
        br = make_br(nal.data, nal.size);
        if (nal.type == NAL_SPS) {
            printf("Detected SPS Nal unit : \n   ");
            int ret = decode_sps(&br, &ps);
            printf("\n");

            num_sps++;
        }
        if (nal.type == NAL_PPS) {
            printf("Detected PPS Nal unit : \n   ");
            int ret = decode_pps(&br, &ps);
            printf("\n");

            num_pps++;
        }
    }

    printf("\n[INFO] scanned total of %d nal units, found %d SPS units, %d PPS units\n", num_nals, num_sps, num_pps);

    free(data);
    free(nals);

    return 0;
}
