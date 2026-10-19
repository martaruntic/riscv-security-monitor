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
- [x] PMP rules that protect the monitor and a secure region (default deny)
- [x] Secure storage with per-slot access policy
- [ ] Attack tests (illegal reads, writes, executes)
- [ ] Evaluation and limitations

## Design

```
            +-----------------------------+
 M-mode     |  monitor                    |  0x80000000
            |  trap handler, PMP setup,   |
            |  monitor calls              |
            +-----------------------------+
            |  secure region              |  0x80100000
            |  secure storage             |
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
(nothing is delegated with `medeleg`/`mideleg`).

Traps are handled on a separate trap stack. `mscratch` holds its top, and on
entry it is swapped with `sp`, so the handler never trusts the stack of the
code that trapped.

### Memory rules

The supervisor gets access only to what a rule explicitly allows:

| region       | range                     | supervisor access |
|--------------|---------------------------|-------------------|
| monitor      | `0x80000000 - 0x80007000` | none              |
| secure       | `0x80100000 - 0x80101000` | none              |
| supervisor   | `0x80200000 - 0x80203000` | `rwx`             |
| anything else (UART, other RAM, devices) | | none          |

The rules live in a table in `memory_rules.c` and the monitor turns them into
PMP entries at boot. A region that is a power of two and aligned to its size
uses one NAPOT entry, anything else uses a pair of entries in TOR mode:

```
  pmp1:  TOR   0x80000000 - 0x80007000 ---
  pmp2:  NAPOT 0x80100000 - 0x80101000 ---
  pmp4:  TOR   0x80200000 - 0x80203000 rwx
```

Deny rules are placed before allow rules. The lowest matching PMP entry wins,
so a deny keeps working even if a wider allow rule is added later.

The entries are not locked (`L` bit), so M-mode is not restricted by them and
the monitor can still use the secure region.

### Secure storage

A few 64-bit slots in the secure region. PMP stops the supervisor from
touching them directly, so the only way in is through monitor calls, and the
monitor checks a policy for each slot:

| slot | name              | supervisor                        |
|------|-------------------|-----------------------------------|
| 0    | monitor version   | read                              |
| 1    | supervisor data   | read, write                       |
| 2    | secret            | none, can only check a guess      |
| 3    | reserved          | none                              |

A guess for the secret can be checked, but the secret itself is never
returned. After 3 wrong guesses the check is locked until reboot, so the
secret can't be found by trying values one by one.

Every argument that comes from the supervisor (slot number, guess) is checked
before use. Slot numbers are unsigned, so a "negative" slot is just a very
large number and fails the same range check.

### Monitor calls

`a7` = call number, `a0`/`a1` = arguments. On return `a0` is the status and
`a1` the result value, similar to the SBI calling convention.

| a7 | call                  | arguments            | a1 on return        |
|----|-----------------------|----------------------|---------------------|
| 1  | `MCALL_PUTCHAR`       | a0 = character       |                     |
| 2  | `MCALL_SHUTDOWN`      | a0 = exit code       | does not return     |
| 3  | `MCALL_SECURE_READ`   | a0 = slot            | slot value          |
| 4  | `MCALL_SECURE_WRITE`  | a0 = slot, a1 = value|                     |
| 5  | `MCALL_SECRET_VERIFY` | a0 = guess           | 1 if it matches     |

Status codes: `0` ok, `-1` unknown call, `-2` denied, `-3` invalid argument,
`-4` locked out.

### Access violations

If the supervisor touches memory that PMP doesn't allow, the hart raises an
access fault and the monitor gets it. The monitor does not try to recover the
supervisor. It logs the access type, the address (from `mtval`), which region
the address belongs to, the pc, and stops the machine with QEMU exit code 3.

```
[supervisor] reading secure memory directly at 0x0000000080100000
[monitor] ACCESS VIOLATION from S-mode
          type:    load
          address: 0x0000000080100010 (secure)
          pc:      0x0000000080200336
[monitor] supervisor stopped
qemu exit code: 3
```

## Platform findings (QEMU 8.2, `virt`, `-cpu rv64`)

Measured by the monitor at boot:

- 16 PMP entries (the spec allows up to 64)
- PMP granularity is 4 bytes
- S-mode and U-mode are both implemented

Once PMP is implemented, S-mode and U-mode are denied any access that no
entry allows. Without a single PMP entry the switch to S-mode fails right
away with an instruction access fault (`mcause = 1`). QEMU reports it at the
`mret` instruction itself and not at the supervisor entry address.

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
make run    # boots it in QEMU and prints the exit code at the end
make debug  # starts QEMU halted, waiting for gdb on :1234
make dump   # writes the disassembly to build/monitor.asm
```

QEMU exit codes: `0` the supervisor shut down normally, `3` the monitor
stopped it because of an access violation. If something hangs, exit QEMU with
Ctrl-A then X.

## Layout

```
include/                 headers
src/start.S              entry point, stack and .bss setup
src/main.c               monitor startup, switch to S-mode
src/trap_entry.S         trap entry and exit, register save/restore
src/trap.c               trap handler, monitor calls, access violations
src/pmp.c                PMP CSR access, probing, region encoding
src/memory_rules.c       memory rules and turning them into PMP entries
src/secure_storage.c     secure storage and its access policy
src/uart.c               NS16550A UART driver
src/supervisor_entry.S   S-mode entry point
src/supervisor.c         untrusted S-mode code
linker.ld                memory layout of the monitor, secure region and supervisor
```
