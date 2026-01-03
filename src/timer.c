#define _XOPEN_SOURCE 600
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>


uint64_t getMicros() {
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

uint64_t getMillis() {
    return getMicros()/1000;
}

