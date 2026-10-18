#include "pmp.h"
#include "riscv.h"
#include "trap.h"
#include "uart.h"

// CSR numbers are part of the instruction, so they can't be picked at
// runtime. Each entry needs its own case with a fixed CSR name.
#define PMP_INDEX_LIST(X) \
    X(0)  X(1)  X(2)  X(3)  X(4)  X(5)  X(6)  X(7)  \
    X(8)  X(9)  X(10) X(11) X(12) X(13) X(14) X(15) \
    X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) \
    X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31) \
    X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39) \
    X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) \
    X(48) X(49) X(50) X(51) X(52) X(53) X(54) X(55) \
    X(56) X(57) X(58) X(59) X(60) X(61) X(62) X(63)

#define READ_ADDR_CASE(n)  case n: return csr_read(pmpaddr##n);
#define WRITE_ADDR_CASE(n) case n: csr_write(pmpaddr##n, value); break;

uint64_t pmp_read_addr(int index)
{
    switch (index) {
    PMP_INDEX_LIST(READ_ADDR_CASE)
    }
    return 0;
}

void pmp_write_addr(int index, uint64_t value)
{
    switch (index) {
    PMP_INDEX_LIST(WRITE_ADDR_CASE)
    }
}

// On RV64 one pmpcfg register holds 8 entries and only the even ones
// exist: pmpcfg0 has entries 0-7, pmpcfg2 has 8-15, and so on.
static uint64_t read_cfg_register(int register_index)
{
    switch (register_index) {
    case 0:  return csr_read(pmpcfg0);
    case 2:  return csr_read(pmpcfg2);
    case 4:  return csr_read(pmpcfg4);
    case 6:  return csr_read(pmpcfg6);
    case 8:  return csr_read(pmpcfg8);
    case 10: return csr_read(pmpcfg10);
    case 12: return csr_read(pmpcfg12);
    case 14: return csr_read(pmpcfg14);
    }
    return 0;
}

static void write_cfg_register(int register_index, uint64_t value)
{
    switch (register_index) {
    case 0:  csr_write(pmpcfg0, value);  break;
    case 2:  csr_write(pmpcfg2, value);  break;
    case 4:  csr_write(pmpcfg4, value);  break;
    case 6:  csr_write(pmpcfg6, value);  break;
    case 8:  csr_write(pmpcfg8, value);  break;
    case 10: csr_write(pmpcfg10, value); break;
    case 12: csr_write(pmpcfg12, value); break;
    case 14: csr_write(pmpcfg14, value); break;
    }
}

uint8_t pmp_read_cfg(int index)
{
    int register_index = (index / 8) * 2;
    int byte_shift = (index % 8) * 8;

    return (read_cfg_register(register_index) >> byte_shift) & 0xff;
}

void pmp_write_cfg(int index, uint8_t cfg)
{
    int register_index = (index / 8) * 2;
    int byte_shift = (index % 8) * 8;
    uint64_t value = read_cfg_register(register_index);

    value &= ~(0xffUL << byte_shift);
    value |= (uint64_t)cfg << byte_shift;
    write_cfg_register(register_index, value);
}

// An entry exists if its pmpaddr keeps a value we write into it.
// Entries that don't exist are either read-only zero or the CSR access
// itself is an illegal instruction, so the probe has to survive that.
static int entry_exists(int index)
{
    uint64_t readback;

    csr_probe_begin();
    pmp_write_addr(index, ~0UL);
    readback = pmp_read_addr(index);
    pmp_write_addr(index, 0);
    if (csr_probe_end())
        return 0;

    return readback != 0;
}

void pmp_probe(struct pmp_info *info)
{
    uint64_t readback;
    int trailing_zeros = 0;

    info->entry_count = 0;
    info->granularity = 0;

    for (int i = 0; i < PMP_MAX_ENTRIES; i++) {
        if (!entry_exists(i))
            break;
        info->entry_count++;
    }

    if (info->entry_count == 0)
        return;

    // With entry 0 turned off, writing all ones and reading it back shows
    // how many low address bits the hardware ignores. G zero bits mean
    // the smallest region is 2^(G+2) bytes.
    pmp_write_cfg(0, PMP_A_OFF);
    pmp_write_addr(0, ~0UL);
    readback = pmp_read_addr(0);
    pmp_write_addr(0, 0);

    while (trailing_zeros < 64 && ((readback >> trailing_zeros) & 1) == 0)
        trailing_zeros++;

    info->granularity = 1UL << (trailing_zeros + 2);
}

static const char *address_mode_name(uint8_t cfg)
{
    switch (cfg & (3 << 3)) {
    case PMP_A_TOR:   return "TOR  ";
    case PMP_A_NA4:   return "NA4  ";
    case PMP_A_NAPOT: return "NAPOT";
    default:          return "OFF  ";
    }
}

void pmp_print_entries(int entry_count)
{
    for (int i = 0; i < entry_count; i++) {
        uint8_t cfg = pmp_read_cfg(i);

        if ((cfg & (3 << 3)) == PMP_A_OFF)
            continue;

        uart_puts("  pmp");
        uart_put_dec(i);
        uart_puts(": ");
        uart_puts(address_mode_name(cfg));
        uart_puts(" addr=");
        uart_put_hex(pmp_read_addr(i));
        uart_puts(" ");
        uart_putc((cfg & PMP_R) ? 'r' : '-');
        uart_putc((cfg & PMP_W) ? 'w' : '-');
        uart_putc((cfg & PMP_X) ? 'x' : '-');
        if (cfg & PMP_L)
            uart_puts(" locked");
        uart_puts("\n");
    }
}
