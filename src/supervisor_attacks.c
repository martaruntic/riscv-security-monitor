// Attacks a compromised supervisor could try against the monitor.
// Each one is built and run on its own: make run ATTACK=<name>
//
// The supervisor ends the run itself only when it is done:
//   exit 0  the attack was rejected by a monitor call check (blocked)
//   exit 1  the attack got through (the protection is broken)
// If the hardware or the monitor stops it, the exit code comes from the
// monitor instead (3 access violation, 4 privilege violation).
// tests/run_attacks.sh knows which result each attack should end with.

#include <stdint.h>
#include "mcall.h"
#include "platform.h"
#include "secure_storage.h"
#include "supervisor.h"

// Monitor symbols. An attacker that has the monitor binary knows these
// addresses anyway, so it is fair to use them here.
extern char __monitor_start[];
extern char __trap_stack_top[];
extern uint32_t trap_vector[];   // uint32_t so the store below is one sw
extern void trap_handler(void);

#define UNMAPPED_RAM_ADDRESS 0x80400000UL
#define UART_ADDRESS         0x10000000UL

struct attack {
    const char *name;
    const char *description;
    void (*run)(void);
};

static void attack_succeeded(const char *what)
{
    supervisor_puts("[attack] SUCCEEDED: ");
    supervisor_puts(what);
    supervisor_puts("\n");
    supervisor_shutdown(EXIT_ATTACK_SUCCEEDED);
}

static void attack_blocked(const char *what)
{
    supervisor_puts("[attack] blocked: ");
    supervisor_puts(what);
    supervisor_puts("\n");
    supervisor_shutdown(EXIT_OK);
}

static uint64_t read_address(uint64_t address)
{
    return *(volatile uint64_t *)address;
}

static void write_address(uint64_t address, uint64_t value)
{
    *(volatile uint64_t *)address = value;
}

static void jump_to_address(uint64_t address)
{
    void (*target)(void) = (void (*)(void))address;
    target();
}

// ---------------- sanity check ----------------

// Everything here is allowed. If this fails, the rules are too strict.
static void legitimate_calls(void)
{
    struct mcall_result result;

    result = monitor_call(MCALL_SECURE_READ, SLOT_MONITOR_VERSION, 0);
    if (result.status != MCALL_OK || result.value != 1)
        attack_succeeded("allowed read of the version slot failed");

    result = monitor_call(MCALL_SECURE_WRITE, SLOT_SUPERVISOR_DATA, 1234);
    if (result.status != MCALL_OK)
        attack_succeeded("allowed write to the data slot failed");

    result = monitor_call(MCALL_SECURE_READ, SLOT_SUPERVISOR_DATA, 0);
    if (result.status != MCALL_OK || result.value != 1234)
        attack_succeeded("data slot did not keep the written value");

    supervisor_puts("[attack] allowed calls work\n");
    supervisor_shutdown(EXIT_OK);
}

// ---------------- monitor memory ----------------

static void read_monitor_code(void)
{
    uint64_t value = read_address((uint64_t)__monitor_start);
    (void)value;
    attack_succeeded("read monitor code");
}

// The trap stack holds the saved registers of every trap. Changing it
// would let the supervisor pick register values the monitor returns with.
static void write_trap_stack(void)
{
    write_address((uint64_t)__trap_stack_top - 8, 0xdeadbeef);
    attack_succeeded("wrote to the monitor trap stack");
}

// Replace the first instruction of the trap handler, so the next trap
// would run supervisor-chosen code in M-mode.
static void overwrite_trap_vector(void)
{
    *(volatile uint32_t *)trap_vector = 0x00000013;   // nop
    attack_succeeded("overwrote the trap vector");
}

static void jump_into_monitor(void)
{
    jump_to_address((uint64_t)trap_handler);
    attack_succeeded("executed monitor code from S-mode");
}

// ---------------- secure region ----------------

static void read_secure_region(void)
{
    uint64_t secret = read_address(SECURE_REGION_BASE + SLOT_SECRET * 8);
    (void)secret;
    attack_succeeded("read the secret directly");
}

static void write_secure_region(void)
{
    write_address(SECURE_REGION_BASE + SLOT_SECRET * 8, 0);
    attack_succeeded("overwrote the secret directly");
}

static void execute_secure_region(void)
{
    jump_to_address(SECURE_REGION_BASE);
    attack_succeeded("executed from the secure region");
}

// ---------------- devices and other memory ----------------

// The supervisor has to print through the monitor, not on its own
static void write_uart_directly(void)
{
    *(volatile uint8_t *)UART_ADDRESS = 'X';
    attack_succeeded("wrote to the UART directly");
}

// Powering off on its own would let the supervisor hide what happened
static void poweroff_directly(void)
{
    *(volatile uint32_t *)TEST_DEVICE_BASE = TEST_DEVICE_PASS;
    attack_succeeded("powered off the machine directly");
}

static void read_unmapped_ram(void)
{
    uint64_t value = read_address(UNMAPPED_RAM_ADDRESS);
    (void)value;
    attack_succeeded("read RAM outside of any region");
}

// ---------------- paging ----------------

#define SATP_MODE_SV39 (8UL << 60)
#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_A (1 << 6)
#define PTE_D (1 << 7)

#define GIGAPAGE_SIZE   (1UL << 30)
#define ALIAS_BASE      0xC0000000UL   // virtual alias of physical 0x80000000

static uint64_t root_page_table[512] __attribute__((aligned(4096)));

static uint64_t gigapage_entry(uint64_t physical_address)
{
    return ((physical_address >> 12) << 10) |
           PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D;
}

// The supervisor controls its own page tables. It maps the secure region
// at a second virtual address and reads it through that address.
// PMP checks the physical address after translation, so this should fail
// exactly like a direct read.
static void alias_through_paging(void)
{
    uint64_t ram_start = 0x80000000UL;
    uint64_t alias_of_secret = ALIAS_BASE + (SECURE_REGION_BASE - ram_start) +
                               SLOT_SECRET * 8;

    // .bss of the supervisor is not cleared at boot, so clear it here
    for (int i = 0; i < 512; i++)
        root_page_table[i] = 0;

    // identity map for the code that is running right now
    root_page_table[ram_start / GIGAPAGE_SIZE] = gigapage_entry(ram_start);
    // second mapping of the same physical memory
    root_page_table[ALIAS_BASE / GIGAPAGE_SIZE] = gigapage_entry(ram_start);

    __asm__ volatile("csrw satp, %0\n"
                     "sfence.vma zero, zero"
                     :: "r"(SATP_MODE_SV39 | ((uint64_t)root_page_table >> 12))
                     : "memory");

    supervisor_puts("[attack] paging on, reading secret through ");
    supervisor_put_hex(alias_of_secret);
    supervisor_puts("\n");

    uint64_t secret = read_address(alias_of_secret);
    (void)secret;
    attack_succeeded("read the secret through a paging alias");
}

// ---------------- privileged instructions ----------------

// Turning PMP off would remove every rule at once
static void write_pmp_config(void)
{
    __asm__ volatile("csrw pmpcfg0, zero");
    attack_succeeded("changed the PMP config");
}

// Pointing mtvec at supervisor code would make it run in M-mode on the next trap
static void write_trap_vector_register(void)
{
    __asm__ volatile("csrw mtvec, %0" :: "r"((uint64_t)SUPERVISOR_BASE));
    attack_succeeded("changed mtvec");
}

// mscratch holds the trap stack address
static void read_mscratch(void)
{
    uint64_t value;
    __asm__ volatile("csrr %0, mscratch" : "=r"(value));
    (void)value;
    attack_succeeded("read mscratch");
}

// mret only exists in M-mode, it can't be used to climb up
static void mret_from_supervisor(void)
{
    __asm__ volatile("mret");
    attack_succeeded("executed mret in S-mode");
}

// ---------------- monitor call interface ----------------

// Pretend the attacker learned the secret some other way. After the
// lockout even the right value must not be accepted.
#define STOLEN_SECRET 0x3c5a7e19d24b8f61UL

static void brute_force_secret(void)
{
    struct mcall_result result;

    for (int attempt = 0; attempt < MAX_VERIFY_ATTEMPTS; attempt++)
        monitor_call(MCALL_SECRET_VERIFY, attempt, 0);

    result = monitor_call(MCALL_SECRET_VERIFY, STOLEN_SECRET, 0);
    if (result.status == MCALL_OK && result.value == 1)
        attack_succeeded("secret accepted after the lockout");

    supervisor_puts("[attack] right secret after 3 wrong guesses -> ");
    supervisor_puts(status_name(result.status));
    supervisor_puts("\n");
    attack_blocked("verify is locked after too many guesses");
}

// Slot numbers are used as an array index inside the monitor
static void bad_slot_index(void)
{
    uint64_t bad_slots[] = { SECURE_SLOT_COUNT, (uint64_t)-1, 1UL << 63, 0x1000 };

    for (unsigned i = 0; i < sizeof(bad_slots) / sizeof(bad_slots[0]); i++) {
        struct mcall_result read = monitor_call(MCALL_SECURE_READ, bad_slots[i], 0);
        struct mcall_result write = monitor_call(MCALL_SECURE_WRITE, bad_slots[i], 0);

        if (read.status != MCALL_INVALID_ARGUMENT ||
            write.status != MCALL_INVALID_ARGUMENT)
            attack_succeeded("monitor accepted an out of range slot");
    }
    attack_blocked("all out of range slots rejected");
}

static void unknown_call_numbers(void)
{
    long bad_calls[] = { 0, 6, 1000, -1 };

    for (unsigned i = 0; i < sizeof(bad_calls) / sizeof(bad_calls[0]); i++) {
        if (monitor_call(bad_calls[i], 0, 0).status != MCALL_UNKNOWN_CALL)
            attack_succeeded("monitor accepted an unknown call");
    }
    attack_blocked("all unknown calls rejected");
}

// If the monitor saved registers on the caller's stack, the supervisor
// could change them while the monitor runs (or point sp into the monitor).
// Here sp points to a buffer full of a known pattern during an ecall.
// The monitor uses its own trap stack, so the buffer must stay untouched.
#define CANARY 0x5a5a5a5a5a5a5a5aUL

static uint64_t fake_stack[64];

static void trap_uses_caller_stack(void)
{
    for (int i = 0; i < 64; i++)
        fake_stack[i] = CANARY;

    __asm__ volatile("mv   t6, sp\n"
                     "mv   sp, %0\n"
                     "li   a7, %1\n"
                     "li   a0, '.'\n"
                     "ecall\n"
                     "mv   sp, t6\n"
                     :: "r"(&fake_stack[64]), "i"(MCALL_PUTCHAR)
                     : "a0", "a1", "a7", "t6", "memory");
    supervisor_puts("\n");

    for (int i = 0; i < 64; i++) {
        if (fake_stack[i] != CANARY)
            attack_succeeded("monitor wrote to the supervisor stack");
    }
    attack_blocked("monitor did not touch the supervisor stack");
}

// ---------------- list ----------------

static const struct attack attacks[] = {
    { "legitimate_calls",       "allowed monitor calls still work",      legitimate_calls },
    { "read_monitor_code",      "load from monitor code",                read_monitor_code },
    { "write_trap_stack",       "store to the monitor trap stack",       write_trap_stack },
    { "overwrite_trap_vector",  "store to the trap handler code",        overwrite_trap_vector },
    { "jump_into_monitor",      "execute monitor code in S-mode",        jump_into_monitor },
    { "read_secure_region",     "load the secret directly",              read_secure_region },
    { "write_secure_region",    "store to the secret directly",          write_secure_region },
    { "execute_secure_region",  "execute from the secure region",        execute_secure_region },
    { "write_uart_directly",    "store to the UART",                     write_uart_directly },
    { "poweroff_directly",      "store to the QEMU test device",         poweroff_directly },
    { "read_unmapped_ram",      "load RAM that no rule covers",          read_unmapped_ram },
    { "alias_through_paging",   "map the secret at a second address",    alias_through_paging },
    { "write_pmp_config",       "csrw pmpcfg0",                          write_pmp_config },
    { "write_mtvec",            "csrw mtvec",                            write_trap_vector_register },
    { "read_mscratch",          "csrr mscratch",                         read_mscratch },
    { "mret_from_supervisor",   "mret in S-mode",                        mret_from_supervisor },
    { "brute_force_secret",     "guess the secret past the lockout",     brute_force_secret },
    { "bad_slot_index",         "out of range slot numbers",             bad_slot_index },
    { "unknown_call_numbers",   "monitor calls that don't exist",        unknown_call_numbers },
    { "trap_uses_caller_stack", "ecall with sp pointing to a canary",    trap_uses_caller_stack },
};

#define ATTACK_COUNT (sizeof(attacks) / sizeof(attacks[0]))

static int names_equal(const char *first, const char *second)
{
    while (*first && *first == *second) {
        first++;
        second++;
    }
    return *first == *second;
}

void run_attack(const char *name)
{
    for (unsigned i = 0; i < ATTACK_COUNT; i++) {
        if (names_equal(name, attacks[i].name)) {
            supervisor_puts("[attack] ");
            supervisor_puts(attacks[i].name);
            supervisor_puts(": ");
            supervisor_puts(attacks[i].description);
            supervisor_puts("\n");
            attacks[i].run();
            return;
        }
    }

    supervisor_puts("[attack] unknown attack, available:\n");
    for (unsigned i = 0; i < ATTACK_COUNT; i++) {
        supervisor_puts("  ");
        supervisor_puts(attacks[i].name);
        supervisor_puts("\n");
    }
}
