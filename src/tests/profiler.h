//
// Created by gmathix on 4/9/26.
//

#ifndef TOY_H264_PROFILER_H
#define TOY_H264_PROFILER_H


#include <stddef.h>
#include <time.h>


typedef struct Profiler {
    struct CodecContext *p_ctx;


    struct timespec frame_start, frame_end, mb_start, mb_end;

    size_t total_frames;
    size_t total_mbs;

    long long frames_seconds;
    long long frames_microseconds;
    long long mbs_seconds;
    long long mbs_microseconds;

} Profiler ;

void profiler_init(Profiler *prf);
void profiler_start_frame(Profiler *prf);
void profiler_end_frame(Profiler *prf);
void profiler_start_mb(Profiler *prf);
void profiler_end_mb(Profiler *prf);

long long get_total_frame_microseconds(Profiler *prf);
long long get_total_mb_microseconds(Profiler *prf);



#endif //TOY_H264_PROFILER_H