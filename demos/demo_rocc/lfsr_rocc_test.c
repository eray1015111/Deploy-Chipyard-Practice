#include "rocc.h"
#include <stdint.h>
#include <stdio.h>

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
    
    // Enable RoCC in mstatus (set XS bits 15:16 to 3)
    unsigned long mstatus;
    asm volatile ("csrr %0, mstatus" : "=r"(mstatus));
    mstatus |= (3 << 15);
    asm volatile ("csrw mstatus, %0" : : "r"(mstatus));

    printf("========================================\n");
    printf("  RoCC (Custom Coprocessor) LFSR Demo\n");
    printf("========================================\n\n");

    // --- SW Speed test ---
    unsigned long cycles_start = read_mcycle();
    uint32_t sw_result = lfsr_sw(seed, steps);
    unsigned long sw_cycles = read_mcycle() - cycles_start;
    
    printf("[SW] lfsr_sw: 0x%X (cycles=%lu)\n", sw_result, sw_cycles);

    // --- HW (RoCC) Speed test ---
    cycles_start = read_mcycle();
    
    uint64_t hw_result = 0;
    ROCC_INSTRUCTION_DSS(0, hw_result, seed, steps, 0);

    unsigned long hw_cycles = read_mcycle() - cycles_start;
    
    printf("[HW] lfsr_rocc: 0x%X (cycles=%lu)\n", (uint32_t)hw_result, hw_cycles);

    if (sw_result == hw_result) {
        printf("\nSUCCESS! Results match perfectly.\n");
    } else {
        printf("\nFAIL! Results mismatch.\n");
    }

    return 0;
}
