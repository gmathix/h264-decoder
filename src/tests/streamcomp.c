//
// Created by gmathix on 4/14/26.
//


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>




#define ONLY_LUMA   0
#define ONLY_CHROMA 0


const int WIDTH = 1920;
const int HEIGHT = 1080;

const int Y_SIZE = WIDTH * HEIGHT;
const int U_SIZE = WIDTH/2 * HEIGHT/2;
const int V_SIZE = WIDTH/2 * HEIGHT/2;

const int MB_WIDTH = (WIDTH + 15) / 16;
const int MB_HEIGHT = (HEIGHT + 15) / 16;

const uint8_t block_mapping_4x4[16] = {
    0,   1,  4,  5,
    2,   3,  6,  7,
    8,   9, 12, 13,
    10, 11, 14, 15,
};


void print_mb(uint8_t *frame, int blk_size, int mb_idx, int frame_stride)
{
    int mb_x = mb_idx % MB_WIDTH;
    int mb_y = mb_idx / MB_WIDTH;

    int base_x = mb_x * blk_size;
    int base_y = mb_y * blk_size;

    for (int y = 0; y < blk_size; y++) {
        int row = (base_y + y) * frame_stride + base_x;

        for (int x = 0; x < blk_size; x++) {
            printf("%4d %s",
                   frame[row + x],
                   x % 4 == 3 ? "  " : "");
        }

        printf("\n%s", y % 4 == 3 ? "\n" : "");
    }
}


int main(void) {
    char *ref_path = "../videos/debug/ref.yuv";
    char *test_path = "../videos/out/test.yuv";

    FILE *ref_file = fopen(ref_path, "rb");
    if (!ref_file) {
        printf("file not found : %s\n", ref_path);
        exit(1);
    }

    FILE *test_file = fopen(test_path, "rb");
    if (!test_file) {
        printf("file not found : %s\n", test_path);
        exit(1);
    }

    fseek(ref_file, 0, SEEK_END);
    size_t ref_size = ftell(ref_file);
    rewind(ref_file);

    fseek(test_file, 0, SEEK_END);
    size_t test_size = ftell(test_file);
    rewind(test_file);

    if (ref_size != test_size) {
        printf("warning : streams don't have the same size, checking until something goes wrong\n");
    }

    printf("mb_width:%d mb_height:%d\n", MB_WIDTH, MB_HEIGHT);


    uint8_t *ref_frame_Y = malloc(Y_SIZE);
    uint8_t *ref_frame_U = malloc(U_SIZE);
    uint8_t *ref_frame_V = malloc(V_SIZE);


    uint8_t *test_frame_Y = malloc(Y_SIZE);
    uint8_t *test_frame_U = malloc(U_SIZE);
    uint8_t *test_frame_V = malloc(V_SIZE);


    size_t cur_frame = 0;
    size_t pos = 0;
    while (pos < ref_size) {
        printf("frame %lu", cur_frame);

        if (fread(ref_frame_Y, 1, Y_SIZE, ref_file) != Y_SIZE) break;
        if (fread(ref_frame_U, 1, U_SIZE, ref_file) != U_SIZE) break;
        if (fread(ref_frame_V, 1, V_SIZE, ref_file) != V_SIZE) break;

        if (fread(test_frame_Y, 1, Y_SIZE, test_file) != Y_SIZE) break;
        if (fread(test_frame_U, 1, U_SIZE, test_file) != U_SIZE) break;
        if (fread(test_frame_V, 1, V_SIZE, test_file) != V_SIZE) break;



#if !ONLY_CHROMA
        for (int mb = 0; mb < MB_WIDTH * MB_HEIGHT; mb++) {

            int mb_x = mb % MB_WIDTH;
            int mb_y = mb / MB_WIDTH;

            int base_x = mb_x * 16;
            int base_y = mb_y * 16;

            int max_y = (base_y + 16 > HEIGHT) ? HEIGHT - base_y : 16;
            int max_x = (base_x + 16 > WIDTH)  ? WIDTH  - base_x : 16;

            for (int iy = 0; iy < max_y; iy++) {
                for (int ix = 0; ix < max_x; ix++) {

                    int x = base_x + ix;
                    int y = base_y + iy;

                    int idx = y * WIDTH + x;


                    if (ref_frame_Y[idx] != test_frame_Y[idx]) {

                        int blkIdx = (iy / 4) * 4 + (ix / 4);
                        int diff = test_frame_Y[idx] - ref_frame_Y[idx];

                        printf("    mb %d differs in Y plane : \n"
                               "           blkIdx %d\n"
                               "           rel blk: y=%d x=%d\n"
                               "           diff=%d\n",
                               mb,
                               blkIdx,
                               iy % 4,
                               ix % 4,
                               diff);


                        printf("\nref mb : \n");
                        print_mb(ref_frame_Y, 16, mb, WIDTH);
                        printf("\ntest mb : \n");
                        print_mb(test_frame_Y, 16, mb, WIDTH);

                        exit(1);
                    }
                }
            }
        }
#endif


#if !ONLY_LUMA
        const int C_WIDTH  = WIDTH  / 2;
        const int C_HEIGHT = HEIGHT / 2;

        for (int plane = 0; plane < 2; plane++) {
            uint8_t *ref_c  = plane == 0 ? ref_frame_U  : ref_frame_V;
            uint8_t *test_c = plane == 0 ? test_frame_U : test_frame_V;
            char *plane_name = plane == 0 ? "U" : "V";

            for (int mb = 0; mb < MB_WIDTH * MB_HEIGHT; mb++) {
                int mb_x = mb % MB_WIDTH;
                int mb_y = mb / MB_WIDTH;

                int base_x = mb_x * 8;
                int base_y = mb_y * 8;

                int max_y = (base_y + 8 > C_HEIGHT) ? C_HEIGHT - base_y : 8;
                int max_x = (base_x + 8 > C_WIDTH)  ? C_WIDTH  - base_x : 8;

                for (int iy = 0; iy < max_y; iy++) {
                    for (int ix = 0; ix < max_x; ix++) {
                        int x = base_x + ix;
                        int y = base_y + iy;
                        int idx = y * C_WIDTH + x;

                        if (ref_c[idx] != test_c[idx]) {
                            int blkIdx = (iy / 4) * 2 + (ix / 4);
                            int diff = test_c[idx] - ref_c[idx];

                            printf("    mb %d differs in %s plane : \n"
                                   "           chroma blkIdx %d\n"
                                   "           rel blk: y=%d x=%d\n"
                                   "           diff=%d\n",
                                   mb, plane_name,
                                   blkIdx,
                                   iy % 4,
                                   ix % 4,
                                   diff);

                            printf("\nref mb (%s) : \n", plane_name);
                            print_mb(ref_c, 8, mb, C_WIDTH);
                            printf("\ntest mb (%s) : \n", plane_name);
                            print_mb(test_c, 8, mb, C_WIDTH);

                            if (mb / MB_WIDTH >= 1) {
                                printf("\n ref neighbor A : \n");
                                print_mb(ref_c, 8, mb-1, C_WIDTH);
                                printf("\n test neighbor A : \n");
                                print_mb(test_c, 8, mb-1, C_WIDTH);
                            }
                            if ((mb+1) % MB_WIDTH != 0) {
                                printf("ref neighbor B : \n");
                                print_mb(ref_c, 8, mb-MB_WIDTH, C_WIDTH);
                                printf("test neighbor B : \n");
                                print_mb(ref_c, 8, mb-MB_WIDTH, C_WIDTH);
                            }

                            exit(1);
                        }
                    }
                }
            }
        }
#endif

        printf("    ok\n");

        cur_frame++;
        pos += Y_SIZE + U_SIZE + V_SIZE;
    }




    exit(0);
}