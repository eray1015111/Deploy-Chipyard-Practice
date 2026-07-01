#include "mmio.h"
#include <stdio.h>

#define MAC_STATUS 0x5000
#define MAC_A      0x5004
#define MAC_B      0x5008
#define MAC_C      0x500C
#define MAC_RESULT 0x5010

static inline unsigned long read_mcycle(void) {
    unsigned long value;
    asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static inline unsigned long read_minstret(void) {
    unsigned long value;
    asm volatile ("csrr %0, minstret" : "=r"(value));
    return value;
}

unsigned int mac_ref(unsigned int a, unsigned int b, unsigned int c) {
    return (a * b) + c;
}

int main(void)
{
    uint32_t result, ref;
    unsigned long cycles_start, cycles_end, insn_start, insn_end;
    unsigned long sw_cycles, hw_cycles;

    // Test case 1: 25 * 4 + 12 = 112
    uint32_t a1 = 25, b1 = 4, c1 = 12;
    // Test case 2: 100 * 200 + 5000 = 25000
    uint32_t a2 = 100, b2 = 200, c2 = 5000;
    // Test case 3: 0 * 999 + 42 = 42
    uint32_t a3 = 0, b3 = 999, c3 = 42;

    printf("========================================\n");
    printf("  MAC Accelerator Demo (Verilog BlackBox)\n");
    printf("========================================\n\n");

    // ----- Software baseline -----
    cycles_start = read_mcycle();
    insn_start = read_minstret();
    ref = mac_ref(a1, b1, c1);
    insn_end = read_minstret();
    cycles_end = read_mcycle();
    sw_cycles = cycles_end - cycles_start;
    printf("[SW] mac_ref(%u, %u, %u) = %u  (cycles=%lu, insns=%lu)\n",
           a1, b1, c1, ref, sw_cycles, insn_end - insn_start);

    // ----- Hardware accelerator test 1 -----
    // wait for peripheral to be ready
    while ((reg_read8(MAC_STATUS) & 0x2) == 0) ;

    cycles_start = read_mcycle();
    reg_write32(MAC_A, a1);
    reg_write32(MAC_B, b1);
    reg_write32(MAC_C, c1);  // triggers computation

    // wait for result to be valid
    while ((reg_read8(MAC_STATUS) & 0x1) == 0) ;

    result = reg_read32(MAC_RESULT);
    cycles_end = read_mcycle();
    hw_cycles = cycles_end - cycles_start;

    if (result != ref) {
        printf("[HW] FAIL: Test 1: got %u, expected %u\n", result, ref);
        return 1;
    }
    printf("[HW] mac_hw(%u, %u, %u) = %u  (cycles=%lu) -- PASS\n",
           a1, b1, c1, result, hw_cycles);

    // ----- Hardware accelerator test 2 -----
    while ((reg_read8(MAC_STATUS) & 0x2) == 0) ;
    cycles_start = read_mcycle();
    reg_write32(MAC_A, a2);
    reg_write32(MAC_B, b2);
    reg_write32(MAC_C, c2);
    while ((reg_read8(MAC_STATUS) & 0x1) == 0) ;
    result = reg_read32(MAC_RESULT);
    cycles_end = read_mcycle();
    ref = mac_ref(a2, b2, c2);
    if (result != ref) {
        printf("[HW] FAIL: Test 2: got %u, expected %u\n", result, ref);
        return 1;
    }
    printf("[HW] mac_hw(%u, %u, %u) = %u  (cycles=%lu) -- PASS\n",
           a2, b2, c2, result, cycles_end - cycles_start);

    // ----- Hardware accelerator test 3 (zero multiply) -----
    while ((reg_read8(MAC_STATUS) & 0x2) == 0) ;
    cycles_start = read_mcycle();
    reg_write32(MAC_A, a3);
    reg_write32(MAC_B, b3);
    reg_write32(MAC_C, c3);
    while ((reg_read8(MAC_STATUS) & 0x1) == 0) ;
    result = reg_read32(MAC_RESULT);
    cycles_end = read_mcycle();
    ref = mac_ref(a3, b3, c3);
    if (result != ref) {
        printf("[HW] FAIL: Test 3: got %u, expected %u\n", result, ref);
        return 1;
    }
    printf("[HW] mac_hw(%u, %u, %u) = %u  (cycles=%lu) -- PASS\n",
           a3, b3, c3, result, cycles_end - cycles_start);

    printf("\n========================================\n");
    printf("  ALL 3 TESTS PASSED\n");
    printf("========================================\n");
    return 0;
}
