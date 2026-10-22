# Lab2 提示词

本文按两个可独立实现和验证的功能模块组织四段式任务提示词：物理内存初始化、最佳适配连续物理页分配。内容依据本次工程整理

## 提示词一：物理内存初始化模块

### [PROMPT]

**任务详情：** 阅读当前Lab2工程，在 `kern/mm/pmm.c` 指定位置实现物理内存初始化模块。根据设备树提供的RAM范围建立物理页描述符数组，保护已占用区域，将剩余完整物理页注册到物理内存管理器。完整考虑RAM边界、内核边界、描述符数组、设备树占用及虚实地址关系，不能直接将end之后全部内存视为空闲。

**操作约束：**

- 修改实际文件，仅允许在 `kern/mm/pmm.c` 指定位置补充实现及本文件内部static辅助函数。
- 保留 `pmm_init` 调用顺序、公共接口、数据结构、宏、页大小及地址布局。
- 不修改设备树解析、启动汇编、链接脚本和测试。
- 不调用依赖当前分配器的动态内存分配，避免初始化循环依赖。
- 保留高地址直接映射和按物理页管理的工程语义。
- 将指定YOUR CODE归属标记替换为实际负责人学号；未提供时不推测。

**输出与验证要求：** 说明修改位置、结果与适用范围；验证内存范围有效、页号语义一致、描述符页对齐且位于内核后、内核及描述符和DTB页不可分配、空闲集合正确、地址转换往返一致。测试不能被修改以取得通过结果，未运行项明确说明。

### [RELY]

- `PGSIZE=4096`，`PGSHIFT=12`；RAM起点为 `DRAM_BASE`，直接映射偏移为 `PHYSICAL_MEMORY_OFFSET`。
- 启动映射覆盖从DRAM_BASE开始的1 GiB物理区域。
- 链接符号 `extern char end[]` 表示内核静态布局结束的虚拟地址。
- DTB头部采用大端格式，字节4～7记录总字节长度。

```c
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);
extern uint64_t boot_dtb;

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

struct Page *pages;
size_t npage;
const size_t nbase; /* DRAM_BASE / PGSIZE */
uint64_t va_pa_offset;

static void init_memmap(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

可用接口：`SetPageReserved`、`set_page_ref`、`PPN`、`ROUNDUP`、`ROUNDDOWN`、`PADDR`、`pa2page`、`page2pa`。npage表示物理内存结束位置的页号上界，描述符数量与npage-nbase对应。实现前核对实际头文件，不另造名称。

### [GUARANTEE]

```c
static void page_init(void);
```

正确建立 `pages`、`npage`、`va_pa_offset`，通过现有 `init_memmap` 注册可用页。允许增加static辅助函数，不更改公共接口。

### [SPECIFICATION]

#### page_init

**Pre-Condition：** 已进入具有直接映射的C初始化阶段；管理器已选定并初始化；dtb_init已执行；链接符号、DTB地址和转换定义有效。

**Post-Condition：**

- va_pa_offset等于直接映射偏移，npage表示RAM完整页范围的页号上界。
- pages位于内核end之后并页对齐，数组覆盖需要描述的完整物理页。
- 固件前置保留区、内核、描述符和RAM内DTB覆盖页不进入空闲集合。
- 其余可用完整页通过init_memmap注册，区域互不重叠，不注册空区间或不完整页。

**Case 1：布局有效。** 完成描述符和空闲区初始化，使后续分配释放可用。

**Case 2：信息无效或不支持。** 对零长度、起点不符、范围溢出、超出启动映射、描述符空间不足或无可用页，通过断言或内核错误机制停止，不注册无效范围。

**Case 3：DTB与候选空闲区重叠。** 排除DTB覆盖的完整页，仅注册前后非空可用区间。

**Case 4：DTB不重叠。** 注册完整候选空闲区间。

**Requirements：** 采用明确的左闭右开边界；区分虚拟地址、物理地址、页号和描述符下标；检查容量和算术边界；不覆盖内核及DTB。说明实现针对当前单段RAM实验布局，不声称支持全部设备树保留规则。

## 提示词二：最佳适配连续物理页分配模块

### [PROMPT]

**任务详情：** 在 `kern/mm/best_fit_pmm.c` 指定位置实现完整最佳适配模块，包含初始化、初始空闲区注册、连续页申请、释放合并和计数查询。申请时从所有足够大的块中选择最小块；同大小选择较低地址，兼容现有管理器接口及检查函数。

**操作约束：**

- 修改实际文件，仅补充指定实现位置；允许模块内部static辅助函数与管理状态。
- 允许在 `kern/mm/pmm.c` 管理器选择处启用best_fit_pmm_manager。
- 不修改basic_check、best_fit_check、评分测试、数据结构、宏或接口签名。
- 不硬编码测试地址、不伪造PASS、不跳过断言。
- 不修改启动映射、设备树解析及其他分配器。
- 指定归属标记填写实际负责人学号，未提供时不推测。

**输出与验证要求：** 解释链表不变量、选择规则、分裂合并；验证地址有序且区间不重叠、块头长度正确、总计数一致、失败不改变状态、精确匹配与分裂正确，以及释放时前后和双侧合并。运行现有检查及make grade，未执行项明确说明。

### [RELY]

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;

struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
```

页接口：`PageReserved`、`PageProperty`、`SetPageProperty`、`ClearPageProperty`、`set_page_ref`、`le2page`。

循环双向链表接口：`list_init`、`list_empty`、`list_next`、`list_prev`、`list_add`（插在节点后）、`list_add_before`、`list_del`。

同一pages数组中相邻描述符对应相邻物理页；初始区域由page_init筛选且描述符为保留状态；检查函数直接使用free_list和nr_free，需提供兼容名称。申请返回描述符指针，不是业务内存指针。当前不要求多核并发同步。以实际头文件核对定义。

### [GUARANTEE]

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
const struct pmm_manager best_fit_pmm_manager;
```

保留现有 `basic_check`、`best_fit_check`，管理器check成员连接best_fit_check。允许增加static辅助函数，不更改公开接口。

### [SPECIFICATION]

#### best_fit_init

**Pre-Condition：** 尚未注册物理页，不存在需保留的在用管理状态。

**Post-Condition：** 空闲循环链表有效且为空，空闲计数为0。

#### best_fit_init_memmap

**Pre-Condition：** n>0；base描述合法连续n页；全部为保留页；区域可分配且与已注册区间不重叠。

**Post-Condition：** 清除区域保留状态，引用计数为0；首描述符为空闲块头且property=n；块头进入按地址递增的链表；空闲页总数增加n。

**Case 1：链表为空。** 注册后含一个有效空闲块。

**Case 2：链表非空。** 插入正确地址位置，保留已有块和有效连接。

#### best_fit_alloc_pages

**Pre-Condition：** n>0，零页申请触发断言；管理状态满足不变量。

**Post-Condition：** 成功返回连续n页的首描述符，选择最小够用块，同大小选择较低地址；分配区域退出空闲集合，总数减少n，其他已分配区域不受影响。失败返回NULL，链表、描述符状态及计数不变。

**Case 1：精确匹配。** 分配整块，不产生剩余块。

**Case 2：最优块较大。** 从低地址端分配n页，剩余部分构成有效空闲块，链表仍有序。

**Case 3：总空闲页不足。** 返回NULL，不修改状态。

**Case 4：总数足够但连续块不足。** 返回NULL，不修改状态。

**Requirements：** 不保证返回页数据已清零；PageProperty为0不能单独证明该页已分配；不要求按块大小排序，说明当前查找复杂度。

#### best_fit_free_pages

**Pre-Condition：** n>0，零页释放触发断言；base和n表示合法、连续、允许释放的页；调用者保证不再使用、不是保留页且未重复释放；管理状态有效。

**Post-Condition：** 区域可重新分配、引用计数为0，总空闲页数恰好增加n；链表按地址有序且不重叠；与释放区直接相邻的空闲块合并，块头长度正确。合并不额外增加计数。

**Case 1：两侧不相邻。** 新增独立空闲块。

**Case 2：仅前侧相邻。** 合并后以前块为起点。

**Case 3：仅后侧相邻。** 合并后以释放区为起点。

**Case 4：两侧相邻。** 形成覆盖三段的单一连续块。

**Requirements：** 不要求维护每次申请记录；支持释放合法持有的连续范围；不声称现有断言识别所有非法释放；不修改框架测试。

#### best_fit_nr_free_pages

**Pre-Condition：** 管理器已初始化，计数有效。

**Post-Condition：** 返回空闲页总数，单位为页，不修改状态。

#### best_fit_pmm_manager

**Pre-Condition：** 实现签名符合框架。

**Post-Condition：** 接口成员连接对应最佳适配实现，check连接既有检查函数，上层统一接口实际使用最佳适配策略。
