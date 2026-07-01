#ifndef UART_H
#define UART_H

#include <stdint.h>

static inline void uart_putchar(char c) {
    volatile uint32_t *uart_tx = (volatile uint32_t *)0x10020000;
    while ((*uart_tx) & 0x80000000);
    *uart_tx = c;
}

static inline void uart_print_str(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putchar('\r');
        uart_putchar(*s++);
    }
}

static inline void uart_print_hex(uint64_t val) {
    uart_print_str("0x");
    if (val == 0) { uart_putchar('0'); return; }
    int started = 0;
    for (int i = 15; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        if (nibble != 0 || started || i == 0) {
            started = 1;
            uart_putchar(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
        }
    }
}

static inline void uart_print_dec(uint64_t val) {
    if (val == 0) { uart_putchar('0'); return; }
    char buf[20];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        uart_putchar(buf[--i]);
    }
}

#endif
