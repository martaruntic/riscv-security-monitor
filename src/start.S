# Entry point. QEMU jumps here in M-mode with a0 = hart id.
# Only hart 0 runs the monitor, every other hart is parked.

    .section .text.boot
    .global _start
_start:
    csrr    t0, mhartid
    bnez    t0, park_hart

    # no interrupts until the monitor sets up its own trap handling
    csrw    mie, zero
    csrw    mstatus, zero

    la      sp, __monitor_stack_top

    # zero out .bss
    la      t1, __bss_start
    la      t2, __bss_end
clear_bss:
    bgeu    t1, t2, bss_done
    sd      zero, 0(t1)
    addi    t1, t1, 8
    j       clear_bss
bss_done:

    call    monitor_main

    # monitor_main should never return
park_hart:
    wfi
    j       park_hart
