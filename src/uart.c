#include "uart.h"

// NS16550A UART on the QEMU virt machine
#define UART_BASE 0x10000000UL

#define REG_THR 0   // transmit holding register (write)
#define REG_IER 1   // interrupt enable register
#define REG_FCR 2   // FIFO control register
#define REG_LCR 3   // line control register
#define REG_LSR 5   // line status register

#define LSR_TX_EMPTY (1 << 5)

static volatile uint8_t *const uart = (volatile uint8_t *)UART_BASE;

void uart_init(void)
{
    uart[REG_IER] = 0x00;   // polling only, no UART interrupts
    uart[REG_LCR] = 0x03;   // 8 data bits, no parity, 1 stop bit
    uart[REG_FCR] = 0x01;   // enable FIFO
}

void uart_putc(char c)
{
    while ((uart[REG_LSR] & LSR_TX_EMPTY) == 0)
        ;
    uart[REG_THR] = (uint8_t)c;
}

void uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n')
            uart_putc('\r');
        uart_putc(*str++);
    }
}

void uart_put_hex(uint64_t value)
{
    const char *digits = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc(digits[(value >> shift) & 0xf]);
}

void uart_put_dec(uint64_t value)
{
    char reversed[20];
    int length = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }
    while (value > 0) {
        reversed[length++] = '0' + (value % 10);
        value /= 10;
    }
    while (length > 0)
        uart_putc(reversed[--length]);
}
