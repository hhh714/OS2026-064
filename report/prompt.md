# Lab2 提示词

本文包含六份四段式任务规格：物理内存初始化、Best Fit实现、Buddy实现与测试、SLUB实现与测试。前两份沿用已有材料，后四份依据本次实际协作需求整理为规范版本，不作为逐字会话历史。分配器实现与测试分别定义职责，测试预期不因实现结果而降低。

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

## 提示词三 Buddy System 分配模块

### [PROMPT]

参考现有ucore物理内存管理框架，从零实现伙伴系统并接入内核。目标是按2的幂管理连续物理页，通过分裂与伙伴合并完成分配、释放。
仅新增buddy_pmm.c/.h及必要接入、编译选项；不得修改Page公共布局、原有Best Fit/First Fit或框架测试。默认仍用Best Fit，支持PMM=buddy切换。不得修改启动映射、DTB解析或评分脚本。采用单hart启动环境，明确并发限制。
输出实现、接口契约、构建命令和适用边界；未实际运行时不得声称通过。

### [RELY]

提供pmm.h的struct pmm_manager、Page定义、PGSIZE、页号转换、页状态宏、list.h及pmm.c当前接入方式。
page_init可能多次注册不连续范围，范围不保证为2的幂，DTB页保持保留。物理页描述符从nbase开始，npage为页号上界。

### [GUARANTEE]

对外定义const struct pmm_manager buddy_pmm_manager，按现有签名提供init、init_memmap、alloc_pages、free_pages、nr_free_pages、check。
内部函数可自行设计。通过原统一接口调用，提供后续独立自检的入口，不改原评分契约。

### [SPECIFICATION]

- init：前置无分配者；后置各阶链表为空、空闲计数为0。
- init_memmap(base,n)：前置范围合法、n>0且页面保留；后置恰好注册指定范围，分为物理页号对齐的幂次块，不丢失尾页、不跨保留洞。重复或非法注册拒绝。
- alloc_pages(n)：n>0；向上取整为2的幂，从最小可用阶取得块，必要时拆分。成功返回对齐首页，实际占用范围不与其他分配重叠；计数减少实际块页数。无法满足或请求过大返回NULL且状态不变。
- free_pages(base,n)：必须为有效分配首页及原请求页数，不支持局部释放；检查边界、分配记录和引用后，归还实际块。只合并同阶且互为伙伴的空闲块，使用绝对物理页号定位，不将描述符下标误作PPN。重复、错误长度、内部页释放拒绝。
- nr_free_pages：返回实际未分配页数，不修改状态。申请3页占4页、释放原请求3页后恢复4页。
- check：留出可调用自检位置；测试由下一任务设计，不能靠更改测试迁就错误实现。

## 提示词四 Buddy System 测试模块

### [PROMPT]

针对伙伴系统实现设计并接入独立测试。目标是验证阶选择、拆分、合并、保留洞和状态恢复，而不只是检查返回非空。
不得修改算法、公共接口和框架测试。允许新增内部测试辅助函数和check调用；如发现实现缺陷，先报告，不降低断言要求。
输出每个场景的前置状态、操作、断言、理由和运行方法。致命非法输入测试应隔离，不得提前终止正常测试组。

### [RELY]

仅提供伙伴系统接口契约、该模块代码、Page与链表定义、页号转换和计数接口。测试在单hart启动阶段执行，尚无其他分配者；可借用真实连续页隔离测试，必须保留并恢复原状态。

### [GUARANTEE]

提供buddy_check及必要内部一致性检查，通过buddy_pmm_manager.check调用。成功结束恢复原空闲页数；失败指出触发的断言。不得释放其他使用者的页面。

### [SPECIFICATION]

- 空池申请1页返回NULL；申请超过支持最大块大小返回NULL且计数不变。
- 注册非幂次7页区间及另一8页区间，隔开1页保留洞；逐页分配15次，不重复、不返回洞中页，第16次失败。
- 逆序释放，空闲量恢复15；注册中间页后，可递归合成并申请对齐16页块。错误伙伴关系、跨洞合并应被该场景识别。
- 请求3页和5页分别占4、8页；检查物理对齐、不重叠及空闲量减少12页。容量不足请求不改变状态。
- 检查各阶节点长度、对齐、双向连接及该阶页数累计，总和等于全局空闲计数。
- 测试结束释放持有页，恢复原链表和数量。若只通过实现断言保护非法释放，明确该项未做独立预期失败运行，不冒充测试覆盖。

## 提示词五 简化 SLUB 对象分配模块

### [PROMPT]

从零实现简化SLUB，在已有页分配器上提供不同字节大小的内核对象分配。必须体现“底层页分配、上层小对象分配”两层架构，可简化，不要求Linux完整特性。
只新增slub.c/.h、最小启动接入及SLUB=1选项；不修改Page布局、Best Fit/Buddy算法、原检查函数或评分脚本。默认不启动扩展，兼容Best Fit和Buddy两种底层。
输出接口、实现流程、错误处理、运行命令及限制。不得猜测头文件中被注释的宏可用，先核实定义。

### [RELY]

提供alloc_pages/free_pages/nr_free_pages、Page到物理地址的转换、va_pa_offset、PGSIZE、list.h、字符串操作和断言接口。
内核直接映射支持页描述符对应物理页到内核虚拟地址转换；Buddy释放要求传原页请求数。初始化发生在pmm_init之后、单hart无并发对象使用者阶段。

### [GUARANTEE]

公开接口：void slub_init(void)、void *kmalloc(size_t size)、void kfree(void *object)、void slub_check(void)。
内部元数据及辅助函数自行设计；不依赖kmalloc分配自身元数据，避免递归初始化。单次初始化，不运行时更换底层分配器。

### [SPECIFICATION]

- slub_init：在PMM可用后调用一次，建立空的大小类及对象管理链表，不提前占用动态页。
- kmalloc：size=0返回NULL；支持16..2048字节幂次大小类、16字节对齐。小对象从已有可用slab取槽位，无可用slab时才申请页并切分。对象内freelist与分配位图配合，维护partial/full状态。
- 大于2048字节请求使用包含管理头开销的连续页，计算前检查大小溢出；记录原页请求数以便适配Buddy。大于支持范围或底层失败返回NULL，不破坏已有对象。
- kfree：NULL无操作；根据所有权信息定位slab，再校验对象起点和分配状态，不直接解引用任意输入地址。释放槽位后恢复freelist；空slab归还整页，大对象归还原页请求范围。非法及可识别的重复释放触发断言。
- 页头和每个对象不重叠，允许写入全部请求字节；释放不要求调用者传长度。返回内容未初始化，调用者不得越界。
- 说明不实现多核per-CPU缓存、同步锁及悬空指针跨地址复用检测，不声称等同Linux完整SLUB。

## 提示词六 SLUB 测试模块

### [PROMPT]

独立设计简化SLUB自检，验证不同大小对象分配和释放、复用、错误处理及无泄漏，并分别在Best Fit、Buddy底层实际运行。
只新增测试和必要的内部可控故障注入，不能修改算法行为以通过测试。保留原基础评分。失败输出与尚未执行的预期必须区分。

### [RELY]

提供slub接口及实现、页分配接口和计数、大小类/对齐约定。测试运行于空缓存的单hart启动阶段；内部释放检查可返回状态，公开kfree仍按契约触发断言。

### [GUARANTEE]

交付slub_check及测试说明，SLUB=1时在初始化后调用。所有成功申请对象最终释放；正常返回时底层空闲页数恢复，缓存链表为空。

### [SPECIFICATION]

- 测试0字节、极大请求和kfree(NULL)，确认返回及状态符合约定。
- 覆盖各大小类边界前后、1字节、2048/2049、4096及9000字节。检查16字节对齐，写入并核对全部请求字节。
- 同时持有不同大小对象，填入不同模式，验证数据隔离；至少持有超过单slab容量的同类对象，检查地址不重复及跨slab正确性。
- 从满slab释放一个对象后重新分配，验证full到partial转换及空槽复用；最后释放所有对象，验证页归还。
- 检查内部指针、未知地址和可识别重复释放被拒绝且状态不变；用内部非致命检查测试，明确这不等于运行公开接口的致命断言测试。
- 在底层页申请入口模拟单次失败，分别触发小对象新slab和大对象失败路径；返回NULL且空闲页数不变。注明是故障注入，不声称真实耗尽整机RAM。
- 分别给出SLUB=1和PMM=buddy SLUB=1的干净构建命令，以实际运行日志确认结果。测试不能保证所有输入、并发或完整Linux特性正确。
