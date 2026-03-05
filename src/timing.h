#ifndef __jl_timing_h_
#define __jl_timing_h_
#include <stdint.h>

int64_t get_micro_seconds();
int64_t get_milli_seconds();
void sleep_milli_seconds(int64_t millis);
void sleep_micro_seconds(int64_t micros);
#endif // __jl_timing_h_
