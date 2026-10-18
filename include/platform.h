#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

// QEMU virt has a "test" device that can power off the machine.
// Writing PASS ends QEMU with exit code 0, FAIL | (code << 16) with code.
#define TEST_DEVICE_BASE 0x100000UL
#define TEST_DEVICE_PASS 0x5555
#define TEST_DEVICE_FAIL 0x3333

// Where the untrusted supervisor code is linked (see linker.ld)
#define SUPERVISOR_BASE 0x80200000UL

static inline void platform_poweroff(uint32_t exit_code)
{
    volatile uint32_t *test_device = (volatile uint32_t *)TEST_DEVICE_BASE;

    if (exit_code == 0)
        *test_device = TEST_DEVICE_PASS;
    else
        *test_device = (exit_code << 16) | TEST_DEVICE_FAIL;

    while (1)
        __asm__ volatile("wfi");
}

#endif
