//
// Created by gmathix on 4/3/26.
//


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * Usage : ./gen_rgb_video <width> <height> <frames> <output.rgb>
 * then encode with :
 * ffmpeg -f rawvideo -pixel_format rgb24 -video_size WxH -framerate 25 -i out.rgb \
 *      -c:v libx264 -profile:v baseline -crf 0 -preset ultrafast \
 *      -x264-params "keyint=1:bframes=0:no-cabac=1" -g 1 -bf 0 -pix_fmt yuv420p -an out.h264
*/

typedef enum {
    GRAD_HORIZONTAL,
    GRAD_VERTICAL,
    GRAD_DIAGONAL,
    GRAD_RADIAL,
    GRAD_COUNT
} GradientType ;

static void write_frame(FILE *f, int w, int h, int frame, GradientType type) {
    u_char *row = malloc(w*3);

    float hue_shit = (float)frame / 30.0f; // full cycle every 30 frames

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float fx = (float)x / (w-1);
            float fy = (float)y / (h-1);
            float t;

            switch (type) {
                case GRAD_HORIZONTAL: t = fx; break;
                case GRAD_VERTICAL: t = fy; break;
                case GRAD_DIAGONAL: t = (fx + fy) * 0.5f; break;
                case GRAD_RADIAL: {
                    float dx = fx - 0.5f, dy = fy - 0.5f;
                    t = sqrtf(dx*dx + dy*dy) * 1.414f; // normalize to [0,1]
                    if (t > 1.0f) t = 1.0f;
                    break;
                }
                default : t = fx;
            }

            float h6 = fmodf((t + hue_shit) * 6.0f, 6.0f);
            int   hi = (int)h6;
            float f2 = h6 - hi;
            float v  = 1.0f;
            float p  = 0.0f;
            float q  = 1.0f - f2;
            float r, g, b;

            switch (hi % 6) {
                case 0: r=v; g=f2; b=p; break;
                case 1: r=q; g=v;  b=p; break;
                case 2: r=p; g=v;  b=f2; break;
                case 3: r=p; g=q;  b=v; break;
                case 4: r=f2; g=p; b=v; break;
                default: r=v; g=p;  b=q; break;
            }

            row[x*3+0] = (unsigned char)(r * 255.0f + 0.5f);
            row[x*3+1] = (unsigned char)(g * 255.0f + 0.5f);
            row[x*3+2] = (unsigned char)(b * 255.0f + 0.5f);
        }
        fwrite(row, 3, w, f);
    }
    free(row);
}



int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <width> <height> <frames> <output.rgb> [gradient: 0=horiz 1=vert 2=diag 3=radial]\n"
            "\nExample (single macroblock, all-intra):\n"
            "  %s 16 16 4 out.rgb 0\n"
            "  ffmpeg -f rawvideo -pixel_format rgb24 -video_size 16x16 -framerate 25 -i out.rgb \\\n"
            "         -c:v libx264 -profile:v baseline -crf 0 -preset ultrafast \\\n"
            "         -x264-params \"keyint=1:bframes=0:no-cabac=1\" -g 1 -bf 0 -pix_fmt yuv420p -vf scale=16:16 out.264\n\n"
            "  # For 4 macroblocks (2x2 grid, 32x32):\n"
            "  %s 32 32 4 out.rgb 2\n",
            argv[0], argv[0], argv[0]);
        return 1;
    }

    int w      = atoi(argv[1]);
    int h      = atoi(argv[2]);
    int frames = atoi(argv[3]);
    const char *outpath = argv[4];
    GradientType type = (argc >= 6) ? (GradientType)(atoi(argv[5]) % GRAD_COUNT) : GRAD_DIAGONAL;

    if (w <= 0 || h <= 0 || frames <= 0 || w % 16 != 0 || h % 16 != 0) {
        fprintf(stderr, "Width and height must be positive multiples of 16 (macroblock size).\n");
        return 1;
    }

    FILE *f = fopen(outpath, "wb");
    if (!f) { perror("fopen"); return 1; }

    printf("Writing %dx%d, %d frame(s), gradient type %d -> %s\n", w, h, frames, type, outpath);
    for (int i = 0; i < frames; i++)
        write_frame(f, w, h, i, type);

    fclose(f);
    printf("Done. Encode with:\n");
    printf("  ffmpeg -f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate 25 -i %s \\\n", w, h, outpath);
    printf("         -c:v libx264 -profile:v baseline -crf 0 -preset ultrafast \\\n");
    printf("         -x264-params \"keyint=1:bframes=0:no-cabac=1\" -g 1 -bf 0 -pix_fmt yuv420p -an out.h264\n");
    printf("  # Strip to raw Annex B NAL units if needed:\n");
    printf("  ffmpeg -i out.264 -c copy -bsf:v h264_mp4toannexb out.h264\n");
    return 0;
}