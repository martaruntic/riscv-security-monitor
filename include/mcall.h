#ifndef MCALL_H
#define MCALL_H

// Calls the supervisor can make into the monitor with ecall.
// a7 holds the call number, a0 the argument, the result comes back in a0.

#define MCALL_PUTCHAR  1   // a0 = character to print
#define MCALL_SHUTDOWN 2   // a0 = exit code, powers off QEMU

#define MCALL_OK              0
#define MCALL_UNKNOWN_CALL   -1

#endif
