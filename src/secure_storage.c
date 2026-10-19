#include "secure_storage.h"
#include "mcall.h"

#define MONITOR_VERSION 1

// Fixed for the demo. A real monitor would get this from hardware
// (fuses, a key derived at boot), never from its own binary.
#define DEMO_SECRET 0x3c5a7e19d24b8f61UL

#define SLOT_READ  (1 << 0)
#define SLOT_WRITE (1 << 1)

struct secure_storage {
    uint64_t slots[SECURE_SLOT_COUNT];
    uint64_t failed_verify_attempts;
};

// Placed in the .secure section, which the linker puts into the secure
// region. That region has no PMP permissions for the supervisor.
static struct secure_storage storage __attribute__((section(".secure")));

// What the supervisor may do with each slot through monitor calls
static const uint8_t slot_policy[SECURE_SLOT_COUNT] = {
    [SLOT_MONITOR_VERSION] = SLOT_READ,
    [SLOT_SUPERVISOR_DATA] = SLOT_READ | SLOT_WRITE,
    [SLOT_SECRET]          = 0,
    [SLOT_RESERVED]        = 0,
};

void secure_storage_init(void)
{
    // .secure is NOLOAD, so its content is whatever was in RAM. Clear it.
    for (int i = 0; i < SECURE_SLOT_COUNT; i++)
        storage.slots[i] = 0;
    storage.failed_verify_attempts = 0;

    storage.slots[SLOT_MONITOR_VERSION] = MONITOR_VERSION;
    storage.slots[SLOT_SECRET] = DEMO_SECRET;
}

// slot comes straight from a supervisor register, so it is checked before
// it is used as an index. It's unsigned, so "negative" values are huge
// and fail the same check.
static int slot_is_valid(uint64_t slot)
{
    return slot < SECURE_SLOT_COUNT;
}

long secure_storage_read(uint64_t slot, uint64_t *value)
{
    *value = 0;
    if (!slot_is_valid(slot))
        return MCALL_INVALID_ARGUMENT;
    if (!(slot_policy[slot] & SLOT_READ))
        return MCALL_DENIED;

    *value = storage.slots[slot];
    return MCALL_OK;
}

long secure_storage_write(uint64_t slot, uint64_t value)
{
    if (!slot_is_valid(slot))
        return MCALL_INVALID_ARGUMENT;
    if (!(slot_policy[slot] & SLOT_WRITE))
        return MCALL_DENIED;

    storage.slots[slot] = value;
    return MCALL_OK;
}

// The supervisor can check if it knows the secret, but never read it.
// After MAX_VERIFY_ATTEMPTS wrong guesses the slot is locked until reboot,
// otherwise the secret could be found by guessing.
long secure_storage_verify_secret(uint64_t guess, uint64_t *matches)
{
    *matches = 0;
    if (storage.failed_verify_attempts >= MAX_VERIFY_ATTEMPTS)
        return MCALL_LOCKED_OUT;

    if (guess == storage.slots[SLOT_SECRET]) {
        *matches = 1;
        storage.failed_verify_attempts = 0;
    } else {
        storage.failed_verify_attempts++;
    }
    return MCALL_OK;
}
