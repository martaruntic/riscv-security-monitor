OUTPUT_ARCH("riscv")
ENTRY(_start)

/* QEMU virt loads the image at the start of DRAM when run with -bios none */
MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } > RAM

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > RAM

    .data : ALIGN(16) {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } > RAM

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Monitor stack, 16 KiB, grows down from __monitor_stack_top */
    . = ALIGN(16);
    __monitor_stack_bottom = .;
    . += 0x4000;
    __monitor_stack_top = .;

    . = ALIGN(4096);
    __monitor_end = .;
}
