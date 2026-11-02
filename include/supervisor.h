#ifndef SUPERVISOR_H
#define SUPERVISOR_H

// Helpers for the S-mode code. Everything here runs in the supervisor and
// only talks to the monitor through ecall.

#include <stdint.h>

struct mcall_result {
    long status;
    uint64_t value;
};

struct mcall_result monitor_call(long call_number, uint64_t first_argument,
                                 uint64_t second_argument);

void supervisor_putc(char c);
void supervisor_puts(const char *str);
void supervisor_put_hex(uint64_t value);
const char *status_name(long status);
void supervisor_shutdown(uint32_t exit_code);

// Runs the attack with this name. Returns only if the name is unknown.
void run_attack(const char *name);

#endif
