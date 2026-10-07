# Lab1 提示词汇总

## 记录范围与真实性

2026-09-23 的自然语言提示词原文保留在 `records/AI协作记录-历史.md`，它们不符合本次指导书的标准结构，不能事后声称原本符合规范。本文件中的 P1 是 2026-10-07 复核和修订使用的任务规格，由本次用户请求规范化整理；它不是历史用户逐字提示词。成员此前使用的其他提示词如存在，应由成员补充原文、日期、迭代结果。当前用户原始请求为：核对 lab1.zip 与小组仓库已有代码，按指导书规范和报告模板完成 Lab0、Lab0.5、Lab1 的材料；随后确认前两项合并到 lab1，成员信息自行补充。

## P1：启动工程复核、调试可复现性与报告规范整理

```text
[PROMPT]
任务：检查 code/ 下最小内核启动模块，核对提供的原始 lab1 工程；完善 Makefile 的调试器可配置性；补充真实复位流程验证与 report/report.md、report/prompt.md。
操作要求：修改实际文件。先检查现有工程和工具版本，再编译、运行、调试。依据实际输出修订报告。
输出要求：保留内核行为和公开接口；保留历史提示词记录；标明规范化任务规格的来源；不编造成员信息、历史迭代、截图或评分结果。

[RELY]
真实代码：ENTRY(kern_entry)，BASE_ADDRESS=0x80200000。
entry.S：la sp, bootstacktop；tail kern_init。
memlayout.h：KSTACKPAGE=2；KSTACKSIZE=(KSTACKPAGE*PGSIZE)。
mmu.h：PGSHIFT=12；PGSIZE=4096。
init.c：extern char edata[], end[]；memset(edata,0,end-edata)；cprintf 后无限循环。
可用接口：void *memset(void *s, char c, size_t n)；int cprintf(const char *fmt, ...)。
Makefile 已有 qemu/debug，使用 -bios default -kernel $(UCOREIMG)。
原始压缩包没有 tools/grade.sh。现有日志不是本次测试结果。

[GUARANTEE]
必须保留的接口：int kern_init(void) __attribute__((noreturn))；汇编入口 kern_entry。
需修改的构建接口：make gdb 支持 GDB 变量覆盖，默认工具名称兼容原工程。
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
Post-Condition：按模板六个部分回答全部练习，引用可核验日志；保留待补成员信息。
Case 1：原工程缺评分脚本，说明 make grade 无法执行官方评分，不能自行伪造官方通过结果。
Case 2：无法获得原生终端截图，标记待补，不能将重绘日志冒充截图。
```

## 本次执行反馈

第一轮：比对原包发现仅两处 QEMU 参数变更；运行成功。发现旧报告环境不是当前环境，更新环境记录。调试脚本初版多执行了一条 si，导致“固件入口”观察点晚一条指令；第二轮修正为四条单步后观察跳转，再单步进入固件。

第二轮：按真实复位指令和内存内容完成练习2；GDB 参数改为可配置；报告采用模板并明确截图、成员信息和原始评分脚本缺失状态。
