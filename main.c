//
// Created by gmathix on 3/18/26.
//


#include <stdio.h>
#include <stdlib.h>

#include "src/decoder.h"


int main(void) {
    int res = (int)(size_t)12 - (int)(size_t)13;
    printf("%lu\n", res);

    const char *path = "../videos/output.h264";
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("file not found : %s\n", path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);


    uint8_t *data = malloc(size);
    fread(data, 1, size, file);
    fclose(file);


    CodecContext *context = decoder_init(data, size);


    decoder_run(context);


    return 0;
}
