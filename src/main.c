#include "riscv.h"
#include "uart.h"

extern char __monitor_end[];

// Prints which privilege modes the hart supports, based on misa.
// The monitor needs at least M + one lower mode to isolate anything.
static void report_platform(void)
{
    uint64_t misa = csr_read(misa);

    uart_puts("hart id:         ");
    uart_put_dec(csr_read(mhartid));
    uart_puts("\nmisa:            ");
    uart_put_hex(misa);
    uart_puts("\nsupervisor mode: ");
    uart_puts((misa & MISA_EXT('S')) ? "yes" : "no");
    uart_puts("\nuser mode:       ");
    uart_puts((misa & MISA_EXT('U')) ? "yes" : "no");
    uart_puts("\nmonitor image ends at ");
    uart_put_hex((uint64_t)__monitor_end);
    uart_puts("\n");
}

void monitor_main(void)
{
    uart_init();
    uart_puts("\n[monitor] booted in M-mode\n");

    report_platform();

    uart_puts("[monitor] nothing else to do yet, halting\n");
    halt();
}
