# 按 demo-pages、demo-procs、demo-switch、demo-entry 的顺序执行。
# 这里只读状态和控制断点/单步，不修改内核变量或寄存器。

define demo-pages
    # 选择自检中第一次实际可读写的映射，perm=6 即 PTE_R | PTE_W。
    thbreak page_insert if perm == 6
    continue
    set $demo_page = page
    set $demo_root = pgdir
    printf "Mapping before: la=%#lx, perm=%#x, ref=%d\n", la, perm, page->ref
    finish
    set $demo_l1 = (pte_t *)((($demo_root[0] >> 10) << 12) + va_pa_offset)
    set $demo_l0 = (pte_t *)((($demo_l1[0] >> 10) << 12) + va_pa_offset)
    printf "Mapping after: ref=%d, root PTE=%#lx, leaf PTE=%#lx\n", $demo_page->ref, $demo_root[0], $demo_l0[0]
end
document demo-pages
Stop at the first read/write page_insert and inspect its reference count and PTE.
Run this first, after make gdb stops at kern_init.
end

define demo-procs
    thbreak cpu_idle
    continue
    printf "current PID=%d, init PID=%d, init state=%d\n", current->pid, initproc->pid, initproc->state
    p initproc->parent == idleproc
    p/x idleproc->pgdir
    p/x initproc->pgdir
    p/x idleproc->kstack
    p/x initproc->kstack
    p/x initproc->context
    p/x initproc->tf->epc
    p/x initproc->tf->status
end
document demo-procs
Inspect idle/init before the first schedule: shared page table, separate stacks.
Run after demo-pages.
end

define demo-switch
    thbreak switch_to
    continue
    set $demo_from = (struct context *)$a0
    set $demo_to = (struct context *)$a1
    set $demo_old_sp = $sp
    set $demo_interrupts = $sstatus & 2
    set $demo_satp_mode = (unsigned long long)$satp >> 60
    printf "Switch: current PID=%d, SIE=%d, satp.MODE=%d\n", current->pid, $demo_interrupts, $demo_satp_mode
    info registers ra sp a0 a1
    x/14gx $a1
    # 14 条 STORE 写入旧 context，接着 14 条 LOAD 恢复新 context。
    si 14
    set $demo_saved_sp = $demo_from->sp
    printf "Saved old sp=%#lx\n", $demo_saved_sp
    si 14
    printf "Restored sp=%#lx, ra=%#lx\n", $sp, $ra
    si
    x/i $pc
end
document demo-switch
Single-step all 14 saves, all 14 restores and ret in switch_to.
Run after demo-procs; the final PC should be forkret.
end

define demo-entry
    thbreak kernel_thread_entry
    continue
    set $demo_fn = $s0
    set $demo_arg = $s1
    set $demo_entry_sie = $sstatus & 2
    printf "Thread entry: PID=%d, SIE=%d\n", current->pid, $demo_entry_sie
    info symbol $s0
    x/s $s1
    thbreak init_main
    continue
    p current->pid
    p arg
    thbreak do_exit
    continue
    printf "Thread returned: exit code=%d\n", error_code
end
document demo-entry
Follow forkret/sret into kernel_thread_entry, init_main and do_exit.
Run after demo-switch; output is also visible in the QEMU terminal.
end
