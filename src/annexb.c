//
// Created by smegmuss on 3/17/26.
//

#include "annexb.h"



int count_nals(const uint8_t *buf, size_t size) {
    int count = 0;

    for (size_t i = 0; i + 3 < size; i++) {
        if (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 0 && buf[i+3] == 1){
            count++;
            i += 2;
        }
        if ((buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 1)) {
            count++;
        }

    }

    return count;
}

void fill_nal_units(const uint8_t *buf, size_t size, NalUnit *nal_units, int max_nals) {
    size_t i = 0;
    int nal_count = 0;

    while (i + 3 < size) {
        /* find start code */
        if (buf[i] == 0 && buf[i+1] == 0 &&
           (buf[i+2] == 1 || (buf[i+2] == 0 && buf[i+3] == 1))) {

            size_t start;

            if (buf[i+2] == 1) { // 00 00 01
                start = i+3;
                i += 3;
            } else { // 00 00 00 01
                start = i+4;
                i += 4;
            }

            /* find next start code */
            size_t next = i;
            while (next + 3 < size &&
                    !(buf[next] == 0 && buf[next+1] == 0 &&
                      (buf[next+2] == 1 || (buf[next+2] == 0 && buf[next+3] == 1))))
            {
                next++;
            }

            if (nal_count < max_nals) {
                uint8_t header = buf[start];

                NalUnit unit = {NULL, 0, 0, 0};

                size_t rbsp_size = 0;
                unit.data = nal_to_rbsp(
                    buf + start + 1, // nal unit payload without header
                    next - start - 1,
                    &rbsp_size
                    );

                unit.size = rbsp_size;
                unit.ref_idc = (header >> 5) & 0x03;
                unit.type = header & 0x1F;

                nal_units[nal_count] = unit;
            }

            nal_count++;
            i = next;
           }
        else {
            i++;
        }
    }
}

uint8_t *nal_to_rbsp(const uint8_t *buf, size_t size, size_t *rbsp_size) {
    uint8_t *rbsp = malloc(size); // worst case : same size
    size_t j = 0;

    int zero_count = 0;
    for (size_t i = 0; i < size; i++) {
        if (zero_count == 2 && buf[i] == 0x03) { // skip emulation prevention byte
            zero_count = 0;
            continue;
        }

        rbsp[j++] = buf[i];

        if (buf[i] == 0x00) {
            zero_count++;
        } else {
            zero_count = 0;
        }
    }

    *rbsp_size = j;

    return rbsp;
}