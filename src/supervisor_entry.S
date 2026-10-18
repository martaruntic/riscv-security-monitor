# First instruction the supervisor runs after the monitor does mret.
# The supervisor has its own stack, it never touches the monitor's.

    .section .supervisor.text.entry
    .global supervisor_entry
supervisor_entry:
    la      sp, __supervisor_stack_top
    call    supervisor_main

    # supervisor_main should not return, if it does just spin here
1:
    j       1b
