#ifndef RISCV_H
#define RISCV_H

#include <stdint.h>

#define csr_read(csr) ({                                   \
    uint64_t __value;                                      \
    __asm__ volatile("csrr %0, " #csr : "=r"(__value));    \
    __value;                                               \
})

#define csr_write(csr, value) \
    __asm__ volatile("csrw " #csr ", %0" :: "r"((uint64_t)(value)))

// mstatus.MPP field: privilege mode we return to on mret
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_MPP_MASK  (3UL << MSTATUS_MPP_SHIFT)
#define MSTATUS_MPP_U     (0UL << MSTATUS_MPP_SHIFT)
#define MSTATUS_MPP_S     (1UL << MSTATUS_MPP_SHIFT)
#define MSTATUS_MPP_M     (3UL << MSTATUS_MPP_SHIFT)

// misa extension bits
#define MISA_EXT(letter) (1UL << ((letter) - 'A'))

static inline void halt(void)
{
    while (1)
        __asm__ volatile("wfi");
}

#endif
