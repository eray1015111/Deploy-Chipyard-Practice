#include <stdint.h>
#include <stdio.h>

#define LFSR_POLY 0x80200003U

static inline unsigned long read_mcycle(void)
{
    unsigned long value;
    asm volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

static uint32_t lfsr_step(uint32_t state)
{
    return (state & 1U) ? ((state >> 1) ^ LFSR_POLY) : (state >> 1);
}

uint32_t lfsr_sw(uint32_t seed, uint32_t steps)
{
    uint32_t lfsr = seed;

    for (uint32_t i = 0; i < steps; i++) {
        lfsr = lfsr_step(lfsr);
    }

    return lfsr;
}

int main(void)
{
    const uint32_t seed = 0x12345678U;
    const uint32_t steps = 10000U;

    printf("Pure C LFSR baseline\n");
    printf("Seed: 0x%08X, steps: %u\n", seed, steps);

    printf("[Golden Model] First 100 one-step states:\n");
    uint32_t current = seed;
    for (int i = 1; i <= 100; i++) {
        current = lfsr_sw(current, 1);
        printf("%08X ", current);
        if ((i % 10) == 0) {
            printf("\n");
        }
    }

    unsigned long start = read_mcycle();
    uint32_t result = lfsr_sw(seed, steps);
    unsigned long cycles = read_mcycle() - start;

    printf("\n[SW] lfsr_sw(seed=0x%08X, steps=%u) = 0x%08X\n",
           seed, steps, result);
    printf("[SW] cycles=%lu\n", cycles);
    return 0;
}
