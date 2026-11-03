# riscv-security-monitor

A small security monitor for RISC-V that runs in machine mode and uses
Physical Memory Protection (PMP) to keep a secure memory region and the
monitor itself out of reach of the operating system kernel, even when that
kernel is compromised.

The goal is to explore the ideas behind trusted execution environments
(privilege separation, memory isolation, a small trusted computing base) and
to measure what this kind of isolation actually stops and what it does not.

**Result on QEMU 8.2:** 20 attacks from a compromised kernel, all 20 stopped.
Monitor size: 861 lines of code. One known limitation (no timer interrupt) is
shown by a test as well.

## Status

Finished prototype.

- [x] Bare-metal boot in M-mode on QEMU `virt`, UART output
- [x] Trap handling in the monitor (separate trap stack, full register save)
- [x] PMP probing (entry count and granularity)
- [x] Dropping to S-mode, monitor calls through `ecall`
- [x] PMP rules that protect the monitor and a secure region (default deny)
- [x] Secure storage with per-slot access policy
- [x] Attack tests (20 attacks, run with `make test`)
- [x] Evaluation: threat model, limitations, TCB size, CI

## Threat model

**Trusted:** the hardware (the RISC-V hart and its PMP) and the monitor, which
is the only code in M-mode.

**Not trusted:** the supervisor, which stands in for an operating system
kernel in S-mode. It is assumed to be fully compromised: the attacker can run
any code in S-mode, knows the monitor binary and every address in it, and
controls its own page tables.

**Protected:**

- confidentiality and integrity of the secure region (the secret can't be
  read or changed)
- integrity of the monitor (its code, data, stacks and trap handling can't
  be changed or taken over)
- the PMP configuration itself (the supervisor can't turn the rules off)
- devices that only the monitor should use (UART, power off)

**Not protected:** availability (the supervisor can keep the CPU forever),
physical attacks, side channels and devices that access memory on their own
(see [Limitations](#limitations)).

## Design

```
            +-----------------------------+
 M-mode     |  monitor (trusted)          |  0x80000000
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
| supervisor   | `0x80200000 - 0x80206000` | `rwx`             |
| anything else (UART, other RAM, devices) | | none          |

The rules live in a table in `memory_rules.c` and the monitor turns them into
PMP entries at boot. A region that is a power of two and aligned to its size
uses one NAPOT entry, anything else uses a pair of entries in TOR mode:

```
  pmp1:  TOR   0x80000000 - 0x80007000 ---
  pmp2:  NAPOT 0x80100000 - 0x80101000 ---
  pmp4:  TOR   0x80200000 - 0x80206000 rwx
```

The supervisor region is computed from the linker symbols, so it grows with
the supervisor code (the attack code and its page table are linked in too).

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
returned. After 3 wrong guesses in a row the check is locked until reboot,
so the secret can't be found by trying values one by one. A right guess
resets the counter.

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

### Privilege violations

If the supervisor runs an instruction that S-mode is not allowed to run
(writing an M-mode CSR, `mret`), the hart raises an illegal instruction
exception. The monitor logs the instruction bits (QEMU puts them in `mtval`)
and stops the machine with exit code 4. Any other exception from the
supervisor stops it with exit code 5, so a broken supervisor never leaves
QEMU hanging.

## Attack tests

`src/supervisor_attacks.c` has a list of things a compromised supervisor
could try. Each attack is built into its own image and booted on its own,
because a blocked attack stops the machine:

```
make run ATTACK=read_secure_region   # run one attack
make test                            # run all of them and check the results
```

`tests/run_attacks.sh` knows the expected QEMU exit code of every attack and
a line that has to show up in the log. If an attack gets through, the
supervisor reports it itself and exits with code 1.

| attack                   | what it tries                               | stopped by          | exit |
|--------------------------|---------------------------------------------|---------------------|------|
| `read_monitor_code`      | load from monitor code                      | PMP, load fault     | 3    |
| `write_trap_stack`       | store to the monitor trap stack             | PMP, store fault    | 3    |
| `overwrite_trap_vector`  | replace the first trap handler instruction  | PMP, store fault    | 3    |
| `jump_into_monitor`      | execute monitor code in S-mode              | PMP, fetch fault    | 3    |
| `read_secure_region`     | load the secret directly                    | PMP, load fault     | 3    |
| `write_secure_region`    | overwrite the secret directly               | PMP, store fault    | 3    |
| `execute_secure_region`  | jump into the secure region                 | PMP, fetch fault    | 3    |
| `write_uart_directly`    | print without going through the monitor     | PMP, store fault    | 3    |
| `poweroff_directly`      | power off QEMU through the test device      | PMP, store fault    | 3    |
| `read_unmapped_ram`      | load RAM that no rule covers                | PMP, load fault     | 3    |
| `alias_through_paging`   | map the secret at a second virtual address  | PMP, load fault     | 3    |
| `write_pmp_config`       | `csrw pmpcfg0`                              | illegal instruction | 4    |
| `write_mtvec`            | `csrw mtvec`                                | illegal instruction | 4    |
| `read_mscratch`          | `csrr mscratch` (trap stack address)        | illegal instruction | 4    |
| `mret_from_supervisor`   | `mret` in S-mode                            | illegal instruction | 4    |
| `brute_force_secret`     | 3 wrong guesses, then the right secret      | lockout             | 0    |
| `bad_slot_index`         | slot 4, -1, 2^63, 0x1000                    | argument check      | 0    |
| `unknown_call_numbers`   | call numbers 0, 6, 1000, -1                 | argument check      | 0    |
| `trap_uses_caller_stack` | `ecall` with `sp` pointing to a canary      | separate trap stack | 0    |

Two more tests check that the protections don't get in the way of normal use:

| test                  | what it checks                                         | exit |
|-----------------------|--------------------------------------------------------|------|
| `legitimate_calls`    | allowed reads and writes of secure storage work        | 0    |
| `secret_verify_works` | the right secret is accepted, a match resets the count | 0    |

And one test shows a known limitation instead of hiding it:

| test           | what it does                              | result                     |
|----------------|-------------------------------------------|----------------------------|
| `spin_forever` | loops forever without calling the monitor | never stopped, QEMU timeout |

Output of `make test`:

```
passed: 21, failed: 0, known limitations: 1
```

To check that the tests can actually fail, the secure rule was changed to
`rwx` for one run. `read_secure_region` and `write_secure_region` then
reported success (exit 1), `execute_secure_region` ran into zeroed memory
(illegal instruction, exit 4), and `alias_through_paging` read the secret
too. The script reported 4 failures, as it should.

### Notes from testing

- **Paging does not get around PMP.** The supervisor controls `satp` and its
  own page tables. In `alias_through_paging` it maps physical RAM at a second
  virtual address and reads the secret through it. PMP checks the physical
  address after translation, so the read still faults. `mtval` holds the
  virtual address (`0xc0100010`), which is why the monitor can't name the
  region in that log. The page table walk itself is allowed, because the
  table lives in supervisor memory.
- **The first version of `overwrite_trap_vector` faulted on a load, not a
  store.** `trap_vector` was declared as `char[]`, so the compiler did not
  know it was aligned and turned the 32-bit store into byte accesses that
  start with a load. PMP stopped it either way, but the test was not testing
  what it said. Declaring it as `uint32_t[]` gives a single `sw`.
- **The trap stack design holds.** During an `ecall` with `sp` pointing to a
  buffer full of a known pattern, the buffer stays unchanged, so the monitor
  never touches the caller's stack.
- **A too short test timeout made one test flaky.** With a 5 second limit,
  the first QEMU start after `make clean` sometimes did not finish in time.
  The limit is now 15 seconds and can be changed with `QEMU_TIMEOUT`.

## Trusted computing base

Everything the security of the system depends on is the monitor. A bug
anywhere in it could break the isolation, so it is kept small. Counted with
`make loc` (no blank lines, no comment-only lines):

| part                          | lines of code |
|-------------------------------|---------------|
| monitor (trusted)             | 861           |
| supervisor (untrusted)        | 143           |
| attacks (untrusted, test only)| 272           |

The biggest monitor files are the trap handler (`trap.c`, 188) and the PMP
code (`pmp.c`, 181). About 70 lines of `pmp.c` are only there to print PMP
entries in a readable way.

## Limitations

This is a prototype, and the isolation only holds within the threat model
above. Things it does not handle:

- **One hart only.** Every hart has its own PMP registers. On a multi-core
  system the monitor would have to set up PMP on every hart before any of
  them runs supervisor code, otherwise a second core could read everything.
- **No timer interrupt.** Once the supervisor runs, it never has to give the
  CPU back to the monitor. `spin_forever` shows this. The monitor protects
  secrecy and integrity, not availability.
- **DMA.** PMP only checks accesses made by the hart. A device that can write
  to memory on its own (DMA) is not checked by it. Protecting against that
  needs an IOPMP or an IOMMU.
- **Side channels.** PMP stops the access, but not timing or cache effects.
  On a real out-of-order core, Spectre-style attacks against the monitor
  would be a separate problem.
- **PMP entries are not locked.** Without the `L` bit, a bug in the monitor
  could change or remove its own rules. Locking them would also apply them to
  M-mode, so the secure region would need its own design for how the monitor
  reaches it.
- **The secret is in the binary.** It is a constant in the source code for
  the demo. A real system would take it from hardware (fuses, a key derived
  at boot) and would need secure boot so the monitor itself can be trusted.
- **No attestation.** Nobody outside the machine can check which monitor is
  running.
- **Lockout until reboot.** The secret is safe from guessing, but a
  compromised supervisor can lock it on purpose. That is a denial of service
  against legitimate users.
- **QEMU is not real hardware.** All results are for QEMU 8.2 (`virt`,
  `-cpu rv64`). Real chips can have a different number of PMP entries,
  coarser granularity, and errata.

## Future work

- Set up PMP on every hart and test with `-smp 4`
- Timer interrupt in the monitor so it can always take the CPU back
- A small enclave in U-mode that the supervisor can't read either
- Run the same tests on real hardware (for example a VisionFive 2)

## Platform findings (QEMU 8.2, `virt`, `-cpu rv64`)

Measured by the monitor at boot:

- 16 PMP entries (the spec allows up to 64)
- PMP granularity is 4 bytes
- S-mode and U-mode are both implemented

Once PMP is implemented, S-mode and U-mode are denied any access that no
entry allows. Without a single PMP entry the switch to S-mode fails right
away with an instruction access fault (`mcause = 1`). QEMU reports it at the
`mret` instruction itself and not at the supervisor entry address.

## Tools

| tool                       | version | used for                                  |
|----------------------------|---------|-------------------------------------------|
| `riscv64-unknown-elf-gcc`  | 13.2.0  | compiling C and assembly, linking         |
| `qemu-system-riscv64`      | 8.2.2   | running the monitor (`virt` machine)      |
| GNU Make                   | 4.3     | build, run, test targets                  |
| `gdb-multiarch`            |         | debugging through QEMU's gdb stub         |
| Bash                       |         | test and line count scripts               |
| GitHub Actions             |         | running `make test` on every push         |

Developed on Ubuntu 24.04 under WSL 2 on Windows, with VS Code.

## Requirements

On Ubuntu/Debian:

```
sudo apt install gcc-riscv64-unknown-elf qemu-system-misc
```

## Build and run

```
make        # builds build/monitor.elf
make run    # boots it in QEMU and prints the exit code at the end
make test   # runs all attacks (see "Attack tests")
make loc    # lines of code of the monitor and the supervisor
make debug  # starts QEMU halted, waiting for gdb on :1234
make dump   # writes the disassembly to build/monitor.asm
```

QEMU exit codes:

| code | meaning                                                  |
|------|----------------------------------------------------------|
| 0    | the supervisor shut down normally                        |
| 1    | an attack got through                                    |
| 2    | unknown `ATTACK` name                                    |
| 3    | the monitor stopped the supervisor: access violation     |
| 4    | the monitor stopped the supervisor: privilege violation  |
| 5    | the monitor stopped the supervisor: other exception      |

If something hangs, exit QEMU with Ctrl-A then X.

## Layout

```
include/                 headers
src/start.S              entry point, stack and .bss setup
src/main.c               monitor startup, switch to S-mode
src/trap_entry.S         trap entry and exit, register save/restore
src/trap.c               trap handler, monitor calls, violations
src/pmp.c                PMP CSR access, probing, region encoding
src/memory_rules.c       memory rules and turning them into PMP entries
src/secure_storage.c     secure storage and its access policy
src/uart.c               NS16550A UART driver
src/supervisor_entry.S   S-mode entry point
src/supervisor.c         untrusted S-mode code, demo
src/supervisor_attacks.c attacks against the monitor
tests/run_attacks.sh     runs every attack and checks the result
tests/count_lines.sh     lines of code per part
linker.ld                memory layout of the monitor, secure region and supervisor
.github/workflows/       CI that runs make test
```

## License

MIT, see `LICENSE`.
