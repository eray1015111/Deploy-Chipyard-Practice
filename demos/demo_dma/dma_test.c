#include <stdint.h>
#include <stdio.h>

#include "mmio.h"

#define DMA_STATUS 0x7000U
#define DMA_ADDR   0x7008U
#define DMA_STEPS  0x7010U
#define DMA_RESULT 0x7018U
#define LFSR_POLY  0x80200003U

static volatile uint32_t mem_array[2] __attribute__((aligned(64)));

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

static uint32_t lfsr_dma(uint32_t seed, uint32_t steps)
{
    mem_array[0] = seed;
    mem_array[1] = steps;
    asm volatile("fence rw, rw" ::: "memory");

    while ((reg_read8(DMA_STATUS) & 0x2U) == 0U) {
    }

    reg_write64(DMA_ADDR, (uint64_t)(uintptr_t)&mem_array[0]);
    reg_write32(DMA_STEPS, steps);

    while ((reg_read8(DMA_STATUS) & 0x1U) == 0U) {
    }

    asm volatile("fence rw, rw" ::: "memory");
    (void)reg_read32(DMA_RESULT);
    return mem_array[0];
}

int main(void)
{
    const uint32_t seed = 0x12345678U;
    const uint32_t steps = 10000U;

    printf("TileLink DMA LFSR demo\n");
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

    printf("\n[DMA] First 100 one-step states:\n");
    uint32_t current_hw = seed;
    for (int i = 1; i <= 100; i++) {
        current_hw = lfsr_dma(current_hw, 1);
        printf("%08X ", current_hw);
        if ((i % 10) == 0) {
            printf("\n");
        }
    }

    unsigned long start = read_mcycle();
    uint32_t sw_result = lfsr_sw(seed, steps);
    unsigned long sw_cycles = read_mcycle() - start;

    start = read_mcycle();
    uint32_t hw_result = lfsr_dma(seed, steps);
    unsigned long hw_cycles = read_mcycle() - start;

    printf("\n[SW] lfsr_sw:  0x%08X, cycles=%lu\n", sw_result, sw_cycles);
    printf("[DMA] lfsr_dma: 0x%08X, cycles=%lu\n", hw_result, hw_cycles);

    if (sw_result == hw_result) {
        printf("PASS: DMA result matches the software golden model.\n");
        return 0;
    }

    printf("FAIL: DMA result does not match the software golden model.\n");
    return 1;
}
