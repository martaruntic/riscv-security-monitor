// Code that runs in S-mode. This plays the role of an untrusted kernel:
// it is linked separately from the monitor (see linker.ld) and it can only
// talk to the monitor through ecall.
//
// Built normally it runs a short demo. Built with ATTACK=<name> it runs
// one attack from supervisor_attacks.c instead.

#include <stdint.h>
#include "mcall.h"
#include "platform.h"
#include "secure_storage.h"
#include "supervisor.h"

struct mcall_result monitor_call(long call_number, uint64_t first_argument,
                                 uint64_t second_argument)
{
    register uint64_t a0 __asm__("a0") = first_argument;
    register uint64_t a1 __asm__("a1") = second_argument;
    register long a7 __asm__("a7") = call_number;
    struct mcall_result result;

    __asm__ volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a7) : "memory");

    result.status = (long)a0;
    result.value = a1;
    return result;
}

void supervisor_putc(char c)
{
    monitor_call(MCALL_PUTCHAR, c, 0);
}

void supervisor_puts(const char *str)
{
    while (*str) {
        if (*str == '\n')
            supervisor_putc('\r');
        supervisor_putc(*str++);
    }
}

void supervisor_put_hex(uint64_t value)
{
    const char *digits = "0123456789abcdef";

    supervisor_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        supervisor_putc(digits[(value >> shift) & 0xf]);
}

const char *status_name(long status)
{
    switch (status) {
    case MCALL_OK:               return "ok";
    case MCALL_UNKNOWN_CALL:     return "unknown call";
    case MCALL_DENIED:           return "denied";
    case MCALL_INVALID_ARGUMENT: return "invalid argument";
    case MCALL_LOCKED_OUT:       return "locked out";
    default:                     return "?";
    }
}

void supervisor_shutdown(uint32_t exit_code)
{
    monitor_call(MCALL_SHUTDOWN, exit_code, 0);
    while (1)
        ;
}

static void print_status(const char *what, struct mcall_result result)
{
    supervisor_puts("[supervisor] ");
    supervisor_puts(what);
    supervisor_puts(" -> ");
    supervisor_puts(status_name(result.status));
    supervisor_puts("\n");
}

static void print_result(const char *what, struct mcall_result result)
{
    supervisor_puts("[supervisor] ");
    supervisor_puts(what);
    supervisor_puts(" -> ");
    supervisor_puts(status_name(result.status));
    if (result.status == MCALL_OK) {
        supervisor_puts(", value ");
        supervisor_put_hex(result.value);
    }
    supervisor_puts("\n");
}

static void use_secure_storage(void)
{
    print_result("read version slot",
                 monitor_call(MCALL_SECURE_READ, SLOT_MONITOR_VERSION, 0));

    print_status("write 42 to data slot",
                 monitor_call(MCALL_SECURE_WRITE, SLOT_SUPERVISOR_DATA, 42));
    print_result("read data slot",
                 monitor_call(MCALL_SECURE_READ, SLOT_SUPERVISOR_DATA, 0));

    print_status("write to version slot",
                 monitor_call(MCALL_SECURE_WRITE, SLOT_MONITOR_VERSION, 7));
    print_result("read secret slot",
                 monitor_call(MCALL_SECURE_READ, SLOT_SECRET, 0));
    print_result("read slot 1000",
                 monitor_call(MCALL_SECURE_READ, 1000, 0));
}

static void guess_secret(void)
{
    for (int attempt = 1; attempt <= MAX_VERIFY_ATTEMPTS + 1; attempt++) {
        struct mcall_result result = monitor_call(MCALL_SECRET_VERIFY, attempt, 0);

        supervisor_puts("[supervisor] guess secret, attempt ");
        supervisor_putc('0' + attempt);
        supervisor_puts(" -> ");
        if (result.status != MCALL_OK)
            supervisor_puts(status_name(result.status));
        else
            supervisor_puts(result.value ? "match" : "wrong");
        supervisor_puts("\n");
    }
}

// The monitor refused to give the secret, so try to read it directly.
// PMP should stop this load before it returns anything.
static void read_secure_memory_directly(void)
{
    volatile uint64_t *secure_memory = (volatile uint64_t *)SECURE_REGION_BASE;
    uint64_t stolen;

    supervisor_puts("[supervisor] reading secure memory directly at ");
    supervisor_put_hex(SECURE_REGION_BASE);
    supervisor_puts("\n");

    stolen = secure_memory[SLOT_SECRET];

    // only reached if the isolation is broken
    supervisor_puts("[supervisor] got the secret: ");
    supervisor_put_hex(stolen);
    supervisor_puts("\n");
}

static void run_demo(void)
{
    use_secure_storage();
    guess_secret();
    read_secure_memory_directly();

    supervisor_puts("[supervisor] done, asking monitor to shut down\n");
    supervisor_shutdown(EXIT_OK);
}

void supervisor_main(void)
{
    supervisor_puts("[supervisor] hello from S-mode\n");

#ifdef ATTACK_NAME
    run_attack(ATTACK_NAME);
    supervisor_shutdown(EXIT_UNKNOWN_ATTACK);
#else
    run_demo();
#endif
}
