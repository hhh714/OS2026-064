# lab1：RISC-V 最小内核启动

本目录是 lab1 的源码。在 Ubuntu 中安装 RISC-V 交叉编译器、Binutils、QEMU 和 Make 后运行：

```bash
make
make qemu
```

成功时，在 OpenSBI 启动信息之后会显示 `(THU.CST) os is loading ...`。内核随后进入源码中的无限循环。退出 QEMU：先按 `Ctrl+A`，松开后按 `X`。

2026-10-07 复核环境为 WSL2 / Ubuntu 24.04.5、GCC 15.1.0、QEMU 4.1.1。两处 QEMU 启动参数使用 `-kernel`；GDB 调用支持变量覆盖；内核 C/汇编保持原始工程内容。完整说明与真实日志见 [实验报告](../report/report.md)。

调试时，终端一运行 `make debug`；终端二运行：

```bash
gdb-multiarch bin/kernel -ex 'set arch riscv:rv64' -ex 'target remote localhost:1234'
```

然后在 GDB 中对 `kern_entry` 和 `kern_init` 设置断点。

复位流程验证命令见 `tools/startup.gdb`，默认连接自动验证端口 12345；交互式 `make debug` 使用 1234，应调整连接端口后使用。也可执行 `make GDB=gdb-multiarch gdb`。

原始压缩包和本仓库没有 `tools/grade.sh`，所以现有 `make grade` 不能进行官方评分。不要将启动成功等同于官方评分通过。
