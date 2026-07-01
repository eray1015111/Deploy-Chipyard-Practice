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
        uint64_t hw_res_val = 0;
        ROCC_INSTRUCTION_DSS(0, hw_res_val, current_hw, 1, 0);
        current_hw = (uint32_t)hw_res_val;
        printf("%08X ", current_hw);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

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
