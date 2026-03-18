#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "src/bitreader.h"
#include "src/annexb.h"

int main(void) {
    char *path = "videos/output.h264";
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



    BitReader br = {
        NULL, 0, 0, 0
    };

    bitreader_init(&br, data, size);


    int num_nals = count_nals(data, size);

    printf("%d total nal units\n", num_nals);


    NalUnit *nal_units = malloc(num_nals * sizeof(NalUnit));
    if (!nal_units) {
        perror("malloc");
        return 1;
    }


    fill_nal_units(data, size, nal_units, num_nals);

    int nb_emulation = 0;
    int zero_count = 0;

    for (int i = 0; i < num_nals; i++) {
        for (int j = 0; j < nal_units[i].size; j++) {
            if (zero_count == 2 && nal_units[i].data[j] == 0x03) {
                zero_count = 0;
                nb_emulation++;
                continue;
            }

            if (nal_units[i].data[j] == 0x00) {
                zero_count++;
            } else {
                zero_count = 0;
            }
        }
    }
    printf("found %d emulation prevention bytes\n", nb_emulation);




    free(nal_units);
    free(data);


    return 0;
}