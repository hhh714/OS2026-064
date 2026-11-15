# Lab4 现场演示步骤

演示目标：证明页映射、内核线程创建、调度和上下文切换形成完整运行链，并能用调试器解释关键状态。所有 `make` 命令都在 WSL Ubuntu 的源码目录执行。

## 1. 进入正确的目录

本机打开 PowerShell，输入：

```powershell
wsl -d Ubuntu
```

进入 Ubuntu 后输入：

```bash
cd '/mnt/c/Users/LENOVO/Desktop/我的文件/大三上/操作系统/lab4/lab4_new'
```

如果使用 GitHub 下载的项目，应切换到 `lab4` 分支，并进入其中的 `code/` 目录；`Makefile` 应位于当前目录。

本机验证环境：Ubuntu、RISC-V GCC 13.2、QEMU 8.2.2、GDB 15.1。需要查看环境时执行：

```bash
riscv64-unknown-elf-gcc --version
qemu-system-riscv64 --version
riscv64-unknown-elf-gdb --version
```

2026-11-15 已在本机实测通过：干净编译、正常启动并自动退出、评分 30/30、页表耗尽检查、四步 GDB 状态检查，以及 QEMU 的手动退出。默认 GDB 和 `gdb-multiarch` 调试入口均已验证。

## 2. 干净编译与正常启动

```bash
make clean
make -j2
make qemu
```

展示以下输出即可，不必逐行解释 OpenSBI 启动信息：

- 物理内存、页表、VMA 自检成功，例如 `check_alloc_page()`、`check_pgdir()`、`check_boot_pgdir()`、`check_vma_struct()` 和 `check_vmm()` 的成功提示。
- `this initproc, pid = 1, name = "init"`。
- `To U: "Hello world!!".` 和结束问候。

可以这样说明：启动执行流被登记为 PID 0 的 idle 线程；创建出的 PID 1 的 init 线程获得独立内核栈，由调度器选中，完成上下文切换后执行 `init_main`。

随后出现 `process exit!!.` 和监控器欢迎语，是模板 `do_exit()` 中保留的 `panic`。当前模板的 `kmonitor()` 紧接着调用 `sbi_shutdown()`，所以 QEMU 会自动退出并返回终端，不会停在可输入命令的 `K>` 提示符。本实验尚未实现完整退出和资源回收，因此这个特定提示符合当前代码的预期。其他断言或异常不能据此忽略。

需要在 QEMU 尚未自动退出时手动结束它：**先按 `Ctrl-a`，松开后再按 `x`**。

## 3. 运行评分与额外检查

退出 QEMU 后执行：

```bash
make grade
```

应看到：

```text
check PMM:             OK
check Process Create:  OK
check Schedule:        OK
Total Score: 30/30
```

这三项分别验证物理页管理与映射、线程创建、线程调度。普通启动只有 idle/init；评分脚本还会注入测试线程验证调度行为。

额外验证页表创建失败时的处理：

```bash
python3 tools/check_pte_oom.py
```

该测试耗尽真实空闲物理页，检查没有空闲页、只够创建一层页表以及已有映射仍须保留等情况，并恢复测试所占物理页。应看到基础测试和页表耗尽测试的 PASS 输出。

演示前可以再自动排练一次完整的 GDB 流程：

```bash
make check-debug
```

应看到四项结果：

```text
[Debug pages] PASS
[Debug processes] PASS
[Debug switch] PASS
[Debug entry] PASS
```

这会实际启动 QEMU、连接 GDB、设置断点、单步执行并检查内核状态。日志在 `obj/debug/demo-gdb.log` 和 `obj/debug/demo-qemu.log`，测试结束后自动关闭此次启动的模拟器。

## 4. 双终端现场调试

打开两个 WSL Ubuntu 终端，都进入第 1 步的源码目录。

终端 A：

```bash
make debug
```

出现 `QEMU debug: waiting for GDB on 127.0.0.1:1234` 后暂停等待属于正常现象，`-S` 要求 CPU 等待调试器启动。

终端 B：

```bash
make gdb
```

调试器会自动连接，并在 `kern_init` 停下。调试版使用 `-O0`，产物位于 `obj/debug/` 和 `bin/debug/`，便于观察局部变量和执行顺序。首次断点使用硬件断点，以便在开启分页前设置内核高虚拟地址断点。

在 `(gdb)` 提示符后，**按顺序**执行下面四个命令。它们是 `tools/demo.gdb` 中的辅助命令，内部使用真实断点、打印和单步操作，不修改内核变量或寄存器。

### 4.1 页映射

```gdb
demo-pages
```

停在自检第一次创建可读写映射的 `page_insert`，随后执行到该函数返回。应看到：

- `la=0x100`，`perm=0x6`，即 `PTE_R | PTE_W`。
- 物理页引用计数从 `0` 变为 `1`。
- 根目录项与叶子 PTE 已建立；叶子 PTE 的低三位包含 `V/R/W`，物理地址以当次输出为准。

说明要点：Sv39 经过两级目录后定位叶子 PTE；建立、替换和撤销映射时，必须同步维护引用计数与 TLB。在同一虚拟地址重新映射同一物理页时，引用计数保持不变；映射到另一个虚拟地址会新增引用，替换映射时要撤销旧页的引用。

### 4.2 线程创建后的状态

```gdb
demo-procs
```

此时停在 `cpu_idle`，第一次调度还没发生。应看到：

- `current PID=0`，`init PID=1`，`init state=2`（`PROC_RUNNABLE`）。
- `initproc->parent == idleproc` 为真。
- 两个线程的 `pgdir` 相同，`kstack` 不同。
- init 的 `context.ra` 指向 `forkret`，`tf->epc` 指向 `kernel_thread_entry`。

需要把地址与符号对应起来时，可以补充：

```gdb
p/x initproc->context.ra
p/x (void *)forkret
p/x initproc->tf->epc
p/x (void *)kernel_thread_entry
```

说明要点：本实验的内核线程共享内核地址空间，但各有 PCB、内核栈和执行现场；`copy_mm` 暂不复制用户地址空间。

### 4.3 上下文切换与汇编单步

```gdb
demo-switch
```

该命令在 `switch_to` 停住，显示寄存器和目标 context，分别执行 14 条保存指令、14 条恢复指令，再单步执行 `ret`。应看到：

- `current PID=1`：`proc_run` 已先更新当前线程指针。
- `SIE=0`：切换临界区内关闭 S 模式中断。
- `satp.MODE=8`：使用 Sv39。
- 旧栈指针被保存，新栈指针被恢复；`ret` 后 PC 位于 `forkret`。

说明要点：`switch_to` 保存 `ra`、`sp`、`s0–s11` 共 14 个寄存器，与 `struct context` 字段布局对应。它遵守函数调用约定；完整陷阱现场由另一份 `trapframe` 保存和恢复。

如需展示原始单步命令，可查看 `tools/demo.gdb`；核心命令是 `info registers`、`x/14gx $a1`、`si 14`、`si 14`、`si`。不要在辅助命令已经执行完后重复这组单步并期待同样位置。

### 4.4 新线程首次运行

```gdb
demo-entry
```

依次到达 `kernel_thread_entry`、`init_main` 和 `do_exit`。应看到：

- `s0` 对应 `init_main`，`s1` 指向 `"Hello world!!"`。
- 线程入口的 `SIE=2`：这里输出的是 `sstatus & 2` 的原始掩码，表示 SIE 位已经开启。
- 当前 PID 为 1；线程函数返回值，即 `error_code`，为 0。
- 终端 A 同时显示 init 的输出。

解释首次运行路径：

```text
switch_to → forkret → forkrets → __trapret
          → sret → kernel_thread_entry → init_main → do_exit
```

`SPIE=1` 配合 `sret` 恢复中断；这与切换过程中的 `SIE=0` 不冲突。

演示结束时，在终端 B 输入：

```gdb
detach
quit
```

`detach` 后内核继续运行，终端 A 显示预期的退出提示并自动返回命令行；如果提前结束调试且 QEMU 仍在运行，可在终端 A 按 `Ctrl-a`、`x` 退出。四个辅助命令对应启动过程中的不同阶段，如需重新演示，应重新执行 `make debug` / `make gdb`。

## 5. Challenge 与可能的提问

完整解答见 [challenge.md](challenge.md)。现场可以配合源码说明：

| 问题 | 要点与源码 |
| --- | --- |
| 为什么保存中断状态后再恢复？ | `kern/sync/sync.h`：保存进入临界区前的 SIE，原来关闭就继续保持关闭，支持嵌套使用；不能一律开启中断。 |
| `get_pte` 为什么有两段相似代码？ | `kern/mm/pmm.c`：Sv39 有三级页表，两段分别处理根目录到中间目录、中间目录到叶表；Sv32 有两级，Sv48 有四级。 |
| 查找与创建是否应拆开？ | 当前 `create` 参数复用遍历逻辑，适合教学；较大系统可以将只读查找和确保映射结构存在的接口分开，避免查询意外分配。 |
| 所有线程有独立地址空间吗？ | 本阶段内核线程共享启动页表，各自拥有独立内核栈；VMA 管理的是逻辑范围，本实验不实现用户空间复制、按需分页或页面置换。 |
| 有抢占式调度吗？ | 当前按实验实现协作式调度；`schedule` 循环寻找 runnable 线程，没有候选时回到 idle。不要将时钟中断等同于已经实现时间片抢占。 |

关键实现中已加入中文注释，重点看 `page_insert/get_pte`、`copy_thread/do_fork/proc_run`、`schedule`、`switch_to` 和中断保存恢复函数。

## 6. 现场注意事项

- 所有编译和运行命令都使用 WSL Ubuntu，当前目录必须有本实验的 `Makefile`。
- `make grade` 和 `make clean` 会清理构建产物。先评分、后调试；不要在 GDB 会话进行中运行它们。
- 1234 端口同时只能用于一场调试。`make check-debug` 之前先退出手动启动的 QEMU；它不会替你关闭其他会话。
- 若环境只有 `gdb-multiarch`，使用 `make gdb GDB=gdb-multiarch`；评分和自动排练也可分别使用 `make grade GDB=gdb-multiarch`、`make check-debug GDB=gdb-multiarch`。
- 原框架的 `kmalloc.c` 类型转换、评分线程入口返回类型会产生编译警告；链接器的 `removing unused section` 是回收未使用段的信息。判断失败应看命令是否退出成功、评分和实际运行状态。
- 建议现场顺序为：干净编译 → 正常启动 → 30/30 评分 → 四步 GDB 调试 → Challenge 说明。额外 OOM 检查和自动调试检查可提前排练，或在有时间时展示。
