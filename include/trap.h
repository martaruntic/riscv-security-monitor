#ifndef TRAP_H
#define TRAP_H

#include <stdint.h>

// Saved registers of whoever was running when the trap happened.
// Layout must match the offsets used in trap.S
struct trap_frame {
    uint64_t regs[32];  // x0..x31, regs[0] is unused
    uint64_t mepc;
    uint64_t mstatus;
    uint64_t mcause;
    uint64_t mtval;
};

#define TRAP_FRAME_SIZE (36 * 8)

// mcause values for synchronous exceptions
#define CAUSE_MISALIGNED_FETCH    0
#define CAUSE_FETCH_ACCESS        1
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_BREAKPOINT          3
#define CAUSE_MISALIGNED_LOAD     4
#define CAUSE_LOAD_ACCESS         5
#define CAUSE_MISALIGNED_STORE    6
#define CAUSE_STORE_ACCESS        7
#define CAUSE_ECALL_FROM_U        8
#define CAUSE_ECALL_FROM_S        9
#define CAUSE_ECALL_FROM_M        11

#define MCAUSE_INTERRUPT_BIT (1UL << 63)

void trap_init(void);
void trap_handler(struct trap_frame *frame);

// While probing, an illegal instruction in M-mode is expected (the CSR
// may not exist), so the handler skips it instead of halting.
// csr_probe_end returns 1 if something trapped since csr_probe_begin.
void csr_probe_begin(void);
int csr_probe_end(void);

#endif
