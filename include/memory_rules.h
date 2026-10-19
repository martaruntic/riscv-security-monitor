#ifndef MEMORY_RULES_H
#define MEMORY_RULES_H

#include <stdint.h>
#include "pmp.h"

// One rule says what the supervisor may do with one memory region.
// Memory that no rule covers is denied to S-mode and U-mode.
struct memory_rule {
    const char *name;
    uint64_t base;
    uint64_t size;
    uint8_t supervisor_access;   // PMP_R / PMP_W / PMP_X, 0 means no access
};

void memory_rules_init(void);
int memory_rules_apply(const struct pmp_info *info);
void memory_rules_print(void);
const char *memory_rules_region_name(uint64_t address);

#endif
