#include "riscv.h"
#include "uart.h"
#include "trap.h"
#include "pmp.h"
#include "memory_rules.h"
#include "secure_storage.h"

extern char __monitor_start[];
extern char __monitor_end[];
extern char __secure_start[];
extern char __secure_end[];
extern char __supervisor_start[];
extern char __supervisor_end[];
extern char supervisor_entry[];

static struct pmp_info pmp;

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
    uart_puts("\nmonitor:         ");
    uart_put_hex((uint64_t)__monitor_start);
    uart_puts(" - ");
    uart_put_hex((uint64_t)__monitor_end);
    uart_puts("\nsecure region:   ");
    uart_put_hex((uint64_t)__secure_start);
    uart_puts(" - ");
    uart_put_hex((uint64_t)__secure_end);
    uart_puts("\nsupervisor:      ");
    uart_put_hex((uint64_t)__supervisor_start);
    uart_puts(" - ");
    uart_put_hex((uint64_t)__supervisor_end);
    uart_puts("\n");
}

static void report_pmp(void)
{
    uart_puts("pmp entries:     ");
    uart_put_dec(pmp.entry_count);
    uart_puts("\npmp granularity: ");
    uart_put_dec(pmp.granularity);
    uart_puts(" bytes\n");
}

// Triggers a breakpoint and checks that we come back with registers intact.
static void test_trap_path(void)
{
    uint64_t before = 0x1234abcd;
    uint64_t after;

    uart_puts("[monitor] testing trap path with ebreak\n");

    // keep the value in a register across the trap
    __asm__ volatile(
        "mv   t3, %1\n"
        "ebreak\n"
        "mv   %0, t3\n"
        : "=r"(after)
        : "r"(before)
        : "t3");

    if (after == before)
        uart_puts("[monitor] back from trap, registers preserved\n");
    else
        uart_puts("[monitor] register was changed by the trap handler!\n");
}

// mret jumps to mepc and switches to the mode stored in mstatus.MPP
static void enter_supervisor(void *entry_point)
{
    uint64_t mstatus = csr_read(mstatus);

    mstatus &= ~MSTATUS_MPP_MASK;
    mstatus |= MSTATUS_MPP_S;
    csr_write(mstatus, mstatus);
    csr_write(mepc, entry_point);

    // no paging for the supervisor yet
    csr_write(satp, 0);

    __asm__ volatile("mret");
}

void monitor_main(void)
{
    uart_init();
    uart_puts("\n[monitor] booted in M-mode\n");

    trap_init();
    pmp_probe(&pmp);

    report_platform();
    report_pmp();

    test_trap_path();

    if (pmp.entry_count == 0) {
        uart_puts("[monitor] no PMP on this hart, can't isolate anything, halting\n");
        halt();
    }

    secure_storage_init();
    memory_rules_init();

    uart_puts("[monitor] memory rules:\n");
    memory_rules_print();

    if (memory_rules_apply(&pmp) != 0) {
        uart_puts("[monitor] failed to set up PMP, halting\n");
        halt();
    }

    uart_puts("[monitor] pmp entries:\n");
    pmp_print_entries(pmp.entry_count);

    uart_puts("[monitor] dropping to S-mode at ");
    uart_put_hex((uint64_t)supervisor_entry);
    uart_puts("\n");
    enter_supervisor(supervisor_entry);

    // mret never comes back here
    halt();
}
