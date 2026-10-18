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
- [x] Trap handling in the monitor (separate trap stack, full register save)
- [x] PMP probing (entry count and granularity)
- [x] Dropping to S-mode, monitor calls through `ecall`
- [ ] PMP rules that protect the monitor and a secure region
- [ ] Access-control rules
- [ ] Attack tests (illegal reads, writes, executes)
- [ ] Evaluation and limitations

## Design

```
            +-----------------------------+
 M-mode     |  monitor                    |  0x80000000
            |  trap handler, PMP setup,   |
            |  monitor calls              |
            +-------------^---------------+
                          | ecall / traps
            +-------------+---------------+
 S-mode     |  supervisor (untrusted)     |  0x80200000
            |  own code, data and stack   |
            +-----------------------------+
```

The monitor is the only code that runs in M-mode. The supervisor plays the
role of an untrusted kernel: it is linked into its own memory region and can
reach the monitor only through `ecall`. Every trap goes to the monitor
(nothing is delegated with `medeleg`/`mideleg` yet).

Traps are handled on a separate trap stack. `mscratch` holds its top, and on
entry it is swapped with `sp`, so the handler never trusts the stack of the
code that trapped.

### Monitor calls

| a7 | name             | a0            | result                      |
|----|------------------|---------------|-----------------------------|
| 1  | `MCALL_PUTCHAR`  | character     | 0                           |
| 2  | `MCALL_SHUTDOWN` | exit code     | does not return             |
| *  | anything else    |               | -1 (`MCALL_UNKNOWN_CALL`)   |

## Platform findings (QEMU 8.2, `virt`, `-cpu rv64`)

Measured by the monitor at boot:

- 16 PMP entries (the spec allows up to 64)
- PMP granularity is 4 bytes
- S-mode and U-mode are both implemented

Once PMP is implemented, S-mode and U-mode are denied any access that no
entry allows. Without a single PMP entry the switch to S-mode fails right
away with an instruction access fault (`mcause = 1`). QEMU reports it at the
`mret` instruction itself and not at the supervisor entry address.

For now the last entry (`pmp15`) opens the whole address space as NAPOT
with `rwx`. The lowest matching entry wins, so protected regions will be
placed in the lower entries.

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
make run    # boots it in QEMU, it powers off by itself when the supervisor is done
make debug  # starts QEMU halted, waiting for gdb on :1234
make dump   # writes the disassembly to build/monitor.asm
```

If something hangs, exit QEMU with Ctrl-A then X.

## Layout

```
include/                 headers
src/start.S              entry point, stack and .bss setup
src/main.c               monitor startup, PMP setup, switch to S-mode
src/trap_entry.S         trap entry and exit, register save/restore
src/trap.c               trap handler and monitor calls
src/pmp.c                PMP CSR access and probing
src/uart.c               NS16550A UART driver
src/supervisor_entry.S   S-mode entry point
src/supervisor.c         untrusted S-mode code
linker.ld                memory layout of the monitor and the supervisor
```
