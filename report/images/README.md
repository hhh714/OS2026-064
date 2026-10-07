# 待补真实测试截图

日志已保存在 ../records/，但日志文件不替代课程要求的截图。

1. 在 code/ 运行 `make clean && make`，截图保存为 build.png。
2. 运行 `make qemu`，截取 OpenSBI 信息和启动文字，保存为 boot.png；Ctrl+A 然后 X 退出。
3. 两个终端运行 `make debug`、`make gdb`，按 tools/startup.gdb 中的命令调试，分别截图复位指令、固件入口、内核入口及 sp=bootstacktop，保存为 reset.png、kernel-entry.png。
4. 在 report.md 的测试截图节添加 `![描述](./images/文件名.png)`。
5. 原工程缺少 tools/grade.sh，不生成虚假的评分截图。取得官方脚本后才运行 make grade 并截图；若教师确认 Lab1 无评分脚本，保留说明。
