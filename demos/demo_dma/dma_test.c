#include "mmio.h"
#include <stdint.h>
#include <stdio.h>

#define DMA_STATUS 0x7000
#define DMA_ADDR   0x7008
#define DMA_COUNT  0x7010
#define DMA_RESULT 0x7014

static inline unsigned long read_mcycle(void) {
    unsigned long value;
    asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

unsigned int lfsr_sw(unsigned int seed, unsigned int steps) {
    unsigned int lfsr = seed;
    for (unsigned int i = 0; i < steps; i++) {
        if (lfsr & 1) lfsr = (lfsr >> 1) ^ 0x80200003;
        else          lfsr = (lfsr >> 1);
    }
    return lfsr;
}

int main(void) {
    uint32_t seed = 0x12345678, steps = 10000;
    
    uint32_t mem_array[2] __attribute__((aligned(8)));
    mem_array[0] = seed;
    mem_array[1] = steps;
    
    printf("========================================\n");
    printf("  TileLink Master with DMA LFSR Demo\n");
    printf("========================================\n\n");


    printf("[Golden Model] Dumping first 100 intermediate values for verification:\n");
    uint32_t current_sw = seed;
    for (int i = 1; i <= 100; i++) {
        current_sw = lfsr_sw(current_sw, 1);
        printf("%08X ", current_sw);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

    printf("[HW Model] Dumping first 100 intermediate values for verification:\n");
    uint32_t current_hw = seed;
    uint32_t dma_array[2] __attribute__((aligned(8)));
    for (int i = 1; i <= 100; i++) {
        dma_array[0] = current_hw;
        dma_array[1] = 1;
        while ((reg_read8(DMA_STATUS) & 0x2) == 0) ;
        reg_write64(DMA_ADDR, (uint64_t)(uintptr_t)dma_array);
        reg_write32(DMA_COUNT, 1);
        while ((reg_read8(DMA_STATUS) & 0x1) == 0) ;
        current_hw = reg_read32(DMA_RESULT);
        printf("%08X ", current_hw);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

    unsigned long cycles_start = read_mcycle();
    uint32_t sw_result = lfsr_sw(seed, steps);
    unsigned long sw_cycles = read_mcycle() - cycles_start;
    
    printf("[SW] lfsr_sw: 0x%X (cycles=%lu)\n", sw_result, sw_cycles);

    while ((reg_read8(DMA_STATUS) & 0x2) == 0) ;

    cycles_start = read_mcycle();
    reg_write64(DMA_ADDR, (uint64_t)(uintptr_t)mem_array);
    reg_write32(DMA_COUNT, steps); 

    while ((reg_read8(DMA_STATUS) & 0x1) == 0) ;

    uint32_t hw_result = reg_read32(DMA_RESULT);
    unsigned long hw_cycles = read_mcycle() - cycles_start;
    
    printf("[HW] lfsr_dma: 0x%X (cycles=%lu)\n", hw_result, hw_cycles);

    if (sw_result == hw_result) {
        printf("\nSUCCESS! Results match perfectly.\n");
    } else {
        printf("\nFAIL! Results mismatch.\n");
    }

    return 0;
}
