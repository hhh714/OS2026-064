# Lab1 提示词汇总

本文按独立功能组织任务提示词，供逐项分析、实现和验证使用；条目编号表示任务顺序，不表示实际调用次数或迭代次数。

## P1：最小内核启动与调试整体任务

```text
[PROMPT]
任务：检查 code/ 下最小内核启动模块，完善 Makefile 的调试器可配置性；补充真实复位流程验证与 report/report.md、report/prompt.md。
操作要求：修改实际文件。先检查现有工程和工具版本，再编译、运行、调试。依据实际输出修订报告。
输出要求：保留内核行为和公开接口；报告按模板组织，测试结果根据实际观察填写。

[RELY]
真实代码：ENTRY(kern_entry)，BASE_ADDRESS=0x80200000。
entry.S：la sp, bootstacktop；tail kern_init。
memlayout.h：KSTACKPAGE=2；KSTACKSIZE=(KSTACKPAGE*PGSIZE)。
mmu.h：PGSHIFT=12；PGSIZE=4096。
init.c：extern char edata[], end[]；memset(edata,0,end-edata)；cprintf 后无限循环。
可用接口：void *memset(void *s, char c, size_t n)；int cprintf(const char *fmt, ...)。
Makefile 已有 qemu/debug，使用 -bios default -kernel $(UCOREIMG)。
工程缺少 tools/grade.sh，不能完成评分。

[GUARANTEE]
必须保留的接口：int kern_init(void) __attribute__((noreturn))；汇编入口 kern_entry。
需修改的构建接口：make gdb 支持 GDB 变量覆盖，默认使用工具链 GDB。
交付接口：make、make qemu、make debug、make gdb；report.md 和 prompt.md。
本任务以验证已有模块为主，不要求新增内核函数；可新增独立调试命令文件，不更改公开 C 接口。

[SPECIFICATION]
kern_entry
Pre-Condition：固件已将控制权交给内核入口，启动栈区域有效。
Post-Condition：进入 kern_init 前，sp 等于 bootstacktop；尾跳转不生成返回到入口的新返回地址。

kern_init
Pre-Condition：栈已初始化，链接符号和 SBI 控制台可用。
Post-Condition：未初始化静态存储区域清零，输出启动信息，持续停留在内核循环。
Requirements：不能将主动结束 QEMU 解释为内核崩溃。

gdb 目标与调试验证
Pre-Condition：存在带符号 ELF 和等待连接的 QEMU。
Post-Condition：使用指定调试器加载 ELF 并连接；实测复位 PC、ROM 指令、固件入口、内核入口与栈顶。
Case 1：存在工具链 GDB，默认命令可执行。
Case 2：使用多架构调试器，可通过 make GDB=gdb-multiarch gdb 覆盖。
Requirements：地址和指令必须来自实际反汇编；说明 QEMU 预加载镜像与固件交接的区别。

报告与证据
Pre-Condition：已取得模板、指导书和真实测试日志。
Post-Condition：按模板六个部分回答全部练习，准确记录测试结果；保留待补成员信息。
Case 1：工程缺评分脚本，说明 make grade 无法执行官方评分，不能自行伪造官方通过结果。
Case 2：无法获得原生终端截图，标记待补，不能将重绘日志冒充截图。
```

## P2：Lab0 环境检查与交叉编译

```text
[PROMPT]
任务：检查 Lab1 所需 Linux/WSL、RISC-V 工具链、Make、QEMU 和 GDB，分析 code/Makefile 的构建链路。
操作要求：读取实际配置，执行工具版本查询和工程构建；需要调整构建配置时修改实际文件。
输出要求：列出宿主架构、目标架构、工具版本和构建产物，解释编译、链接和镜像转换的作用。

[RELY]
GCCPREFIX=riscv64-unknown-elf-；CC=$(GCCPREFIX)gcc；LD=$(GCCPREFIX)ld。
OBJCOPY=$(GCCPREFIX)objcopy；工具链目录为 /opt/course-riscv/riscv-elf-toolchains/bin。
链接脚本 code/tools/kernel.ld：OUTPUT_ARCH(riscv)，ENTRY(kern_entry)，BASE_ADDRESS=0x80200000。
构建产物：bin/kernel、bin/ucore.img；QEMU 平台为 virt。

[GUARANTEE]
构建接口：make、make qemu。
本任务不新增 C 函数；保留链接入口和产物路径。

[SPECIFICATION]
## 环境检查
Pre-Condition：已能访问工程与 Linux/WSL 环境。
Post-Condition：明确每个必需工具是否可执行，记录可用工具的版本。
Case 1：工具可用，使用已配置的工具完成构建。
Case 2：工具缺失，报告具体缺失项及其影响，不声称环境准备完成。

## 构建
Pre-Condition：交叉编译器、链接器、二进制工具和 Make 可用。
Post-Condition：生成 RISC-V ELF64 内核及裸镜像，ELF 入口为 0x80200000。
Requirements：区分宿主机运行的编译器与生成代码的目标架构；ELF 用于符号调试，裸镜像用于启动。
```

## P3：汇编入口、启动栈与 C 初始化

```text
[PROMPT]
任务：分析 code/kern/init/entry.S、init.c 和 tools/kernel.ld，完成练习1并验证最小内核初始化行为。
操作要求：读取实际文件与反汇编；发现行为不符合规格时，在对应实际文件中修正。
输出要求：解释 la sp, bootstacktop 与 tail kern_init 的行为、目的和实际展开形式。

[RELY]
entry.S：kern_entry 中执行 la sp, bootstacktop 和 tail kern_init。
启动栈在 .data 中分配；PGSIZE=4096，KSTACKPAGE=2，KSTACKSIZE=8192。
链接入口为 0x80200000；edata 与 end 由链接脚本提供。
可用函数：void *memset(void *s, char c, size_t n)；int cprintf(const char *fmt, ...)。

[GUARANTEE]
汇编接口：kern_entry、bootstack、bootstacktop。
C 接口：int kern_init(void) __attribute__((noreturn))。
不改变入口名称、栈大小或公开接口；必要的辅助函数应为 static。

[SPECIFICATION]
## kern_entry
Pre-Condition：控制权到达内核入口，启动栈区域有效且符合对齐要求。
Post-Condition：进入 C 初始化前，sp 指向 bootstacktop；执行流转入 kern_init。
Requirements：解释尾调用不会为入口建立新的返回地址；不得假设伪指令必然只对应一条机器指令。

## kern_init
Pre-Condition：栈已经建立，edata/end 合法，SBI 输出可用。
Post-Condition：清零指定的静态未初始化存储区，输出启动文字，持续循环而不返回。
Case 1：清零区间非空，对整个区间进行清零。
Case 2：清零区间为空，不写入区间外内存，继续完成输出。
Requirements：启动栈不得被初始化清零覆盖；以调试器检查 sp 和 bootstacktop 的一致性。
```

## P4：格式化输出与 SBI 服务链路

```text
[PROMPT]
任务：分析 code/kern/libs/stdio.c、kern/driver/console.c、libs/printfmt.c 和 libs/sbi.c 的启动输出链路。
操作要求：读取各函数实际实现，运行内核核对输出；仅在发现问题时修改对应文件。
输出要求：说明格式化、逐字符输出、控制台封装和 ecall 的职责，并验证启动消息。

[RELY]
启动调用：cprintf("%s\n\n", message)。
调用链：cprintf → vcprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → sbi_call。
SBI_CONSOLE_PUTCHAR=1；sbi_call 使用 a7 传服务编号，a0–a2 传参数，通过 ecall 请求固件服务。
此实验使用支持 legacy SBI 控制台输出的 OpenSBI。

[GUARANTEE]
int cprintf(const char *fmt, ...)；int vcprintf(const char *fmt, va_list ap)；
void cons_putc(int c)；void sbi_console_putchar(unsigned char ch)；
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2)。
保留现有接口；不扩展到用户态系统调用或输入驱动实现。

[SPECIFICATION]
## 格式化输出
Pre-Condition：格式串及对应实参有效，控制台服务可用。
Post-Condition：按格式逐字符输出，cprintf 返回格式化输出的字符数量。
Requirements：变参访问遵循工程的 va_list 定义；字符计数与输出内容一致。

## SBI 控制台输出
Pre-Condition：固件支持相应服务，内核运行环境允许执行 ecall。
Post-Condition：字符通过固件控制台出现在 QEMU 终端。
Requirements：说明 S 模式内核请求 M 模式固件服务与用户态系统调用的区别；核对实际输出，不以代码阅读代替运行验证。
```

## P5：GDB 跟踪复位到内核的完整流程

```text
[PROMPT]
任务：完成 Lab1 练习2，使用 QEMU 与 GDB 跟踪复位 ROM、OpenSBI、kern_entry 和 kern_init。
操作要求：执行实际调试，完善 code/tools/startup.gdb，记录指令与寄存器观察结果。
输出要求：回答第一条指令地址、最初几条指令的功能、固件与内核交接地址，以及栈初始化结果。

[RELY]
make debug 使用 -s -S；GDB 默认连接 localhost:1234。
符号文件 bin/kernel；kern_entry=0x80200000；栈顶符号 bootstacktop。
当前 QEMU 4.1.1 的 virt 平台复位地址为 0x1000，OpenSBI 入口为 0x80000000。
可用命令：info registers、x/i、si、break、continue、p/x。

[GUARANTEE]
构建接口：make debug、make gdb；GDB 可通过变量 GDB 覆盖。
命令文件：code/tools/startup.gdb；不新增内核 C 接口。

[SPECIFICATION]
## 复位与固件
Pre-Condition：QEMU 在客体 CPU 执行前暂停，GDB 已连接。
Post-Condition：观察复位 PC，反汇编启动指令，单步确认跳转到固件入口。
Requirements：以当前平台的实际指令为准，不能用其他版本示例替代实测。

## 内核交接与栈
Pre-Condition：固件已开始执行，内核入口断点已设置。
Post-Condition：命中 kern_entry，再到达 kern_init，验证 sp=bootstacktop。
Requirements：在复位暂停时检查内核地址内容，区分 QEMU 预加载与固件控制权交接；不把未观察到的内存写入描述成调试结论。
```

## P6：按模板整理实验报告与提交目录

```text
[PROMPT]
任务：按报告模板完成 report/report.md，汇总提示词为 report/prompt.md，组织 report/images/ 中的测试截图。
操作要求：直接编辑实际 Markdown 文件；按练习顺序组织解答，检查目录与图片链接。
输出要求：报告包含基本信息、实验目的、环境、整体逻辑、内容与实现、测试、总结；Lab0 和 Lab0.5 合并为前置学习。

[RELY]
分支名称 lab1；顶层目录 code/、report/。
report/ 仅包含 report.md、prompt.md、images/。
成员学号、姓名、分工、各成员 AI 工具与模型由成员填写。
已验证构建、启动文字、复位流程和栈；工程缺少 tools/grade.sh。

[GUARANTEE]
文档接口：report/report.md、report/prompt.md；截图接口：report/images/。
本任务不修改内核函数；保留报告模板的主要章节。

[SPECIFICATION]
## 实验报告
Pre-Condition：已有指导书、模板和实际验证结果。
Post-Condition：完整回答两项练习，解释核心函数与模块，列出实验知识点及未涉及的 OS 原理。
Requirements：提示词按四块结构书写，测试结论与实际结果一致；不编造成员信息或未完成的评分。

## 截图与目录
Pre-Condition：测试已执行，截图来自对应测试过程。
Post-Condition：图片存入 images/，Markdown 用相对路径引用；report/ 无多余记录文件。
Case 1：截图已经取得，检查文件存在、内容对应和链接正确。
Case 2：截图尚未取得，报告明确待补，不能声称已完成截图交付。
```

## 交付材料整理任务

用户要求：report/ 仅包含 report.md、prompt.md 和 images/，删除多余文件；README 和其他交付文件直接说明最终成果，不包含错误版本或版本对比信息。

执行要求：清理多余报告附件，保留练习解答、实际环境和验证结果，检查引用和目录结构后提交 lab1 分支。
