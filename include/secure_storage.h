#ifndef SECURE_STORAGE_H
#define SECURE_STORAGE_H

#include <stdint.h>

// Small key-value storage that lives in the secure region.
// The supervisor can't touch the memory directly (PMP blocks it), it can
// only ask the monitor, and the monitor checks the policy of each slot.

#define SLOT_MONITOR_VERSION  0   // supervisor: read only
#define SLOT_SUPERVISOR_DATA  1   // supervisor: read and write
#define SLOT_SECRET           2   // supervisor: no access, can only verify a guess
#define SLOT_RESERVED         3   // supervisor: no access
#define SECURE_SLOT_COUNT     4

#define MAX_VERIFY_ATTEMPTS 3

void secure_storage_init(void);

// All of these return an MCALL_* status code
long secure_storage_read(uint64_t slot, uint64_t *value);
long secure_storage_write(uint64_t slot, uint64_t value);
long secure_storage_verify_secret(uint64_t guess, uint64_t *matches);

#endif
