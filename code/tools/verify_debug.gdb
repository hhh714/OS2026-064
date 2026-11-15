# These assertions check live kernel state after each presentation step.
demo-pages
python
assert int(gdb.parse_and_eval("$demo_page->ref")) == 1
assert int(gdb.parse_and_eval("$demo_l0[0] & 1")) == 1
assert int(gdb.parse_and_eval("$demo_l0[0] & 6")) == 6
print("[Debug pages] PASS")
end

demo-procs
python
assert int(gdb.parse_and_eval("current->pid")) == 0
assert int(gdb.parse_and_eval("initproc->pid")) == 1
assert int(gdb.parse_and_eval("initproc->state")) == 2
assert int(gdb.parse_and_eval("initproc->parent == idleproc")) == 1
assert int(gdb.parse_and_eval("initproc->pgdir == idleproc->pgdir")) == 1
assert int(gdb.parse_and_eval("initproc->kstack != idleproc->kstack")) == 1
assert int(gdb.parse_and_eval("initproc->context.ra == (uintptr_t)forkret")) == 1
assert int(gdb.parse_and_eval("initproc->tf->epc == (uintptr_t)kernel_thread_entry")) == 1
print("[Debug processes] PASS")
end

demo-switch
python
assert int(gdb.parse_and_eval("$demo_interrupts")) == 0
assert int(gdb.parse_and_eval("$demo_satp_mode")) == 8
assert int(gdb.parse_and_eval("$demo_saved_sp == $demo_old_sp")) == 1
assert int(gdb.parse_and_eval("$sp == $demo_to->sp")) == 1
assert int(gdb.parse_and_eval("$pc == (uintptr_t)forkret")) == 1
assert int(gdb.parse_and_eval("current->pid")) == 1
print("[Debug switch] PASS")
end

demo-entry
python
assert int(gdb.parse_and_eval("current->pid")) == 1
assert int(gdb.parse_and_eval("$demo_fn == (uintptr_t)init_main")) == 1
assert gdb.parse_and_eval("(char *)$demo_arg").string() == "Hello world!!"
assert int(gdb.parse_and_eval("$demo_entry_sie")) == 2
assert int(gdb.parse_and_eval("error_code")) == 0
print("[Debug entry] PASS")
end
detach
quit
