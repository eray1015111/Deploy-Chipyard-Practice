#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define N 256
#define LFSR_POLY 0x80200003U

static uint32_t seeds[N];
static uint32_t lfsr_sw_res[N];
static uint32_t lfsr_vec_res[N];

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

void lfsr_sw_array(const uint32_t *seeds_in, uint32_t *res, uint32_t steps, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint32_t lfsr = seeds_in[i];
        for (uint32_t s = 0; s < steps; s++) {
            lfsr = lfsr_step(lfsr);
        }
        res[i] = lfsr;
    }
}

void lfsr_vec_array(const uint32_t *seeds_in, uint32_t *res, uint32_t steps, size_t n)
{
    size_t vl;

    for (; n > 0; n -= vl, seeds_in += vl, res += vl) {
        vl = __riscv_vsetvl_e32m1(n);
        vuint32m1_t vlfsr = __riscv_vle32_v_u32m1(seeds_in, vl);

        for (uint32_t s = 0; s < steps; s++) {
            vuint32m1_t bit0 = __riscv_vand_vx_u32m1(vlfsr, 1U, vl);
            vbool32_t cond = __riscv_vmseq_vx_u32m1_b32(bit0, 1U, vl);
            vuint32m1_t shifted = __riscv_vsrl_vx_u32m1(vlfsr, 1U, vl);
            vuint32m1_t shifted_xor = __riscv_vxor_vx_u32m1(shifted, LFSR_POLY, vl);
            vlfsr = __riscv_vmerge_vvm_u32m1(shifted, shifted_xor, cond, vl);
        }

        __riscv_vse32_v_u32m1(res, vlfsr, vl);
    }
}

int main(void)
{
    const uint32_t steps = 100U;

    for (int i = 0; i < N; i++) {
        seeds[i] = 0x12345678U + (uint32_t)i * 0x11111111U;
    }

    printf("RVV vector LFSR demo\n");
    printf("Elements: %d, steps per element: %u\n", N, steps);

    printf("[Golden Model] First 100 one-step states for seed 0x12345678:\n");
    uint32_t current_sw = 0x12345678U;
    for (int i = 1; i <= 100; i++) {
        current_sw = lfsr_step(current_sw);
        printf("%08X ", current_sw);
        if ((i % 10) == 0) {
            printf("\n");
        }
    }

    printf("\n[RVV] First 100 one-step states for seed 0x12345678:\n");
    uint32_t current_vec = 0x12345678U;
    for (int i = 1; i <= 100; i++) {
        uint32_t in[1] = {current_vec};
        uint32_t out[1] = {0};
        lfsr_vec_array(in, out, 1, 1);
        current_vec = out[0];
        printf("%08X ", current_vec);
        if ((i % 10) == 0) {
            printf("\n");
        }
    }

    unsigned long start = read_mcycle();
    lfsr_sw_array(seeds, lfsr_sw_res, steps, N);
    unsigned long sw_cycles = read_mcycle() - start;

    start = read_mcycle();
    lfsr_vec_array(seeds, lfsr_vec_res, steps, N);
    unsigned long vec_cycles = read_mcycle() - start;

    int match = 1;
    for (int i = 0; i < N; i++) {
        if (lfsr_sw_res[i] != lfsr_vec_res[i]) {
            match = 0;
            printf("Mismatch at %d: sw=0x%08X rvv=0x%08X\n",
                   i, lfsr_sw_res[i], lfsr_vec_res[i]);
            break;
        }
    }

    printf("\n[SW] Vector baseline cycles=%lu\n", sw_cycles);
    printf("[RVV] Vector LFSR cycles=%lu\n", vec_cycles);

    if (match) {
        printf("PASS: RVV result matches the software golden model.\n");
        return 0;
    }

    printf("FAIL: RVV result does not match the software golden model.\n");
    return 1;
}
