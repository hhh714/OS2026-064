# Lab2：物理内存与页表

本目录包含 Sv39 启动映射、设备树内存解析、物理页初始化与最佳适配分配器。

```bash
make
make qemu
make grade
```

已在 WSL Ubuntu 环境中验证启动和分配器检查，配套评分结果为 20/20。

调试内核运行 `make debug`，另一个终端运行 `make gdb`。观察 QEMU 内部 MMU 需额外使用带调试信息的 QEMU 和宿主 GDB。

目录中的 tools/startup.gdb 是保留的 Lab1 调试脚本，不适用于本实验的高地址入口；Lab2 调试流程需另行整理。
