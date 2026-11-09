/* Workload shared by the x86-64 guest probe and the native ARM64 build, so
 * the two timings can be compared. Plain C, no dependencies. */
#ifndef HYPOCKET_PROBE_WORKLOAD_H
#define HYPOCKET_PROBE_WORKLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runs a fixed CPU workload (integer, floating point and memory) and returns
 * a checksum. The same input must give the same checksum on every CPU. */
uint64_t probe_workload(void);

#ifdef __cplusplus
}
#endif

#endif
