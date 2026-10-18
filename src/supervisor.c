// Code that runs in S-mode. This plays the role of an untrusted kernel:
// it is linked separately from the monitor (see linker.ld) and it can only
// talk to the monitor through ecall.

#include <stdint.h>
#include "mcall.h"

static long monitor_call(long call_number, long argument)
{
    register long a0 __asm__("a0") = argument;
    register long a7 __asm__("a7") = call_number;

    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static void supervisor_puts(const char *str)
{
    while (*str) {
        if (*str == '\n')
            monitor_call(MCALL_PUTCHAR, '\r');
        monitor_call(MCALL_PUTCHAR, *str++);
    }
}

void supervisor_main(void)
{
    supervisor_puts("[supervisor] hello from S-mode\n");

    // the monitor prints which mode a breakpoint came from,
    // so this shows that we really run below M-mode
    __asm__ volatile("ebreak");
    supervisor_puts("[supervisor] back after ebreak\n");

    if (monitor_call(99, 0) == MCALL_UNKNOWN_CALL)
        supervisor_puts("[supervisor] unknown monitor call rejected as expected\n");

    supervisor_puts("[supervisor] done, asking monitor to shut down\n");
    monitor_call(MCALL_SHUTDOWN, 0);
}
