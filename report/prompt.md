# Lab4 四段式提示词汇总

以下提示词按实际任务与修复过程整理。路径相对于 `code/`。实验过程见 [实验报告](report.md)。

## 一、页表操作

### Prompt
补齐 `kern/mm/pmm.c` 的页映射建立和删除。核对中间页表创建失败时的清理，保留框架接口和评分断言。

### Rely
`get_pte`、`pte_create`、`pte2page`、物理页分配与引用计数接口、`tlb_invalidate`，以及 Sv39 各级索引和 PTE 权限定义。

### Guarantee
保留 `int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)` 和 `void page_remove(pde_t *pgdir, uintptr_t la)`。

### Specification
- 空地址建立目标映射。同页重复插入时更新权限，引用总量保持不变。异页替换时撤销旧引用并建立新引用。
- 删除只查找已有路径。空项直接返回，有效项清除映射并递减引用，最后一个引用释放物理页。
- 映射更新后同步翻译。路径分配失败返回 `-E_NO_MEM`，数据页引用和已有映射保持原状，归还本次创建的空页表。
- 验收覆盖无空闲页、只够创建一层、重复失败、已有映射保留。

## 二、内核线程创建和初始化

### Prompt
补齐 `kern/process/proc.c` 中线程创建与初始化逻辑，实现从普通函数入口到可运行线程的路径。沿用框架 PCB、栈和 TrapFrame 布局。

### Rely
`struct proc_struct`、`struct context`、`struct trapframe`、启动栈及页表、内核栈分配、PID 与链表接口、`forkret`、`forkrets` 和 `kernel_thread_entry`。

### Guarantee
实现 `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm`。保持 `kernel_thread` 的入口类型 `int (*)(void *)`。

### Specification
- PCB 初始为未初始化状态、PID 为 -1，关联共享的启动页表。启动现场包装为 PID 0 的 idle，并创建 PID 1 的 init。
- 新线程自有内核栈。准备 TrapFrame、初始栈指针和首次返回入口，使线程函数获得正确参数。
- 完成资源准备后再在临界区分配 PID、登记并唤醒。失败返回适当错误码并释放已申请资源。
- `copy_mm` 保留接口，符合内核线程共享地址空间的约定。测试检查 PID、可运行状态及线程能首次执行。

## 三、调度与上下文切换

### Prompt
补齐 idle 循环、调度选择、线程运行切换和底层汇编。使用 Sv39 的页表切换方式并保留 Lab3 Trap 返回链。

### Rely
`proc_list`、`current`、`idleproc`、`PROC_RUNNABLE`、`need_resched`、`struct context`、本地中断保护和 Sv39 的 `satp` 设置方式。

### Guarantee
实现 `cpu_idle`、`schedule`、`proc_run`、`switch_to`，保持原签名与汇编调用约定。

### Specification
- idle 在需要时触发调度。循环扫描链表寻找可运行线程，跳过哨兵，无候选时使用 idle。
- 维护重调度标记和运行计数，只有选中不同线程才切换。
- 在本地中断保护下更新当前线程、页表和上下文，退出时按保存值恢复中断状态。
- 汇编按 `struct context` 的字段顺序保存并恢复 `ra`、`sp`、`s0–s11`。验证首次进入和多个线程主动让出后的往返执行。

## 四、Challenge 分析

### Prompt
结合现有代码回答保存与恢复本地中断、分页模式异同、以及查找和分配是否需要拆分这几项问题。

### Rely
`sync.h`、`intr.c`、`get_pte()`、Sv32/Sv39/Sv48 页表结构。

### Guarantee
交付原理分析，引用现有实现作为证据。

### Specification
列出原 SIE 为 0 或 1 时的状态变化，并用嵌套临界区说明恢复旧状态的作用。比较页表级数、表项宽度、索引位数。解释 Sv39 两次下降为何相似，以及 `create` 的副作用、错误回滚和拆分接口的取舍。

## 五、构建问题修复补充提示词

### Prompt
根据实际 `make grade` 日志定位两处类型错误，修改持久源码中的对应调用点或测试入口。

### Rely
`kva2page(void *)`、`kernel_thread(int (*fn)(void *), void *, uint32_t)`、`tools/test_lab4.c` 和评分注入流程。

### Guarantee
保留分配器和线程创建接口、原有测试断言及调度测试行为。

### Specification
把整数形式的内核地址显式转换为接口要求的指针。让评分线程入口的真实函数类型与接口匹配。修改注入源文件，重新评分时检查编译、QEMU 运行和三项结果。
