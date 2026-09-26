# lab1：RISC-V 最小内核启动

本目录是 lab1 的源码。在 Ubuntu 中安装 RISC-V 交叉编译器、Binutils、QEMU 和 Make 后运行：

```bash
make
make qemu
```

成功时，在 OpenSBI 启动信息之后会显示 `(THU.CST) os is loading ...`。内核随后进入源码中的无限循环。退出 QEMU：先按 `Ctrl+A`，松开后按 `X`。

本次验证环境为 WSL2 / Ubuntu 24.04、GCC 13.2、Binutils 2.42、QEMU 8.2.2。工作副本的 Makefile 有两处与当前 QEMU 配套的启动参数调整，其他原始源码保持不变；原因和验证记录见 [`../report/实验执行记录.md`](../report/实验执行记录.md)。

调试时，终端一运行 `make debug`；终端二运行：

```bash
gdb-multiarch bin/kernel -ex 'set arch riscv:rv64' -ex 'target remote localhost:1234'
```

然后在 GDB 中对 `kern_entry` 和 `kern_init` 设置断点。
