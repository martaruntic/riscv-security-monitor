OUTPUT_ARCH("riscv")
ENTRY(_start)

/*
 * Memory map (QEMU virt, -bios none):
 *
 *   0x80000000  monitor (M-mode): code, data, monitor stack, trap stack
 *   0x80200000  supervisor (S-mode): code, data, supervisor stack
 *
 * Everything from a file whose name starts with "supervisor" goes into the
 * supervisor region, everything else belongs to the monitor.
 */
MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    /* ---------------- monitor ---------------- */
    . = ORIGIN(RAM);
    __monitor_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(EXCLUDE_FILE(*supervisor*.o) .text EXCLUDE_FILE(*supervisor*.o) .text.*)
    } > RAM

    .rodata : ALIGN(16) {
        *(EXCLUDE_FILE(*supervisor*.o) .rodata EXCLUDE_FILE(*supervisor*.o) .rodata.*)
        *(EXCLUDE_FILE(*supervisor*.o) .srodata EXCLUDE_FILE(*supervisor*.o) .srodata.*)
    } > RAM

    .data : ALIGN(16) {
        *(EXCLUDE_FILE(*supervisor*.o) .data EXCLUDE_FILE(*supervisor*.o) .data.*)
        *(EXCLUDE_FILE(*supervisor*.o) .sdata EXCLUDE_FILE(*supervisor*.o) .sdata.*)
    } > RAM

    .bss : ALIGN(16) {
        __bss_start = .;
        *(EXCLUDE_FILE(*supervisor*.o) .bss EXCLUDE_FILE(*supervisor*.o) .bss.*)
        *(EXCLUDE_FILE(*supervisor*.o) .sbss EXCLUDE_FILE(*supervisor*.o) .sbss.*)
        *(EXCLUDE_FILE(*supervisor*.o) COMMON)
        __bss_end = .;
    } > RAM

    /* Monitor stack, 16 KiB, grows down from __monitor_stack_top */
    . = ALIGN(16);
    __monitor_stack_bottom = .;
    . += 0x4000;
    __monitor_stack_top = .;

    /* Separate stack used only while handling a trap, 4 KiB */
    __trap_stack_bottom = .;
    . += 0x1000;
    __trap_stack_top = .;

    . = ALIGN(4096);
    __monitor_end = .;

    /* ---------------- supervisor ---------------- */
    . = 0x80200000;
    __supervisor_start = .;

    .supervisor 0x80200000 : {
        KEEP(*(.supervisor.text.entry))
        *supervisor*.o(.text .text.*)
        . = ALIGN(16);
        *supervisor*.o(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(16);
        *supervisor*.o(.data .data.* .sdata .sdata.*)
        . = ALIGN(16);
        *supervisor*.o(.bss .bss.* .sbss .sbss.* COMMON)
    }

    /* Supervisor stack, 8 KiB */
    . = ALIGN(16);
    __supervisor_stack_bottom = .;
    . += 0x2000;
    __supervisor_stack_top = .;

    . = ALIGN(4096);
    __supervisor_end = .;
}
