# Lab4 实现核对与 Challenge 解答

## 一、实验范围与实现核对

本实验采用预映射方式建立内核地址空间，引入内核线程、调度器和上下文切换。内核线程共用页表，各自拥有 PCB、内核栈和运行上下文。用户进程、系统调用、按需缺页分配和页面置换属于后续实验内容。

| 要求 | 对应实现 | 核对结果 |
| --- | --- | --- |
| 虚拟地址空间描述 | `mm_struct`、`vma_struct`，以及创建、插入、查询和销毁接口 | 框架已有；启动时通过 `check_vma_struct` 自检 |
| 虚拟内存页操作 | `page_insert`、`page_remove` | 已实现；维护引用计数、页表项及 TLB，重复删除可安全返回 |
| 进程创建与初始化 | `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm` | 已实现；建立 PID 0 的 idle 与 PID 1 的 init |
| 进程调度 | `cpu_idle`、`schedule`、`proc_run`、`switch_to` | 已实现；在临界区选择线程并切换，缺少可运行线程时选择 idle |
| Lab2/Lab3 前置代码 | 物理页分配与初始化、中断入口及现场恢复、时钟中断处理 | 已补齐并通过启动及评分检查 |

`copy_mm` 保留接口并检查当前线程没有用户地址空间，本阶段无需复制用户页表。`alloc_proc` 将 `mm` 初始化为 `NULL`，将 `pgdir` 初始化为内核启动页表的物理地址。`schedule` 支持内核线程主动让出 CPU 后轮流运行；当前实现没有增加时钟驱动的抢占策略。

首次执行的主要调用关系如下：

```text
proc_init → kernel_thread → do_fork → copy_thread

cpu_idle → schedule → proc_run → switch_to
         → forkret → forkrets → __trapret
         → sret → kernel_thread_entry → init_main
```

`context.ra` 使 `switch_to` 首次返回到 `forkret`；trapframe 中的 `epc`、`status` 和通用寄存器再决定 `sret` 后进入的线程函数、运行权限及参数。

## 二、Challenge 1：保存和恢复中断状态

相关代码位于 `code/kern/sync/sync.h` 和 `code/kern/driver/intr.c`。

```c
local_intr_save(intr_flag);
/* 临界区 */
local_intr_restore(intr_flag);
```

### 1. 实际执行过程

`local_intr_save(intr_flag)` 调用 `__intr_save()`，并将返回值赋给 `intr_flag`。它读取 `sstatus` 的 `SIE` 位：原来开启中断时，调用 `intr_disable()` 清除该位，返回 1；原来已经关闭时，直接返回 0。

`local_intr_restore(intr_flag)` 调用 `__intr_restore()`：只有保存的标志为 1，才调用 `intr_enable()` 设置 `SIE`；标志为 0 时不执行开中断操作。这里保存的是原来的中断开关状态，不是整个 `sstatus` 或整套寄存器现场。临界区内应遵守同一套保存/恢复约定，不能私自提前开中断。

| 进入前的 SIE | intr_flag | 临界区内的 SIE | 正常配对退出后的 SIE |
| --- | --- | --- | --- |
| 1 | 1 | 0 | 1 |
| 0 | 0 | 0 | 0 |

`intr_disable()` 使用 `clear_csr(sstatus, SSTATUS_SIE)`，`intr_enable()` 使用 `set_csr(sstatus, SSTATUS_SIE)`。框架分别用 `CSRRC`、`CSRRS` 实现按掩码清位和置位；这些指令对单个 CSR 执行原子的读改写，编译器也可能选用相应立即数形式。`__intr_save()` 本身的读状态与随后清位是两步，不能称为“一条原子指令”。参见 [RISC-V Zicsr 规范](https://docs.riscv.org/reference/isa/v20260120/unpriv/zicsr.html)。

### 2. 为什么要恢复旧状态

如果退出时总是直接开中断，就会破坏嵌套临界区。当前 `schedule()` 调用 `proc_run()` 就存在这种嵌套：

```text
原先开中断
  外层 schedule 保存 1，并关中断
    内层 proc_run 保存 0，继续保持关中断
    内层恢复 0，不开中断
  外层恢复 1，才重新开中断
```

每层使用自己的局部标志变量。发生线程切换时，该层调用的标志随自己的执行现场保留，等这个线程继续执行时再完成恢复。新线程首次运行则经由已准备好的 trapframe 和 `sret` 恢复状态。

这保护 `current` 更新、`satp` 更新和寄存器切换过程。它只控制当前 hart 的 S 模式中断，不会屏蔽同步异常，也不能代替多核共享数据需要的锁。`SIE` 与 `SPIE` 不同：前者控制当前开关，后者参与陷入与 `sret` 时的恢复。参见 [RISC-V 特权规范的 sstatus 说明](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html)。

## 三、Challenge 2：分页模式与 get_pte

### 1. Sv32、Sv39、Sv48 的区别

以下比较普通 4 KiB 页面。每张页表也占一页，表项大小和级数决定索引的划分。

| 模式 | 运行环境 | 页表级数 | 每项大小 | 每表项数 | 虚拟地址有效位的划分 |
| --- | --- | --- | --- | --- | --- |
| Sv32 | RV32 | 2 | 4 B | 1024 | 10 + 10 + 12 |
| Sv39 | RV64 | 3 | 8 B | 512 | 9 + 9 + 9 + 12 |
| Sv48 | RV64 | 4 | 8 B | 512 | 9 + 9 + 9 + 9 + 12 |

最后 12 位是页内偏移。Sv39 的 64 位地址中，位 63–39 必须复制位 38；Sv48 中位 63–48 必须复制位 47。级数增加可表达更大的虚拟地址空间，也增加页表遍历的成本。规格依据为 [RISC-V 特权规范的 Sv32、Sv39、Sv48 小节](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html)。

索引位数可以由表项数计算：`4096 / 4 = 1024 = 2^10`，所以 Sv32 每级取 10 位；`4096 / 8 = 512 = 2^9`，所以 Sv39 和 Sv48 每级取 9 位。

### 2. get_pte 的两段代码为什么相似

本实验 `get_pte()` 遍历的是 Sv39 的三层页表。用硬件层号说明，其路径是：

```text
la[38:30] = PDX1(la) → L2 根页表中的一个目录项
la[29:21] = PDX0(la) → L1 页表中的一个目录项
la[20:12] = PTX(la)  → L0 页表中的目标 PTE
la[11:0]            → 数据页内偏移
```

第一段检查根页表的目录项，必要时创建 L1 页表；第二段检查 L1 中的目录项，必要时创建 L0 页表。两段都执行同一种操作：

1. 按本级索引定位目录项。
2. 若无效且 `create == 0`，返回 `NULL`。
3. 若允许创建，就分配一个物理页，将其引用计数设为 1 并清零。
4. 将该物理页的页号写入父目录项。
5. 通过 `PDE_ADDR` 与 `KADDR` 找到下一级页表。

多级页表的每层具有相同结构，所以遍历过程重复。两段代码是在不同层进行同一种工作，不是在给同一个数据页分配两次空间。对于完整的普通页路径，Sv32 在到达最终 PTE 前需要一次这样的下降；Sv39 需要两次；Sv48 需要三次。可以用层号、索引宽度、目标层号参数化一个循环，但本实验不要求实现通用的多模式页表管理器。

`get_pte()` 返回的是最终 PTE 的内核虚拟地址，不等于已经建立数据页映射。即使返回非空，目标 PTE 也可能仍然无效；`page_insert()` 才负责写入数据页映射、权限和引用计数。

本实验的 `ptd_create()` 给新目录项设置 `V`，保持 `R/W/X` 为 0 以表示下级页表，且不设置非叶项保留的 `U/A/D` 位。`proc_run()` 用 RV64 的 Sv39 模式编码写入 `satp`，没有调用库中的 Sv32 版 `lsatp()`。这些限制与 [RISC-V 非叶项及 satp 定义](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html)一致。

还需要区分普通页与大页：硬件允许在较高层遇到叶项并提前结束遍历；现有 `get_pte()` 面向 4 KiB 映射路径，并不是通用的大页查询函数。启动汇编中的高地址大页映射不能简单当成下一级页表继续解引用。

### 3. 查找与分配是否应该拆开

在本实验中，保留 `get_pte(pgdir, la, create)` 是合理的。调用方可以明确选择只查找或按需创建中间页表，共享一套遍历逻辑，便于与已有接口配合：

```c
get_pte(pgdir, la, 0);  // page_remove、get_page：只查询，不分配
get_pte(pgdir, la, 1);  // page_insert：缺少中间页表时允许创建
```

这种设计的代价是副作用依赖 `create` 参数，调用者容易忽略分配失败。尤其在先创建了一层、下一层又分配失败时，必须明确如何回滚。此次核对为该路径补上了处理：仅撤销本次新建的空中间页表，恢复原目录项，执行翻译同步并归还物理页；已有页表、已有映射和待映射数据页的引用计数保持不变。

对于更复杂的系统，可以在对外接口上区分“纯查找”和“确保路径存在”，例如 `lookup_pte()` 与 `ensure_pte()`，内部仍共用一个遍历器。这样能明确只读操作与可能分配内存的操作，分别约定失败、锁和回滚行为，同时避免复制整套页表算法。拆接口本身不会自动解决并发问题，查找后再创建也仍要受正确的同步保护。

因此，本实验无需为了思考题改掉既有函数签名；明确 `create` 的语义并处理失败路径即可。支持更多分页模式、大页和并发修改时，再把接口分层会更有价值。

## 四、验证记录

2026-11-15 在 WSL Ubuntu 与 QEMU RISC-V 环境验证：

```bash
cd code
make grade
python3 tools/check_pte_oom.py
```

原有评分输出 `Total Score: 30/30`。额外测试实际耗尽物理页，覆盖没有空闲页、只够创建一层页表、重复失败、失败时保留已有映射，以及最终归还全部测试页面：

```text
[Test PMM] PASS
[Test PMM allocation failure] PASS
[Test Process Create] PASS
[Test Schedule] PASS
```

新增回归测试在修复前于 `root[0] == 0` 断言失败，修复后通过。测试只在临时副本中插入检查，没有修改原有评分测试。30/30 对应三项基础功能的机器评分；Challenge 的完成内容是本文件中的原理分析，并不另计在该分数内。
