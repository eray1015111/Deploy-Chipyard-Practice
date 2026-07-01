#include <stdint.h>
#include <stdio.h>

#include "mmio.h"

#define LFSR_STATUS 0x6000U
#define LFSR_SEED   0x6004U
#define LFSR_STEPS  0x6008U
#define LFSR_RESULT 0x600CU
#define LFSR_POLY   0x80200003U

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

static uint32_t lfsr_hw(uint32_t seed, uint32_t steps)
{
    while ((reg_read8(LFSR_STATUS) & 0x2U) == 0U) {
    }

    reg_write32(LFSR_SEED, seed);
    reg_write32(LFSR_STEPS, steps);

    while ((reg_read8(LFSR_STATUS) & 0x1U) == 0U) {
    }

    return reg_read32(LFSR_RESULT);
}

int main(void)
{
    const uint32_t seed = 0x12345678U;
    const uint32_t steps = 10000U;

    printf("LFSR MMIO accelerator demo\n");
    printf("Seed: 0x%08X, steps: %u\n", seed, steps);

    printf("[Golden Model] First 100 one-step states:\n");
    uint32_t current_sw = seed;
    for (int i = 1; i <= 100; i++) {
        current_sw = lfsr_sw(current_sw, 1);
        printf("%08X ", current_sw);
        if ((i % 10) == 0) {
            printf("\n");
        }
    }

    printf("\n[Hardware] First 100 one-step states:\n");
    uint32_t current_hw = seed;
    for (int i = 1; i <= 100; i++) {
        current_hw = lfsr_hw(current_hw, 1);
        printf("%08X ", current_hw);
        if ((i % 10) == 0) {
            printf("\n");
        }
    }

    unsigned long start = read_mcycle();
    uint32_t sw_result = lfsr_sw(seed, steps);
    unsigned long sw_cycles = read_mcycle() - start;

    start = read_mcycle();
    uint32_t hw_result = lfsr_hw(seed, steps);
    unsigned long hw_cycles = read_mcycle() - start;

    printf("\n[SW] lfsr_sw(seed=0x%08X, steps=%u) = 0x%08X, cycles=%lu\n",
           seed, steps, sw_result, sw_cycles);
    printf("[HW] lfsr_hw(seed=0x%08X, steps=%u) = 0x%08X, cycles=%lu\n",
           seed, steps, hw_result, hw_cycles);

    if (sw_result == hw_result) {
        printf("PASS: hardware result matches the software golden model.\n");
        return 0;
    }

    printf("FAIL: hardware result does not match the software golden model.\n");
    return 1;
}
