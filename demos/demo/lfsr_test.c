#include "mmio.h"
#include <stdio.h>

#define LFSR_STATUS 0x6000
#define LFSR_SEED   0x6004
#define LFSR_STEPS  0x6008
#define LFSR_RESULT 0x600C

static inline unsigned long read_mcycle(void) {
    unsigned long value;
    asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

unsigned int lfsr_sw(unsigned int seed, unsigned int steps) {
    unsigned int lfsr = seed;
    for (unsigned int i = 0; i < steps; i++) {
        if (lfsr & 1) {
            lfsr = (lfsr >> 1) ^ 0x80200003;
        } else {
            lfsr = (lfsr >> 1);
        }
    }
    return lfsr;
}

int main(void)
{
    uint32_t seed = 0x12345678;
    uint32_t steps = 10000;
    
    uint32_t sw_result, hw_result;
    unsigned long cycles_start, cycles_end;
    unsigned long sw_cycles, hw_cycles;

    printf("========================================\n");
    printf("  LFSR Hardware Accelerator Demo\n");
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
    for (int i = 1; i <= 100; i++) {
        while ((reg_read8(LFSR_STATUS) & 0x2) == 0) ;
        reg_write32(LFSR_SEED, current_hw);
        reg_write32(LFSR_STEPS, 1);
        while ((reg_read8(LFSR_STATUS) & 0x1) == 0) ;
        current_hw = reg_read32(LFSR_RESULT);
        printf("%08X ", current_hw);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

    // ----- Software baseline -----
    cycles_start = read_mcycle();
    sw_result = lfsr_sw(seed, steps);
    cycles_end = read_mcycle();
    sw_cycles = cycles_end - cycles_start;
    
    printf("[SW] lfsr_sw(seed=0x%X, steps=%u) = 0x%X\n", seed, steps, sw_result);
    printf("     Cycles consumed: %lu\n\n", sw_cycles);

    // ----- Hardware accelerator test -----
    // wait for peripheral to be ready
    while ((reg_read8(LFSR_STATUS) & 0x2) == 0) ;

    cycles_start = read_mcycle();
    reg_write32(LFSR_SEED, seed);
    reg_write32(LFSR_STEPS, steps); // triggers computation

    // wait for result to be valid
    while ((reg_read8(LFSR_STATUS) & 0x1) == 0) ;

    hw_result = reg_read32(LFSR_RESULT);
    cycles_end = read_mcycle();
    hw_cycles = cycles_end - cycles_start;

    printf("[HW] lfsr_hw(seed=0x%X, steps=%u) = 0x%X\n", seed, steps, hw_result);
    printf("     Cycles consumed: %lu  (includes MMIO overhead)\n\n", hw_cycles);

    if (sw_result != hw_result) {
        printf("FAIL: Results do not match!\n");
        return 1;
    }
    
    printf("SUCCESS! Results match perfectly.\n");
    printf("Hardware Speedup: %lu / %lu = %.2f X\n", sw_cycles, hw_cycles, (float)sw_cycles / (float)hw_cycles);
    printf("========================================\n");
    return 0;
}
