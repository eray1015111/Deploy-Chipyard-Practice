#include <stdio.h>
#include <stdint.h>

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
    
    uint32_t sw_result;
    unsigned long cycles_start, cycles_end;
    unsigned long sw_cycles;

    printf("========================================\n");
    printf("  LFSR Pure Software Demo (Golden Model)\n");
    printf("========================================\n\n");

    // ----- Correctness Dump (First 100 steps) -----
    printf("[Golden Model] Dumping first 100 intermediate values for verification:\n");
    uint32_t current_val = seed;
    for (int i = 1; i <= 100; i++) {
        current_val = lfsr_sw(current_val, 1);
        printf("%08X ", current_val);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

    // ----- Software baseline (Performance Test) -----
    printf("Running Performance Test (steps=%u)...\n", steps);
    cycles_start = read_mcycle();
    sw_result = lfsr_sw(seed, steps);
    cycles_end = read_mcycle();
    sw_cycles = cycles_end - cycles_start;
    
    printf("[SW] lfsr_sw(seed=0x%X, steps=%u) = 0x%X\n", seed, steps, sw_result);
    printf("     Cycles consumed: %lu\n\n", sw_cycles);

    printf("SUCCESS! Pure Software simulation completed.\n");
    printf("========================================\n");
    
    for (volatile int i = 0; i < 10000; i++);
    return 0;
}
