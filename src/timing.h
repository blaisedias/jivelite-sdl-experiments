#ifndef __jl_timing_h_
#define __jl_timing_h_
#include <stdint.h>

uint64_t get_micro_seconds();
uint64_t get_milli_seconds();
void sleep_milli_seconds(unsigned millis);
#endif // __jl_timing_h_
