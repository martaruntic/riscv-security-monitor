#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

// QEMU virt has a "test" device that can power off the machine.
// Writing PASS ends QEMU with exit code 0, FAIL | (code << 16) with code.
#define TEST_DEVICE_BASE 0x100000UL
#define TEST_DEVICE_PASS 0x5555
#define TEST_DEVICE_FAIL 0x3333

// Fixed addresses, must match linker.ld
#define SECURE_REGION_BASE 0x80100000UL
#define SUPERVISOR_BASE    0x80200000UL

// QEMU exit codes. 0 and 1 come from the supervisor itself through
// MCALL_SHUTDOWN, the others are used when the monitor stops it.
#define EXIT_OK                  0   // supervisor finished normally
#define EXIT_ATTACK_SUCCEEDED    1   // supervisor got past a protection
#define EXIT_UNKNOWN_ATTACK      2   // ATTACK name not found
#define EXIT_ACCESS_VIOLATION    3   // PMP blocked a memory access
#define EXIT_PRIVILEGE_VIOLATION 4   // illegal instruction in S-mode
#define EXIT_SUPERVISOR_CRASH    5   // any other exception in S-mode

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
