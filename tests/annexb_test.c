#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "../src/util/bitreader.h"
#include "../src/annexb.h"

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
    if (!data) {
        perror("malloc");
        fclose(f);
        return 1;
    }

    fread(data, 1, size, f);
    fclose(f);



    BitReader br = {NULL, 0, 0, 0};
    bitreader_init(&br, data, size);


    int raw_emulation = 0;
    int zc = 0;
    for (size_t i = 0; i < size; i++) {
        if (zc == 2 && data[i] == 0x03) {
            raw_emulation++;
            //printf("raw emulation byte at byte %lu, line 0x%x0 (shape %s)\n", i,  i / 16,
               // i % 2 == 0 ? "0000 03" : "00 0003");
            zc = 0;
            continue;
        }
        if (data[i] == 0x00) zc++;
        else zc = 0;
    }
    printf("emulation bytes in raw stream : %d\n", raw_emulation);


    int num_nals = count_nals(data, size);
    printf("%d total nal units\n", num_nals);

    NalUnit *nal_units = malloc(num_nals * sizeof(NalUnit));
    if (!nal_units) {
        perror("malloc");
        return 1;
    }

    fill_nal_units(data, size, nal_units, num_nals);


    uint32_t line = 0;
    size_t bytes =0;

    int nb_emulation = 0;


    for (int i = 0; i < num_nals; i++) {
        int j = 0;
        while (j < nal_units[i].size) {
            if (nal_units[i].data[j] == 0x00 && nal_units[i].data[j+1] == 0x00 && nal_units[i].data[j+2] == 0x03) {
                nb_emulation++;
                j += 3;
            } else {
                j++;
            }
            bytes++;
        }
    }
    printf("found %d emulation prevention bytes in RBSP\n", nb_emulation);



    free(nal_units);
    free(data);


    return 0;
}