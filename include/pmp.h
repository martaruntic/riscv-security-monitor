#ifndef PMP_H
#define PMP_H

#include <stdint.h>

// The spec allows up to 64 PMP entries, the hardware decides how many exist
#define PMP_MAX_ENTRIES 64

// bits of one pmpNcfg byte
#define PMP_R      (1 << 0)
#define PMP_W      (1 << 1)
#define PMP_X      (1 << 2)
#define PMP_A_OFF   (0 << 3)
#define PMP_A_TOR   (1 << 3)   // top of range, lower bound is the previous entry
#define PMP_A_NA4   (2 << 3)   // naturally aligned 4 bytes
#define PMP_A_NAPOT (3 << 3)   // naturally aligned power of two
#define PMP_L      (1 << 7)    // locked, also applies the rule to M-mode

struct pmp_info {
    int entry_count;            // how many entries the hart really has
    uint64_t granularity;       // smallest region size in bytes
};

uint64_t pmp_read_addr(int index);
void pmp_write_addr(int index, uint64_t value);
uint8_t pmp_read_cfg(int index);
void pmp_write_cfg(int index, uint8_t cfg);

void pmp_probe(struct pmp_info *info);
void pmp_print_entries(int entry_count);

#endif
