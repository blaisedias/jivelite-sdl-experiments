#define _XOPEN_SOURCE 600
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>


int64_t get_micro_seconds() {
    uint64_t millis;
    struct timespec  ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    millis = (ts.tv_sec*1000000);
    millis += (ts.tv_nsec/1000);
    return millis;
}

int64_t get_milli_seconds() {
    return get_micro_seconds()/1000;
}

void sleep_milli_seconds(int64_t millis) {
    if (millis < 0) {
        return;
    }
    struct timespec ts = {.tv_sec =0, .tv_nsec = 1000000*millis};
    nanosleep(&ts, NULL);
}

void sleep_micro_seconds(int64_t micros) {
    if (micros < 0) {
        return;
    }
    struct timespec ts = {.tv_sec =0, .tv_nsec = 1000*micros};
    nanosleep(&ts, NULL);
}
