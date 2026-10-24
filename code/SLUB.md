# Lab2 简化 SLUB Challenge

## 定位与架构

页级分配器（默认 Best Fit，或 Buddy）提供 alloc_pages/free_pages；
slub.c 在其上提供 kmalloc(size)/kfree(pointer)。Page 公共结构不变。
这是体现 SLUB 思想的单 hart 教学实现，不是 Linux SLUB 移植，没有并发锁、
per-CPU 缓存或构造函数，不可在并发/中断调用环境中未经同步使用。

## 接口与规则

- 先 pmm_init，再 slub_init；初始化只能调用一次。
- kmalloc(0) 返回 NULL，内存不足或过大请求返回 NULL。
- 16..2048 字节共8个大小类，每次选择不小于请求的最小类。
- 每个小对象 slab 占一页，页头保存管理数据，剩余空间切成对象。
- 对象按16字节对齐；超过2048字节时使用连续页，包含管理头开销。
  任意大小是支持接口范围内不同字节数的请求，不代表可以申请无限内存。
- 小对象空闲时，其前几个字节存下一个对象指针；位图记录分配状态。
- 缓存维护 partial/full 两条链表；分配满时转入 full，释放后转回 partial。
- 空 slab 立即归还页分配器；大对象按原页请求数释放，兼容 Buddy 取整规则。
- kfree(NULL) 无操作。错误地址、内部地址和可识别的重复释放触发断言。
  指针地址被再次分配后，无法区分旧指针和新指针，调用者不得使用悬空指针。
- kfree 不需要传长度。分配结果未初始化；调用者只能使用所请求的字节范围。
- 通过已管理 slab 列表定位所有权后再访问对象，未将任意输入指针当作页头解引用。

## 验证命令

每次切换编译选项必须 make clean。默认 SLUB=0，保留原评分路径。

```sh
export PATH="/opt/course-riscv/riscv-elf-toolchains/bin:$PATH"
make clean
make SLUB=1
make SLUB=1 qemu
```

伙伴系统作为底层：

```sh
make clean
make PMM=buddy SLUB=1
make PMM=buddy SLUB=1 qemu
```

预期输出 slub_check() succeeded；这是自检预期，不表示尚未运行就已通过。
恢复基础评分：make clean，然后 make，再 make grade。

## 测试内容

slub_check 在启动、没有其他对象使用者时运行：

- 0、超大请求及 kfree(NULL)。
- 大小类边界的前后值、1字节、2048/2049、整页请求及9000字节。
- 每个对象的对齐与全部请求字节的读写验证。
- 超过一页容量的同时分配、地址去重、数据互不破坏。
- full/partial 状态转换及对象槽位复用。
- 内部指针、未知指针、重复释放的拒绝；自检调用内部非致命检查路径。
- 在底层申请页的边界注入一次失败，验证小对象和大对象的 OOM 返回及页数不变。
  这是可控故障注入，不是耗尽机器所有物理内存。
- 所有测试对象释放后，缓存链表为空，底层空闲页总数恢复。

代码写入不等于实际编译运行通过，需保存两种底层配置的实际日志。
