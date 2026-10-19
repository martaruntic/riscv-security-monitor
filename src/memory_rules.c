#include "memory_rules.h"
#include "uart.h"

extern char __monitor_start[];
extern char __monitor_end[];
extern char __secure_start[];
extern char __secure_end[];
extern char __supervisor_start[];
extern char __supervisor_end[];

#define RULE_COUNT 3

// Filled in at runtime, region sizes come from linker symbols and
// those can't be subtracted in a static initializer.
static struct memory_rule rules[RULE_COUNT];

void memory_rules_init(void)
{
    // Deny rules come first. Right now nothing else would allow these
    // regions anyway, but if a wider allow rule is ever added below,
    // the lower PMP entry still wins and the deny keeps working.
    rules[0] = (struct memory_rule){
        .name = "monitor",
        .base = (uint64_t)__monitor_start,
        .size = (uint64_t)(__monitor_end - __monitor_start),
        .supervisor_access = 0,
    };
    rules[1] = (struct memory_rule){
        .name = "secure",
        .base = (uint64_t)__secure_start,
        .size = (uint64_t)(__secure_end - __secure_start),
        .supervisor_access = 0,
    };
    rules[2] = (struct memory_rule){
        .name = "supervisor",
        .base = (uint64_t)__supervisor_start,
        .size = (uint64_t)(__supervisor_end - __supervisor_start),
        .supervisor_access = PMP_R | PMP_W | PMP_X,
    };
}

int memory_rules_apply(const struct pmp_info *info)
{
    int next_entry = 0;

    pmp_clear_all(info->entry_count);

    for (int i = 0; i < RULE_COUNT; i++) {
        next_entry = pmp_add_region(info, next_entry, rules[i].base,
                                    rules[i].size, rules[i].supervisor_access);
        if (next_entry < 0) {
            uart_puts("[monitor] can't fit rule '");
            uart_puts(rules[i].name);
            uart_puts("' into PMP\n");
            pmp_clear_all(info->entry_count);
            return -1;
        }
    }

    pmp_flush();
    return 0;
}

void memory_rules_print(void)
{
    for (int i = 0; i < RULE_COUNT; i++) {
        uint8_t access = rules[i].supervisor_access;

        int name_length = 0;

        uart_puts("  ");
        uart_puts(rules[i].name);
        while (rules[i].name[name_length] != '\0')
            name_length++;
        for (int pad = name_length; pad < 12; pad++)
            uart_putc(' ');
        uart_put_hex(rules[i].base);
        uart_puts(" - ");
        uart_put_hex(rules[i].base + rules[i].size);
        uart_puts("  supervisor: ");
        if (access == 0) {
            uart_puts("none");
        } else {
            uart_putc((access & PMP_R) ? 'r' : '-');
            uart_putc((access & PMP_W) ? 'w' : '-');
            uart_putc((access & PMP_X) ? 'x' : '-');
        }
        uart_puts("\n");
    }
    uart_puts("  everything else              supervisor: none\n");
}

const char *memory_rules_region_name(uint64_t address)
{
    for (int i = 0; i < RULE_COUNT; i++) {
        if (address >= rules[i].base && address - rules[i].base < rules[i].size)
            return rules[i].name;
    }
    return "not covered by any rule";
}
