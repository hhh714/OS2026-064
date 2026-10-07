# Lab1 提示词汇总

## 最小内核启动与调试任务规格

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

## 交付材料整理任务

用户要求：report/ 仅包含 report.md、prompt.md 和 images/，删除多余文件；README 和其他交付文件直接说明最终成果，不包含错误版本或版本对比信息。

执行要求：清理多余报告附件，保留练习解答、实际环境和验证结果，检查引用和目录结构后提交 lab1 分支。
