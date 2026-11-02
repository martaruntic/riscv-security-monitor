#!/usr/bin/env bash
#
# Builds the monitor once per attack, runs it in QEMU and checks that the
# attack ended the way it should.
#
# Each line below: attack name, expected QEMU exit code, text that must
# show up in the log.
#   0  rejected by a monitor call check
#   3  stopped by PMP (access violation)
#   4  stopped as an illegal instruction (privilege violation)
#
# Usage: ./tests/run_attacks.sh            run all attacks
#        ./tests/run_attacks.sh read_secure_region   run only this one

cd "$(dirname "$0")/.." || exit 1

QEMU_TIMEOUT=10

ATTACKS=(
    "legitimate_calls        0  allowed calls work"
    "read_monitor_code       3  type:    load"
    "write_trap_stack        3  type:    store"
    "overwrite_trap_vector   3  type:    store"
    "jump_into_monitor       3  type:    instruction fetch"
    "read_secure_region      3  type:    load"
    "write_secure_region     3  type:    store"
    "execute_secure_region   3  type:    instruction fetch"
    "write_uart_directly     3  type:    store"
    "poweroff_directly       3  type:    store"
    "read_unmapped_ram       3  type:    load"
    "alias_through_paging    3  type:    load"
    "write_pmp_config        4  PRIVILEGE VIOLATION"
    "write_mtvec             4  PRIVILEGE VIOLATION"
    "read_mscratch           4  PRIVILEGE VIOLATION"
    "mret_from_supervisor    4  PRIVILEGE VIOLATION"
    "brute_force_secret      0  blocked"
    "bad_slot_index          0  blocked"
    "unknown_call_numbers    0  blocked"
    "trap_uses_caller_stack  0  blocked"
)

ONLY_ATTACK="$1"
passed=0
failed=0

printf "%-24s %-9s %-6s %s\n" "attack" "expected" "got" "result"
printf "%-24s %-9s %-6s %s\n" "------" "--------" "---" "------"

for line in "${ATTACKS[@]}"; do
    read -r name expected_code expected_text <<< "$line"

    if [ -n "$ONLY_ATTACK" ] && [ "$ONLY_ATTACK" != "$name" ]; then
        continue
    fi

    build_dir="build/attack-$name"
    log_file="$build_dir/qemu.log"

    if ! make -s ATTACK="$name" > /dev/null 2>&1; then
        printf "%-24s %-9s %-6s %s\n" "$name" "$expected_code" "-" "FAIL (build error)"
        failed=$((failed + 1))
        continue
    fi

    timeout "$QEMU_TIMEOUT" qemu-system-riscv64 -machine virt -cpu rv64 -smp 1 \
        -m 128M -nographic -bios none -kernel "$build_dir/monitor.elf" \
        > "$log_file" 2>&1 < /dev/null
    exit_code=$?

    if [ "$exit_code" -eq 124 ]; then
        result="FAIL (timeout)"
    elif [ "$exit_code" -ne "$expected_code" ]; then
        result="FAIL (wrong exit code)"
    elif ! grep -q "$expected_text" "$log_file"; then
        result="FAIL (missing \"$expected_text\")"
    else
        result="ok"
    fi

    printf "%-24s %-9s %-6s %s\n" "$name" "$expected_code" "$exit_code" "$result"

    if [ "$result" = "ok" ]; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

echo
echo "passed: $passed, failed: $failed"
echo "logs are in build/attack-<name>/qemu.log"

[ "$failed" -eq 0 ]
