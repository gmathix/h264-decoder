//
// Created by gmathix on 8/16/26.
//


#include "../global.h"
#include "../util/formulas.h"


#define pixel uint8_t
#define BIT_DEPTH 8
#define FFABS(a)   (a >= 0 ? (a) : (-(a)))
#define av_clip(a, min, max)   (a < min ? (min) : (a > max ? (max) : (a)))
#define av_clip_pixel(a)   av_clip(a, 0, 255)


const int8_t tc0_table[3][52] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,
       1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,
       3,  4,  4,  4,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    },

    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
       1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  4,
       4,  5,  5,  6,  7,  8,  8, 10, 11, 12, 13, 15, 17,
    },

    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,
       1,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  6,
       6,  7,  8,  9, 10, 11, 13, 14, 16, 18, 20, 23, 25,
    },
};


static void h264_loop_filter_luma(uint8_t *p_pix, ptrdiff_t xstride, ptrdiff_t ystride, int inner_iters, int alpha, int beta, int8_t *tc0)
{
    pixel *pix = (pixel*)p_pix;
    int i, d;
    xstride >>= sizeof(pixel)-1;
    ystride >>= sizeof(pixel)-1;
    alpha <<= BIT_DEPTH - 8;
    beta  <<= BIT_DEPTH - 8;
    for( i = 0; i < 4; i++ ) {
        const int tc_orig = tc0[i] * (1 << (BIT_DEPTH - 8));
        if( tc_orig < 0 ) {
            pix += inner_iters*ystride;
            continue;
        }
        for( d = 0; d < inner_iters; d++ ) {
            const int p0 = pix[-1*xstride];
            const int p1 = pix[-2*xstride];
            const int p2 = pix[-3*xstride];
            const int q0 = pix[0];
            const int q1 = pix[1*xstride];
            const int q2 = pix[2*xstride];

            if( FFABS( p0 - q0 ) < alpha &&
                FFABS( p1 - p0 ) < beta &&
                FFABS( q1 - q0 ) < beta ) {

                int tc = tc_orig;
                int i_delta;

                if( FFABS( p2 - p0 ) < beta ) {
                    if(tc_orig)
                        pix[-2*xstride] = p1 + av_clip( (( p2 + ( ( p0 + q0 + 1 ) >> 1 ) ) >> 1) - p1, -tc_orig, tc_orig );
                    tc++;
                }
                if( FFABS( q2 - q0 ) < beta ) {
                    if(tc_orig)
                        pix[   xstride] = q1 + av_clip( (( q2 + ( ( p0 + q0 + 1 ) >> 1 ) ) >> 1) - q1, -tc_orig, tc_orig );
                    tc++;
                }

                i_delta = av_clip( (((q0 - p0 ) * 4) + (p1 - q1) + 4) >> 3, -tc, tc );
                pix[-xstride] = av_clip_pixel( p0 + i_delta );    /* p0' */
                pix[0]        = av_clip_pixel( q0 - i_delta );    /* q0' */
            }
            pix += ystride;
        }
    }
}



void filter_4p_hor_edge_low_bS_luma(int y, int x, const int filter_flags[4],
    uint8_t bS, uint8_t indexA, uint8_t beta, uint8_t samples[24][24]) {

    int treshold = tc0_table[bS-1][indexA];
    int aP, aQ, t, delta;

    int p0, p1, p2, q0, q1, q2;


    for (int i = 0; i < 4; i++) {
        if (filter_flags[i]) {
            p0 = samples[y-1][x];  p1 = samples[y-2][x];  p2 = samples[y-3][x];
            q0 = samples[y][x];    q1 = samples[y+1][x];  q2 = samples[y+2][x];

            aP = _abs(p2 - p0);
            aQ = _abs(q2 - q0);

            t = treshold + ((aP < beta) + (aQ < beta));


            delta = _clip3(-t, t,
                (((q0 - p0) * (1 << 2)) + (p1 - q1) + 4) >> 3);

            /*p1*/ samples[y-2][x] += (aP < beta) * _clip3(-treshold, treshold,
                  (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
            /*p0*/ samples[y-1][x] = _clip1y(p0 + delta, 8);

            /*q0*/ samples[y][x]   = _clip1y(q0 - delta, 8);
            /*q1*/ samples[y+1][x] += (aQ < beta) * _clip3(-treshold, treshold,
                  (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
        }

        x++;
    }
}


static void filter_row(uint8_t *dst, int y, int indexA[4], int alpha[4], int beta[4], int stride,
    const uint8_t luma_block[24][24], const int filter_flags[4][4], const uint8_t bS_list[4]) {

    for (int i = 0; i < 4; i++) {
        if (bS_list[i] > 0) {
            if (bS_list[i] < 4) {
                filter_4p_hor_edge_low_bS_luma(4+y, 4+i*4, filter_flags[i], bS_list[i], indexA[i], beta[i], luma_block);
            } else {
                printf("add the high bS function here peasant\n");
            }
        }
    }
    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 3; i++) {
            dst[i*stride]      = luma_block[y+4+i][k+4];   // q0, q1, q2
            dst[(-i-1)*stride] = luma_block[y+4-i-1][k+4]; // p0, p1, p2
        }
        dst++;
    }
}


static void dash_line(int n) { for (int i = 0; i < n; i++) putchar('-'); }
static void print_block(uint8_t *ptr, int width, int height) {
    for (int y = 0; y < height; y++) {
        if (y % 4 == 0) {
            for (int i = 0; i < 4; i++) {
                putchar('+');
                dash_line(17);
            }
            putchar('+');
            putchar('\n');
        }
        for (int x = 0; x < width; x++) {
            if (x % 4 == 0) { putchar('|'); putchar(' '); }
            printf("%3d ", ptr[x]);
        }
        putchar('|');
        putchar('\n');

        ptr += width;
    }

    for (int i = 0; i < 4; i++) {
        putchar('+');
        dash_line(17);
    }
    putchar('+');
    putchar('\n');
}


#define HEIGHT    20
#define WIDTH     16
#define NB_BLOCKS 20

int main(void) {

    // beautiful
    const uint8_t luma_block[24][24] = {
        {212, 212, 212, 212, 212, 212, 212, 212, 213, 212, 212, 213, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, },
        {212, 212, 212, 212, 212, 212, 212, 212, 213, 213, 212, 212, 211, 212, 212, 212, 212, 212, 212, 212, 212, 212, 211, 212, },
        {211, 211, 212, 213, 212, 211, 212, 212, 212, 213, 213, 208, 211, 212, 212, 212, 211, 213, 213, 212, 212, 212, 211, 212, },
        {211, 211, 212, 214, 213, 211, 211, 212, 213, 213, 213, 213, 210, 211, 212, 212, 211, 213, 211, 211, 212, 212, 209, 211, },
        {211, 211, 211, 213, 213, 211, 210, 212, 213, 213, 213, 216, 214, 209, 210, 212, 220, 211, 210, 216, 210, 212, 212, 211, },
        {210, 210, 211, 213, 212, 211, 208, 211, 213, 212, 215, 205, 212, 225, 221, 216, 230, 225, 228, 214, 203, 207, 211, 212, },
        {210, 210, 211, 213, 212, 210, 208, 211, 213, 212, 215, 174, 197, 228, 223, 204, 172, 200, 213, 197, 199, 210, 206, 210, },
        {210, 210, 211, 213, 212, 210, 208, 211, 211, 212, 215, 161, 108, 109, 116, 106,  86, 113, 127, 164, 214, 220, 201, 208, },
        {210, 210, 210, 212, 211, 210, 209, 210, 211, 211, 214, 175, 117, 103, 116, 119, 122, 118, 103, 162, 220, 214, 201, 210, },
        {210, 210, 210, 212, 211, 210, 209, 210, 211, 211, 211, 200, 196, 203, 213, 212, 212, 213, 189, 195, 215, 209, 207, 213, },
        {211, 211, 211, 210, 210, 210, 209, 210, 211, 210, 213, 219, 232, 237, 231, 231, 231, 231, 231, 208, 211, 212, 212, 214, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 210, 213, 216, 218, 218, 212, 212, 212, 212, 217, 208, 210, 212, 211, 214, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 210, 212, 212, 205, 205, 207, 207, 207, 207, 205, 207, 212, 211, 210, 211, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 210, 212, 212, 209, 208, 209, 210, 209, 208, 209, 210, 213, 212, 210, 211, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 210, 213, 212, 220, 225, 220, 217, 223, 225, 221, 216, 211, 212, 210, 211, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 210, 209, 214, 201, 180, 198, 212, 187, 178, 196, 216, 212, 210, 210, 212, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 209, 216, 195, 136, 138, 170, 157, 144, 163, 145, 200, 222, 212, 210, 212, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 208, 224, 177, 153, 212, 170, 124, 173, 194, 134, 192, 231, 208, 209, 214, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 210, 212, 196, 230, 224, 143, 160, 146, 117, 147, 217, 225, 209, 210, 213, },
        {211, 211, 211, 211, 210, 210, 210, 210, 210, 211, 203, 231, 248, 151, 160, 191, 132, 161, 153, 189, 220, 213, 208, 213, },
        {212, 212, 212, 212, 210, 210, 210, 210, 210, 211, 205, 228, 173, 145, 236, 161, 163, 255, 141, 137, 227, 210, 208, 214, },
        {212, 212, 212, 212, 210, 210, 210, 210, 211, 208, 227, 195, 106, 153, 230, 163, 157, 199, 152, 180, 224, 208, 209, 213, },
        {212, 212, 212, 212, 210, 210, 210, 210, 211, 208, 228, 198, 150, 158, 172, 207, 184, 140, 193, 237, 213, 212, 211, 212, },
        {212, 212, 212, 212, 210, 210, 210, 210, 210, 210, 214, 218, 220, 210, 198, 227, 223, 202, 221, 230, 213, 215, 210, 213, },
    };

    uint8_t *dest_undo264 = malloc(WIDTH * HEIGHT * sizeof(uint8_t));
    uint8_t *dest_ffmpeg  = malloc(WIDTH * HEIGHT * sizeof(uint8_t));

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < HEIGHT; x++) {
            dest_undo264[y*16 + x] = luma_block[y][x+4];
            dest_ffmpeg[y*16 + x]  = luma_block[y][x+4];
        }
    }


    uint8_t bS_list[4] = {1, 1, 2, 2};
    int filter_flags[4][4] = {
        {1, 1, 1, 1},
        {1, 1, 1, 0},
        {1, 0, 0, 1},
        {0, 0, 0, 1},
    };
    int indexA[4] = {28, 28, 28, 28};
    int alpha[4]  = {20, 20, 20, 20};
    int beta[4]   = { 7,  7,  7,  7};
    int8_t tc0[4];
    for (int i = 0; i < 4; i++)
        tc0[i] = tc0_table[bS_list[0]-1][indexA[0]];


    // undo264
    for (int i = 0; i < 4; i++) {
        filter_row(&dest_undo264[4*16], 0, indexA, alpha, beta, 16, luma_block, filter_flags, bS_list);
    }
    // ffmpeg
    h264_loop_filter_luma(&dest_ffmpeg[4*16], 16, 1, 4, alpha[0], beta[0], tc0);


    for (int blk = 0; blk < NB_BLOCKS; blk++) {
        int base = ((blk * 4) / WIDTH) * WIDTH*4  + (blk*4) % WIDTH;
        int yy = (blk * 4) / WIDTH  * 4;
        int xx = (blk * 4) % WIDTH;
        printf("checking block %d at x:%d y:%d : \n", blk, xx, yy);
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int pos = base + y*WIDTH + x;
                if (dest_undo264[pos] != dest_ffmpeg[pos]) {
                    printf("mismatch at blk:%d  y:%d x:%d", blk, y, x);
                    printf("\nundo264 : \n");
                    print_block(dest_undo264, 16, 20);
                    printf("\n\nffmpeg : \n");
                    print_block(dest_ffmpeg, 16, 20);

                    exit(1);
                }
            }
        }
    }



    printf("undo264 and ffmpeg match\n");

    printf("\nundo264 : \n");
    print_block(dest_undo264, 16, 20);
    printf("\n\nffmpeg : \n");
    print_block(dest_ffmpeg, 16, 20);



    free(dest_undo264);
    free(dest_ffmpeg);

    exit(0);
}