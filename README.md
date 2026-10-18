# riscv-security-monitor

A small security monitor for RISC-V that runs in machine mode and uses
Physical Memory Protection (PMP) to isolate a protected memory region from
less privileged software running on the same hart.

The goal is to explore the ideas behind trusted execution environments:
privilege separation, memory isolation and a minimal trusted computing base,
and to measure what this kind of isolation actually stops and what it does not.

## Status

Work in progress.

- [x] Bare-metal boot in M-mode on QEMU `virt`, UART output
- [ ] Trap handling in the monitor
- [ ] Dropping to a lower privilege mode
- [ ] PMP configuration and a protected region
- [ ] Access-control rules
- [ ] Attack tests (illegal reads, writes, executes)
- [ ] Evaluation and limitations

## Requirements

- `riscv64-unknown-elf-gcc` (bare-metal RISC-V toolchain)
- `qemu-system-riscv64` (tested with QEMU 8.2)

On Ubuntu/Debian:

```
sudo apt install gcc-riscv64-unknown-elf qemu-system-misc
```

## Build and run

```
make        # builds build/monitor.elf
make run    # boots it in QEMU (exit with Ctrl-A then X)
make debug  # starts QEMU halted, waiting for gdb on :1234
make dump   # writes the disassembly to build/monitor.asm
```

## Layout

```
include/   headers (CSR helpers, UART)
src/       monitor sources
linker.ld  memory layout, monitor is linked at 0x80000000
```
