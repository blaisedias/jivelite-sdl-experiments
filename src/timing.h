#ifndef __jl_timing_h_
#define __jl_timing_h_
#include <stdint.h>

uint64_t get_micro_seconds();
uint64_t get_milli_seconds();
void sleep_milli_seconds(uint64_t millis);
void sleep_micro_seconds(uint64_t micros);
void sleep_micro_seconds_delta(uint64_t micros1, uint64_t micros2);
#endif // __jl_timing_h_
