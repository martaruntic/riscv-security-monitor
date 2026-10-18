#include "trap.h"
#include "mcall.h"
#include "platform.h"
#include "riscv.h"
#include "uart.h"

extern char trap_vector[];
extern char __trap_stack_top[];

static volatile int probing_csr;
static volatile int probe_trapped;

static const char *exception_names[] = {
    [CAUSE_MISALIGNED_FETCH]    = "instruction address misaligned",
    [CAUSE_FETCH_ACCESS]        = "instruction access fault",
    [CAUSE_ILLEGAL_INSTRUCTION] = "illegal instruction",
    [CAUSE_BREAKPOINT]          = "breakpoint",
    [CAUSE_MISALIGNED_LOAD]     = "load address misaligned",
    [CAUSE_LOAD_ACCESS]         = "load access fault",
    [CAUSE_MISALIGNED_STORE]    = "store address misaligned",
    [CAUSE_STORE_ACCESS]        = "store access fault",
    [CAUSE_ECALL_FROM_U]        = "ecall from U-mode",
    [CAUSE_ECALL_FROM_S]        = "ecall from S-mode",
    [10]                        = "reserved",
    [CAUSE_ECALL_FROM_M]        = "ecall from M-mode",
};

#define EXCEPTION_COUNT (sizeof(exception_names) / sizeof(exception_names[0]))

void trap_init(void)
{
    csr_write(mscratch, __trap_stack_top);
    csr_write(mtvec, trap_vector);   // direct mode, all traps go to trap_vector
}

void csr_probe_begin(void)
{
    probe_trapped = 0;
    probing_csr = 1;
}

int csr_probe_end(void)
{
    probing_csr = 0;
    return probe_trapped;
}

// RISC-V instructions are 4 bytes, compressed ones (C extension) are 2.
// The lowest two bits are 11 only for 4-byte instructions.
static uint64_t instruction_length(uint64_t address)
{
    uint16_t low_half = *(volatile uint16_t *)address;
    return ((low_half & 0x3) == 0x3) ? 4 : 2;
}

// Privilege mode the trap came from, saved by hardware in mstatus.MPP
static const char *previous_mode_name(uint64_t mstatus)
{
    switch (mstatus & MSTATUS_MPP_MASK) {
    case MSTATUS_MPP_U: return "U";
    case MSTATUS_MPP_S: return "S";
    case MSTATUS_MPP_M: return "M";
    default:            return "?";
    }
}

static void print_trap_info(struct trap_frame *frame)
{
    uint64_t cause = frame->mcause & ~MCAUSE_INTERRUPT_BIT;

    uart_puts("[trap] ");
    if (frame->mcause & MCAUSE_INTERRUPT_BIT)
        uart_puts("interrupt");
    else if (cause < EXCEPTION_COUNT && exception_names[cause])
        uart_puts(exception_names[cause]);
    else
        uart_puts("unknown exception");

    uart_puts(" (from ");
    uart_puts(previous_mode_name(frame->mstatus));
    uart_puts("-mode)");
    uart_puts("\n       mcause = ");
    uart_put_hex(frame->mcause);
    uart_puts("\n       mepc   = ");
    uart_put_hex(frame->mepc);
    uart_puts("\n       mtval  = ");
    uart_put_hex(frame->mtval);
    uart_puts("\n");
}

// a7 = call number, a0 = argument, result goes back in a0
static void handle_mcall(struct trap_frame *frame)
{
    uint64_t call_number = frame->regs[17];
    uint64_t argument = frame->regs[10];

    switch (call_number) {
    case MCALL_PUTCHAR:
        uart_putc((char)argument);
        frame->regs[10] = MCALL_OK;
        break;

    case MCALL_SHUTDOWN:
        uart_puts("[monitor] supervisor asked for shutdown, code ");
        uart_put_dec(argument);
        uart_puts("\n");
        platform_poweroff((uint32_t)argument);
        break;

    default:
        frame->regs[10] = (uint64_t)MCALL_UNKNOWN_CALL;
        break;
    }
}

void trap_handler(struct trap_frame *frame)
{
    int from_machine_mode = (frame->mstatus & MSTATUS_MPP_MASK) == MSTATUS_MPP_M;

    // expected fault while probing a CSR, skip the instruction quietly
    if (probing_csr && from_machine_mode &&
        frame->mcause == CAUSE_ILLEGAL_INSTRUCTION) {
        probe_trapped = 1;
        frame->mepc += instruction_length(frame->mepc);
        return;
    }

    // calls from the supervisor are normal traffic, don't print them
    if (frame->mcause == CAUSE_ECALL_FROM_S) {
        handle_mcall(frame);
        frame->mepc += 4;   // ecall is always 4 bytes
        return;
    }

    print_trap_info(frame);

    if (frame->mcause & MCAUSE_INTERRUPT_BIT) {
        uart_puts("[trap] interrupts are not handled yet, halting\n");
        halt();
    }

    switch (frame->mcause) {
    case CAUSE_BREAKPOINT:
        // used for testing the trap path, just continue after it
        frame->mepc += instruction_length(frame->mepc);
        return;

    default:
        uart_puts("[trap] unhandled exception, halting\n");
        halt();
    }
}
