#ifndef MCALL_H
#define MCALL_H

// Calls the supervisor can make into the monitor with ecall.
// a7 = call number, a0/a1 = arguments.
// On return a0 = status (MCALL_OK or a negative error), a1 = result value.

#define MCALL_PUTCHAR        1   // a0 = character
#define MCALL_SHUTDOWN       2   // a0 = exit code, does not return
#define MCALL_SECURE_READ    3   // a0 = slot,          a1 out = value
#define MCALL_SECURE_WRITE   4   // a0 = slot, a1 = value
#define MCALL_SECRET_VERIFY  5   // a0 = guess,         a1 out = 1 if it matches

#define MCALL_OK                0
#define MCALL_UNKNOWN_CALL     -1
#define MCALL_DENIED           -2
#define MCALL_INVALID_ARGUMENT -3
#define MCALL_LOCKED_OUT       -4

#endif
