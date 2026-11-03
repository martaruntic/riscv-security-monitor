#!/usr/bin/env bash
#
# Counts lines of code (no blank lines, no comment-only lines) for the
# monitor, which is the trusted computing base, and for the supervisor,
# which is not trusted.

cd "$(dirname "$0")/.." || exit 1

MONITOR_FILES="src/start.S src/main.c src/trap_entry.S src/trap.c src/pmp.c
               src/memory_rules.c src/secure_storage.c src/uart.c
               include/riscv.h include/uart.h include/trap.h include/pmp.h
               include/memory_rules.h include/secure_storage.h
               include/mcall.h include/platform.h linker.ld"

SUPERVISOR_FILES="src/supervisor_entry.S src/supervisor.c include/supervisor.h"
ATTACK_FILES="src/supervisor_attacks.c"

count_lines() {
    # shellcheck disable=SC2086
    cat $1 | grep -cvE '^\s*$|^\s*(//|#|/\*|\*)'
}

printf "%-28s %s\n" "part" "lines of code"
printf "%-28s %s\n" "----" "-------------"
printf "%-28s %s\n" "monitor (trusted)" "$(count_lines "$MONITOR_FILES")"
for file in $MONITOR_FILES; do
    printf "  %-26s %s\n" "$file" "$(count_lines "$file")"
done
printf "%-28s %s\n" "supervisor (untrusted)" "$(count_lines "$SUPERVISOR_FILES")"
printf "%-28s %s\n" "attacks (untrusted)" "$(count_lines "$ATTACK_FILES")"
