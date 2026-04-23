//
// Created by gmathix on 3/18/26.
//


#include <stdio.h>
#include <stdlib.h>

#include "src/decoder.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int main(int argc, char *argv[]) {

    if (argc == 1) {
        printf("please specify an input file.\n"
               "Usage : intra_decode <input_file> [output_file]");
        return 1;
    }


    const char *in_path  = argv[1];
    const char *out_path = argc > 1 ? argv[2] : "output.yuv";


    FILE *test = fopen(in_path, "rb");
    if (!test) {
        printf("file not found : %s\n", in_path);
        return 1;
    }
    fclose(test);

    int fd = open(in_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;

    uint8_t *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    madvise(data, size, MADV_SEQUENTIAL);


    CodecContext *context = decoder_init(data, size, out_path);


    decoder_run(context);


    long long total_frames_us = get_total_frame_microseconds(context->prf);
    long long total_mb_us = get_total_mb_microseconds(context->prf);

    size_t total_frames = context->prf->total_frames;
    size_t total_mbs = context->prf->total_mbs;

    printf("avg : %.3fms per frame (%.3ffps) ; %.3fus per macroblock\n",
        (double)total_frames_us / (double)total_frames / 1000,
        (double)((double)total_frames / ((double)total_frames_us/1000000)),
        (double)total_mb_us / (double)total_mbs
    );


    decoder_free(context);


    return 0;
}
