//
// Created by gmathix on 4/9/26.
//

#include "profiler.h"



void profiler_init(Profiler *prf) {
    prf->total_frames = 0;
    prf->frames_seconds = 0;
    prf->mbs_microseconds = 0;
    prf->total_mbs = 0;
    prf->frames_microseconds = 0;
    prf->mbs_seconds = 0;
}


void profiler_start_frame(Profiler *prf) {
    clock_gettime(CLOCK_MONOTONIC, &prf->frame_start);
}


void profiler_end_frame(Profiler *prf) {
    clock_gettime(CLOCK_MONOTONIC, &prf->frame_end);

    prf->frames_seconds += prf->frame_end.tv_sec - prf->frame_start.tv_sec;
    prf->frames_microseconds += (prf->frame_end.tv_nsec - prf->frame_start.tv_nsec) / 1000;

    prf->total_frames++;
}


void profiler_start_mb(Profiler *prf) {
    clock_gettime(CLOCK_MONOTONIC, &prf->mb_start);
}


void profiler_end_mb(Profiler *prf) {
    clock_gettime(CLOCK_MONOTONIC, &prf->mb_end);

    prf->mbs_seconds += prf->mb_end.tv_sec - prf->mb_start.tv_sec;
    prf->mbs_microseconds += (prf->mb_end.tv_nsec - prf->mb_start.tv_nsec) / 1000;

    prf->total_mbs++;
}

long long get_total_frame_microseconds(Profiler *prf) {
    return prf->frames_seconds * 1000000 + prf->frames_microseconds;
}


long long get_total_mb_microseconds(Profiler *prf) {
    return prf->mbs_seconds * 1000000 + prf->mbs_microseconds;
}
