# Lab1：RISC-V 最小内核启动

在 Linux 或 WSL 中配置 RISC-V 工具链、Make 和 QEMU，运行：

```bash
make
make qemu
```

成功时输出 `(THU.CST) os is loading ...`，随后内核进入无限循环。退出 QEMU：按 Ctrl+A，松开后按 X。

验证环境：WSL2、Ubuntu 24.04.5、GCC 15.1.0、QEMU 4.1.1。实验说明见 [report.md](../report/report.md)。

调试时，终端一运行 `make debug`；终端二运行 `make gdb`。使用多架构调试器时运行 `make GDB=gdb-multiarch gdb`。连接端口为 1234，完整调试命令见 `tools/startup.gdb`。

已验证编译、启动输出和 GDB 启动流程。工程缺少 `tools/grade.sh`，`make grade` 无法执行评分。
