/* x86-64 Linux test program for the CPU translator. It prints what it is,
 * runs the shared workload and reports time and peak memory, as seen from
 * inside the translated process. */
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>

#include "probe_workload.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Prints a line such as "VmHWM: 1234 kB" from /proc/self/status. */
static void print_status_line(const char* key) {
    FILE* file = fopen("/proc/self/status", "r");
    if (file == NULL) return;
    char line[256];
    size_t key_length = strlen(key);
    while (fgets(line, sizeof line, file) != NULL) {
        if (strncmp(line, key, key_length) == 0) {
            printf("%s", line);
            break;
        }
    }
    fclose(file);
}

int main(void) {
#if defined(__x86_64__)
    printf("probe: compiled for x86_64\n");
#else
    printf("probe: compiled for another architecture\n");
#endif
    struct utsname name;
    if (uname(&name) == 0) printf("probe: running on %s\n", name.machine);

    double start = now_ms();
    unsigned long long checksum = (unsigned long long)probe_workload();
    double elapsed = now_ms() - start;

    printf("probe: checksum %llu\n", checksum);
    printf("probe: workload %.0f ms\n", elapsed);
    print_status_line("VmHWM:");
    print_status_line("VmRSS:");
    printf("probe: ok\n");
    return 0;
}
