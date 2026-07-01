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
int32_t a[N], b[N], c_sw[N], c_hw[N];

void vec_add_sw(int32_t *a, int32_t *b, int32_t *c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

void vec_add_hw(int32_t *a, int32_t *b, int32_t *c, size_t n) {
    size_t vl;
    for (; n > 0; n -= vl, a += vl, b += vl, c += vl) {
        vl = __riscv_vsetvl_e32m1(n);
        vint32m1_t va = __riscv_vle32_v_i32m1(a, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(b, vl);
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
        __riscv_vse32_v_i32m1(c, vc, vl);
    }
}

int main(void) {
    printf("========================================\n");
    printf("  RVV (Vector Coprocessor) Array Add Demo\n");
    printf("========================================\n\n");

    for (int i = 0; i < N; i++) {
        a[i] = i;
        b[i] = i * 2;
    }

    unsigned long cycles_start = read_mcycle();
    vec_add_sw(a, b, c_sw, N);
    unsigned long sw_cycles = read_mcycle() - cycles_start;
    
    printf("[SW] Vector Add (N=256) (cycles=%lu)\n", sw_cycles);

    // Enable vector unit in mstatus.VS (bits 9:10)
    unsigned long mstatus;
    asm volatile ("csrr %0, mstatus" : "=r"(mstatus));
    mstatus |= (3 << 9); 
    asm volatile ("csrw mstatus, %0" : : "r"(mstatus));

    cycles_start = read_mcycle();
    vec_add_hw(a, b, c_hw, N);
    unsigned long hw_cycles = read_mcycle() - cycles_start;

    printf("[HW] Vector Add (N=256) (cycles=%lu)\n", hw_cycles);

    int match = 1;
    for (int i = 0; i < N; i++) {
        if (c_sw[i] != c_hw[i]) {
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
