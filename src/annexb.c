//
// Created by gmathix on 3/17/26.
//

#include "annexb.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>


/* count start codes to find number of nal units */
int count_nals(const uint8_t *buf, size_t size) {
    int count = 0;

    for (size_t i = 0; i + 3 < size; i++) {
        // 00 00 01
        if (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 1) {
            count++;
        }

        // 00 00 00 01
        else if (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 0 && buf[i+3] == 1){
            count++;
            i += 2;
        }
    }

    return count;
}


/* split the raw stream into NAL units with emulation prevention bytes removal */
void fill_nal_units(const uint8_t *buf, size_t size, NalUnit *p_nalUnits, int maxNals) {
    size_t i = 0;
    int nal_count = 0;


    while (i + 3 < size) {

        /* find start code */
        if ((buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 1) ||
            (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 0 && buf[i+3] == 1)) {

            size_t start;

            if (buf[i+2] == 1) { // 00 00 01
                start = i+3;
                i += 3;
            } else { // 00 00 00 01
                start = i+4;
                i += 4;
            }

            /* find next start code and calculate current NAL size */
            size_t next = i;
            while (next < size &&
                    !((buf[next] == 0 && buf[next+1] == 0 && buf[next+2] == 1) ||
                      (buf[next] == 0 && buf[next+1] == 0 && buf[next+2] == 0 && buf[next+3] == 1)))
            {
                next++;
            }


            if (nal_count < maxNals) {
                uint8_t header = buf[start];


                /* new NAL unit */
                NalUnit unit = {NULL, 0, 0, 0};

                size_t rbsp_size = 0; // size after emulation prevention removal
                unit.data = nal_to_rbsp(
                    buf + start + 1, // remove header byte from payload
                    next - start - 1,
                    &rbsp_size
                    );


                unit.size = rbsp_size;
                unit.ref_idc = (header >> 5) & 0x03;
                unit.type = header & 0x1F;

                p_nalUnits[nal_count] = unit;
            }

            nal_count++;
            i = next;
        }
        else {
            i++;
        }
    }
}

NalUnit *next_nal_unit(BitReader *br) {
    NalUnit *nal = calloc(1, sizeof(NalUnit));

    while (bitreader_peek_bits(br, 24) != 1 &&
           bitreader_peek_bits(br, 32) != 1) {
        bitreader_skip_bits(br, 8);
        if (bitreader_bits_remaining(br) < 8) {
            return NULL;
        }
    }

    if (bitreader_peek_bits(br, 24) == 1) {
        bitreader_skip_bits(br, 24);
    } else {
        bitreader_skip_bits(br, 32);
    }

    size_t size = 0;
    while (br->byte_pos < br->size-1 && bitreader_peek_bits(br,24) != 1 && bitreader_peek_bits(br, 32) != 1) {
        size++;
        bitreader_skip_bits(br, 8);
    }

    bitreader_rewind(br, size * 8);

    uint8_t header = bitreader_peek_bits(br, 8);
    size_t rbsp_size = 0;
    nal->data = nal_to_rbsp(
        br->data + br->byte_pos + 1, // remove header byte from payload
        size, &rbsp_size);

    nal->size = rbsp_size;
    nal->ref_idc = (header >> 5) & 0x03;
    nal->type = header & 0x1F;

    bitreader_skip_bits(br, size * 8);


    return nal;
}

uint8_t *nal_to_rbsp(const uint8_t *buf, size_t size, size_t *rbsp_size) {
    uint8_t *rbsp = malloc(size); // worst case : same size
    size_t j = 0;

    // uint8_t *src = buf;
    // uint8_t *end = buf+size;
    // uint8_t *dst = rbsp;
    //
    // while (src < end) {
    //     uint8_t *ptr = memchr(src, 0x03, end - src);
    //     if (ptr == NULL) {
    //         /* no more 0x03, just copy the rest */
    //         memcpy(dst, src, end - src);
    //         dst += end - src;
    //         break;
    //     }
    //
    //     if (ptr >= buf+2 && *(ptr-1) == 0x00 && *(ptr-2) == 0x00) {
    //         // emulation prevention byte found, remove include everything up to (not including) the 0x03
    //         memcpy(dst, src, ptr - src);
    //         dst += ptr - src;
    //         src = ptr + 1; // skip the 0x03
    //     } else {
    //         memcpy(dst, src, ptr - src + 1);
    //         dst += ptr - src + 1;
    //         src = ptr + 1;
    //     }
    // }
    // *rbsp_size = dst - rbsp;

    for (size_t i = 0; i < size; i++) {
        if (i+2 < size && buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x03) { // skip emulation prevention byte
            rbsp[j++] = 0x00;
            rbsp[j++] = 0x00;
            i += 2;
            continue;
        }

        rbsp[j] = buf[i];
        j++;
    }
    *rbsp_size = j;



    return rbsp;
}