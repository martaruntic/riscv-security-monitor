# M-mode trap entry.
#
# mscratch always holds the top of the trap stack while we are not
# inside a trap. On entry we swap it with sp, so the handler never
# runs on the stack of the code that trapped (that code might be
# untrusted later on).

    .section .text
    .global trap_vector
    .align 4                    # mtvec needs a 4-byte aligned address
trap_vector:
    csrrw   sp, mscratch, sp    # sp = trap stack, mscratch = old sp
    addi    sp, sp, -288        # TRAP_FRAME_SIZE

    # save x1, x3..x31 (x0 is always zero, x2 is saved below)
    sd      x1,   1*8(sp)
    sd      x3,   3*8(sp)
    sd      x4,   4*8(sp)
    sd      x5,   5*8(sp)
    sd      x6,   6*8(sp)
    sd      x7,   7*8(sp)
    sd      x8,   8*8(sp)
    sd      x9,   9*8(sp)
    sd      x10, 10*8(sp)
    sd      x11, 11*8(sp)
    sd      x12, 12*8(sp)
    sd      x13, 13*8(sp)
    sd      x14, 14*8(sp)
    sd      x15, 15*8(sp)
    sd      x16, 16*8(sp)
    sd      x17, 17*8(sp)
    sd      x18, 18*8(sp)
    sd      x19, 19*8(sp)
    sd      x20, 20*8(sp)
    sd      x21, 21*8(sp)
    sd      x22, 22*8(sp)
    sd      x23, 23*8(sp)
    sd      x24, 24*8(sp)
    sd      x25, 25*8(sp)
    sd      x26, 26*8(sp)
    sd      x27, 27*8(sp)
    sd      x28, 28*8(sp)
    sd      x29, 29*8(sp)
    sd      x30, 30*8(sp)
    sd      x31, 31*8(sp)

    # old sp is in mscratch
    csrr    t0, mscratch
    sd      t0,   2*8(sp)

    csrr    t0, mepc
    sd      t0,  32*8(sp)
    csrr    t0, mstatus
    sd      t0,  33*8(sp)
    csrr    t0, mcause
    sd      t0,  34*8(sp)
    csrr    t0, mtval
    sd      t0,  35*8(sp)

    mv      a0, sp
    call    trap_handler

    # the handler may have changed mepc/mstatus (e.g. to skip an instruction)
    ld      t0,  32*8(sp)
    csrw    mepc, t0
    ld      t0,  33*8(sp)
    csrw    mstatus, t0

    # put the trap stack top back into mscratch for the next trap
    addi    t0, sp, 288
    csrw    mscratch, t0

    ld      x1,   1*8(sp)
    ld      x3,   3*8(sp)
    ld      x4,   4*8(sp)
    ld      x5,   5*8(sp)
    ld      x6,   6*8(sp)
    ld      x7,   7*8(sp)
    ld      x8,   8*8(sp)
    ld      x9,   9*8(sp)
    ld      x10, 10*8(sp)
    ld      x11, 11*8(sp)
    ld      x12, 12*8(sp)
    ld      x13, 13*8(sp)
    ld      x14, 14*8(sp)
    ld      x15, 15*8(sp)
    ld      x16, 16*8(sp)
    ld      x17, 17*8(sp)
    ld      x18, 18*8(sp)
    ld      x19, 19*8(sp)
    ld      x20, 20*8(sp)
    ld      x21, 21*8(sp)
    ld      x22, 22*8(sp)
    ld      x23, 23*8(sp)
    ld      x24, 24*8(sp)
    ld      x25, 25*8(sp)
    ld      x26, 26*8(sp)
    ld      x27, 27*8(sp)
    ld      x28, 28*8(sp)
    ld      x29, 29*8(sp)
    ld      x30, 30*8(sp)
    ld      x31, 31*8(sp)

    ld      sp,   2*8(sp)       # restore sp last
    mret
