#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

// LAB4 功能三：沿进程链表循环寻找下一个可运行线程。
void schedule(void) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        // idle 不在 proc_list 内；其他线程从自己的下一个节点开始扫描。
        list_entry_t *last = current == idleproc ? &proc_list : &current->list_link;
        list_entry_t *le = last;
        struct proc_struct *next = NULL;
        do {
            le = list_next(le);
            if (le != &proc_list) {
                // 链表头不是 PCB，必须跳过；只选择 PROC_RUNNABLE 的节点。
                struct proc_struct *proc = le2proc(le, list_link);
                if (proc->state == PROC_RUNNABLE) {
                    next = proc;
                    break;
                }
            }
        } while (le != last);

        if (next == NULL) {
            // 没有普通线程可运行时，由 idle 接管 CPU。
            next = idleproc;
        }
        next->runs++;
        // 选中的仍是当前线程时无需做寄存器切换。
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}


