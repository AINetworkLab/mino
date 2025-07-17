#ifndef TIMING_UTILS_H
#define TIMING_UTILS_H

#include <time.h>
#include <doca_log.h>  // ✅ DOCA 的日志头

#define TIMER_START(name) \
    struct timespec name##_start, name##_end; \
    clock_gettime(CLOCK_MONOTONIC, &name##_start)

#define TIMER_END(name, label) \
    clock_gettime(CLOCK_MONOTONIC, &name##_end); \
    do { \
        double elapsed = (name##_end.tv_sec - name##_start.tv_sec) * 1000.0 + \
                         (name##_end.tv_nsec - name##_start.tv_nsec) / 1e6; \
        DOCA_LOG_INFO("[%-25s] 用时: %.2f ms", label, elapsed); \
    } while (0)

#endif