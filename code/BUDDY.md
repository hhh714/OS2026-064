# Lab2 伙伴系统扩展

## 接入与运行

默认仍为 Best Fit，原有测试不修改。伙伴系统通过相同 pmm_manager 接口接入。
新文件为 kern/mm/buddy_pmm.c 和 buddy_pmm.h，pmm.c 负责选择，Makefile 提供 PMM 参数。

每次切换策略必须先清理：Make 不会自动检测编译参数变化。

```sh
export PATH="/opt/course-riscv/riscv-elf-toolchains/bin:$PATH"
make clean
make PMM=buddy
make PMM=buddy qemu
```

预期出现 `memory management: buddy_pmm_manager`、`buddy_check() succeeded`、
`check_alloc_page() succeeded!`。伙伴系统自检只在选用该管理器时执行。

恢复基础评分：

```sh
make clean
make
make grade
```

make grade 的原测试针对 Best Fit，不代表伙伴系统覆盖率或 Challenge 官方评分。

## 算法与契约

- 以 4 KiB 页为基本单位，按 order=0..18 分阶，每阶块长度为 2^order 页。
- 使用绝对物理页号对齐，伙伴页号为 PPN XOR (1 << order)。
- 初始化把任意长度范围拆成对齐的幂次块；DTB 等保留洞不注册，不能跨洞合并。
- 分配向上取整，再从更高阶拆分。例如 alloc_pages(3) 实际占用 4 页。
- free_pages(p, n) 必须传回原首页和原请求页数，例如 free_pages(p, 3)。
  不支持局部释放、重复释放、错误长度释放或尚有引用的页释放，检查失败触发断言。
- Page 公共布局不变。此管理器内部使用 flags 的第2位标记已分配，
  第3位标记分配首页；不要与 PTE 的 R/W/X 权限混淆。
- 空闲块首页 property 记录块页数；已分配首页 property 记录原请求页数。
- nr_free_pages 按实际剩余页数统计，因此申请3页会减少4页。
- 当前是单 hart 初始化阶段实现，不具备并发锁或运行时切换管理器支持。
- SLUB 不属于本次实现。

## 自检设计

借用16个真实连续页并暂时隔离空闲链表，完成测试后恢复原链表并归还页面。
覆盖：空池失败、7页非幂次区间、保留洞、多个区间、单页耗尽和去重、
逆序释放、跨阶递归合并、整块分配、3/5页向上取整、对齐、不重叠、
容量不足和超大请求失败、计数及链表一致性、测试前后页数恢复。
非法释放通过实现中的断言保护；启动自检不主动触发这些致命断言。
