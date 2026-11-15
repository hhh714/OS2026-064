# Lab4：内核线程与调度

本实验基于 uCore / RISC-V RV64，完成页映射管理、内核线程创建、调度和上下文切换。

## 实现内容

| 部分 | 主要函数 | 文件 |
| --- | --- | --- |
| 页映射 | `page_insert`、`page_remove`、`page_remove_pte`、`pgdir_alloc_page` | `kern/mm/pmm.c` |
| 线程创建 | `alloc_proc`、`copy_mm`、`copy_thread`、`do_fork`、`kernel_thread`、`proc_init` | `kern/process/proc.c` |
| 调度与切换 | `schedule`、`proc_run`、`cpu_idle`、`switch_to` | `kern/schedule/sched.c`、`kern/process/proc.c`、`kern/process/switch.S` |

同时补齐模板要求回填的 Lab2 最佳适应物理页分配器、物理页初始化和 Lab3 中断入口、寄存器恢复、时钟中断处理。

```text
创建：proc_init → kernel_thread → do_fork
运行：cpu_idle → schedule → proc_run → switch_to
     → forkret → kernel_thread_entry → init_main
```

本阶段只支持共享内核地址空间的内核线程。`idle` 的 PID 为 0，`init` 的 PID 为 1。调度器循环选择可运行线程，`switch_to` 保存和恢复 `ra`、`sp`、`s0–s11`。

## 编译与运行

在 Linux 或 WSL 中准备 GNU Make、宿主 GCC、`riscv64-unknown-elf-` 工具链、QEMU RISC-V、Python 3、GDB 和 `bc`。以下命令在本 README 所在目录（仓库中的 `code/`）执行：

```bash
make
make qemu
make grade
```

评分脚本默认调用 `riscv64-unknown-elf-gdb`。如果环境只安装了 `gdb-multiarch`，可用：

```bash
make grade GDB=gdb-multiarch
```

调试时在一个终端执行 `make debug`，另一个终端执行 `make gdb`。退出 QEMU 控制台可按 `Ctrl-a` 后再按 `x`。

## 验证结果

2026-11-15 在 WSL Ubuntu、RISC-V GCC 13.2、QEMU 8.2.2 环境执行 `make grade`：

```text
check PMM:             OK
check Process Create:  OK
check Schedule:        OK
Total Score: 30/30
```

普通启动通过物理页、页表和 VMA 自检，随后 `init` 输出 `Hello world!!`。线程函数返回后触发 `process exit!!`，这是模板中 `do_exit` 的预留行为，不属于本阶段需要实现的完整进程退出机制。

运行兼容修正包括：使用 QEMU `-kernel` 指定启动镜像，在开启分页时通过高地址跳板继续执行，以及清除 Sv39 非叶页表项中不应设置的 `U` 位。页表自检同步检查两级目录项的合法标志；评分测试和物理分配器原有自检内容保留。

框架中的 `kmalloc.c` 整数转指针调用和评分线程入口返回类型会产生编译警告，现有评分仍通过。本次未进行内存耗尽注入或用户进程测试。
