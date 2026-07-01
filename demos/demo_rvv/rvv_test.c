#include <riscv_vector.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

static inline unsigned long read_mcycle(void) {
    unsigned long value;
    asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

#define N 256
uint32_t seeds[N], lfsr_sw_res[N], lfsr_hw_res[N];

void lfsr_sw_array(uint32_t *seeds_in, uint32_t *res, uint32_t steps, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uint32_t lfsr = seeds_in[i];
        for (uint32_t s = 0; s < steps; s++) {
            if (lfsr & 1) lfsr = (lfsr >> 1) ^ 0x80200003;
            else          lfsr = (lfsr >> 1);
        }
        res[i] = lfsr;
    }
}

void lfsr_hw_array(uint32_t *seeds_in, uint32_t *res, uint32_t steps, size_t n) {
    size_t vl;
    for (; n > 0; n -= vl, seeds_in += vl, res += vl) {
        vl = __riscv_vsetvl_e32m1(n);
        vuint32m1_t vlfsr = __riscv_vle32_v_u32m1(seeds_in, vl);
        
        for (uint32_t s = 0; s < steps; s++) {
            vuint32m1_t bit0 = __riscv_vand_vx_u32m1(vlfsr, 1, vl);
            vbool32_t cond = __riscv_vmseq_vx_u32m1_b32(bit0, 1, vl);
            
            vuint32m1_t shifted = __riscv_vsrl_vx_u32m1(vlfsr, 1, vl);
            vuint32m1_t shifted_xor = __riscv_vxor_vx_u32m1(shifted, 0x80200003, vl);
            vlfsr = __riscv_vmerge_vvm_u32m1(shifted, shifted_xor, cond, vl);
        }
        __riscv_vse32_v_u32m1(res, vlfsr, vl);
    }
}

int main(void) {
    printf("========================================\n");
    printf("  RVV (Vector Coprocessor) LFSR Demo\n");
    printf("========================================\n\n");
    fflush(stdout);

    // --- Verification phase: Print the exact same 100 intermediate values ---
    printf("[Golden Model] Dumping first 100 intermediate values for verification:\n");
    uint32_t current_sw = 0x12345678;
    for (int i = 1; i <= 100; i++) {
        uint32_t sw_seed_arr[1] = {current_sw};
        uint32_t sw_res_arr[1];
        lfsr_sw_array(sw_seed_arr, sw_res_arr, 1, 1);
        current_sw = sw_res_arr[0];
        printf("%08X ", current_sw);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

    // Enable vector unit in mstatus.VS (bits 9:10)
    unsigned long mstatus;
    asm volatile ("csrr %0, mstatus" : "=r"(mstatus));
    mstatus |= (3 << 9); 
    asm volatile ("csrw mstatus, %0" : : "r"(mstatus));

    printf("[HW Model] Dumping first 100 intermediate values for verification:\n");
    uint32_t current_hw = 0x12345678;
    for (int i = 1; i <= 100; i++) {
        uint32_t hw_seed_arr[1] = {current_hw};
        uint32_t hw_res_arr[1];
        lfsr_hw_array(hw_seed_arr, hw_res_arr, 1, 1);
        current_hw = hw_res_arr[0];
        printf("%08X ", current_hw);
        if (i % 10 == 0) printf("\n");
    }
    printf("\n");

    // --- Performance phase: Compute parallel LFSRs ---
    uint32_t steps = 100; // reduced steps so RTL sim is fast (~10 seconds)
    for (int i = 0; i < N; i++) {
        seeds[i] = 0x12345678U + (uint32_t)i * 0x11111111U;
    }

    unsigned long cycles_start = read_mcycle();
    lfsr_sw_array(seeds, lfsr_sw_res, steps, N);
    unsigned long sw_cycles = read_mcycle() - cycles_start;
    
    printf("[SW] Vector LFSR (N=%d) (cycles=%lu)\n", N, sw_cycles);
    fflush(stdout);

    cycles_start = read_mcycle();
    lfsr_hw_array(seeds, lfsr_hw_res, steps, N);
    unsigned long hw_cycles = read_mcycle() - cycles_start;

    printf("[HW] Vector LFSR (N=%d) (cycles=%lu)\n", N, hw_cycles);
    fflush(stdout);

    int match = 1;
    for (int i = 0; i < N; i++) {
        if (lfsr_sw_res[i] != lfsr_hw_res[i]) {
            match = 0; break;
        }
    }

    if (match) {
        printf("\nSUCCESS! Results match perfectly.\n");
    } else {
        printf("\nFAIL! Results mismatch.\n");
    }
    
    return 0;
}
