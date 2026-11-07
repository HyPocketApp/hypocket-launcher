#include "probe_workload.h"

#include <stdlib.h>
#include <string.h>

#define SIEVE_LIMIT 4000000
#define FLOAT_STEPS 3000000
#define COPY_BYTES (8 * 1024 * 1024)
#define COPY_ROUNDS 16

/* Integer work: count primes with a sieve of Eratosthenes. */
static uint64_t sieve(void) {
    unsigned char* composite = calloc(SIEVE_LIMIT + 1, 1);
    if (composite == NULL) return 0;
    uint64_t count = 0;
    for (uint64_t i = 2; i <= SIEVE_LIMIT; i++) {
        if (composite[i]) continue;
        count++;
        for (uint64_t j = i * i; j <= SIEVE_LIMIT; j += i) composite[j] = 1;
    }
    free(composite);
    return count;
}

/* Floating point work: a simple, deterministic recurrence. */
static uint64_t floats(void) {
    double x = 0.5;
    double sum = 0.0;
    for (int i = 0; i < FLOAT_STEPS; i++) {
        x = 3.7 * x * (1.0 - x);
        sum += x * 0.001;
    }
    return (uint64_t)(sum * 1000.0);
}

/* Memory work: copy a buffer back and forth. */
static uint64_t copies(void) {
    unsigned char* a = malloc(COPY_BYTES);
    unsigned char* b = malloc(COPY_BYTES);
    if (a == NULL || b == NULL) {
        free(a);
        free(b);
        return 0;
    }
    for (int i = 0; i < COPY_BYTES; i++) a[i] = (unsigned char)(i * 31);
    for (int round = 0; round < COPY_ROUNDS; round++) {
        memcpy(b, a, COPY_BYTES);
        b[round] ^= 0x5a;
        memcpy(a, b, COPY_BYTES);
    }
    uint64_t sum = 0;
    for (int i = 0; i < COPY_BYTES; i += 4096) sum += a[i];
    free(a);
    free(b);
    return sum;
}

uint64_t probe_workload(void) {
    return sieve() * 1000003u + floats() * 31u + copies();
}
