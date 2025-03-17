#include <kernel/config.h>
#include <kernel/mm/page.h>
#include <kernel/mm/pagetable.h>

#include <util/atomic.h>
#include <util/list.h>
#include <util/spinlock.h>
#include <util/string.h>
#include <util/sync_utils.h>

#include <spike_interface/spike_utils.h>

// 页结构数组，用于跟踪所有物理页
static struct page *page_pool = NULL;
static uint64 total_pages = 0;
static uint64 page_map_size = 0;

// LRU页链表头
static struct list_head page_lru_list;
static spinlock_t page_lru_lock;

// 物理内存布局
paddr_t mem_base_addr;
paddr_t mem_size;


static struct list_head free_page_list;
static spinlock_t free_page_lock;
static uint64 free_page_counter;

static void init_page_struct(struct page *page);
static paddr_t get_free_page_addr(void);
static void put_free_page(paddr_t addr); // 释放一个物理页到空闲列表，不涉及page结构



// 获取页框号 (PFN)
// 注意，这里addr的值域大于整个物理内存空间
// 所以说，在addr大于mem_base_addr的情况下，我们需要用内核页表来映射
static uint64 get_pfn(paddr_t pa) {
	if(unlikely(pa < mem_base_addr)) {
		sprint("get_pfn: invalid address 0x%lx\n",pa);
		panic();
	}
	if(pa >= mem_base_addr + mem_size) {
		pa = lookup_pa(g_kernel_pagetable,pa);
	}
  return (pa - mem_base_addr) / PAGE_SIZE;
}

// 初始化页结构
static void init_page_struct(struct page *page) {
  if (!page)
    return;

  page->flags = 0;
  atomic_set(&page->_refcount, 0);
  page->index = 0;
  page->paddr = 0;
  page->mapping = NULL;
  INIT_LIST_HEAD(&page->lru);
  spinlock_init(&page->page_lock);
}

// 初始化页管理子系统
void init_page_manager() {
  // 内核程序段起止地址
	extern char _end[];

  paddr_t kernel_end = (paddr_t)&_end;
  uint64 pke_kernel_size = kernel_end - KERN_BASE;
  sprint("PKE kernel start 0x%lx, PKE kernel end: 0x%lx, PKE kernel size: "
         "0x%lx.\n",
         KERN_BASE, kernel_end, pke_kernel_size);
  // 空闲内存起始地址必须页对齐
  paddr_t free_mem_start_addr = ROUNDUP(kernel_end, PAGE_SIZE);

	extern struct MemInfo memInfo; // 从 dtb.c 中获取物理内存信息
  mem_base_addr = KERN_BASE;
  mem_size = ROUNDDOWN(MIN(PKE_MAX_ALLOWABLE_RAM, memInfo.size), PAGE_SIZE);
  assert(mem_size > pke_kernel_size);
  sprint("Free physical memory address: [0x%lx, 0x%lx) \n", free_mem_start_addr,
         DRAM_BASE + mem_size);
  free_page_counter = 0;

  total_pages = (mem_size / PAGE_SIZE);
  // 为页结构数组分配空间
  uint64 page_map_size = ROUNDUP(total_pages * sizeof(struct page), PAGE_SIZE);

  // 保留空间给页结构数组
  page_pool = (struct page *)free_mem_start_addr;

  // 初始化所有页结构
  for (uint64 i = 0; i < total_pages; i++) {
    init_page_struct(&page_pool[i]);
  }

	INIT_LIST_HEAD(&free_page_list);
	spinlock_init(&free_page_lock);
	INIT_LIST_HEAD(&page_lru_list);
	spinlock_init(&page_lru_lock);

  sprint("Page subsystem initialized: %d pages, map size: %lx bytes at 0x%lx\n",
         total_pages, page_map_size, (paddr_t)page_pool);

  // 返回空闲内存开始地址（页结构后的地址）
  paddr_t free_start = free_mem_start_addr + page_map_size;
  sprint("Free memory starts at: 0x%lx\n", free_start);


  for (paddr_t i = free_start; i < DRAM_BASE + mem_size; i += PAGE_SIZE) {
    put_free_page(i);
    // sprint("Free page list initialized with %lx pages\n", i);
  }
  sprint("Physical memory manager initialization complete.\n");
}

// 获取一个空闲页物理地址
static paddr_t get_free_page_addr(void) {
	uint32 flags = spinlock_lock_irqsave(&free_page_lock);

  struct list_head *n = free_page_list.next;
	if(n){
		list_del(n);
		free_page_counter--;
	}else{
		sprint("get_free_page_addr: no free page\n");
	}

	spinlock_unlock_irqrestore(&free_page_lock,flags);
	return (paddr_t)n;
}




// put_free_page
// 释放一个物理页到空闲列表，不涉及page结构
static void put_free_page(paddr_t addr) {
  // 检查地址是否在有效范围内
  if (((uint64)addr % PAGE_SIZE) != 0 || (uint64)addr < mem_base_addr ||
      (uint64)addr >= mem_base_addr + mem_size) {
    sprint("put_free_page: invalid address 0x%lx\n", addr);
    panic();
  }

  // 插入物理页到空闲链表
  struct list_head *n = (struct list_head *)addr;
	uint32 flags = spinlock_lock_irqsave(&free_page_lock);
	list_add(n, &free_page_list);
	free_page_counter++;
	spinlock_unlock_irqrestore(&free_page_lock,flags);
}

// 根据页框号获取页结构
struct page *pfn_to_page(uint64 pfn) {
  if (pfn >= total_pages)
    return NULL;
  // sprint("pfn_to_page: pfn = %lx\n",pfn);
  return &page_pool[pfn];
}

// 根据物理地址获取页结构
struct page *addr_to_page(paddr_t addr) {
  uint64 pfn = get_pfn(addr);
  // sprint("addr_to_page: pfn=%lx\n",pfn);
  return pfn_to_page(pfn);
}

// 根据页结构获取页框号
uint64 page_to_pfn(struct page *page) {
  if (!page)
    return 0;
  return page - page_pool;
}

// 分配单个页结构及对应物理页
struct page *alloc_page(void) {
  paddr_t pa = get_free_page_addr();
  // sprint("alloc_page: pa=%lx\n",pa);

  if (!pa)
    return NULL;

  memset((void*)pa, 0, PAGE_SIZE);
  struct page *page = addr_to_page(pa);
  // sprint("alloc_page: page=%lx\n",page);
  if (page) {
    init_page_struct(page);
    atomic_set(&page->_refcount, 1); // 初始引用计数为1
    page->paddr = pa;
  }

  return page;
}

// 释放单个页结构及对应物理页
void put_page(struct page *page) {
  if (!page)
    return;

  // 确保引用计数为0
  if (atomic_read(&page->_refcount) != 0) {
    // 引用计数不为0，不能释放
    return;
  }

    init_page_struct(page); // 重置页结构
    put_free_page(page->paddr);      // 释放物理页
}

// 增加页引用计数
void get_page(struct page *page) {
  if (!page)
    return;
  atomic_inc(&page->_refcount);
}

// 设置页为脏
void set_page_dirty(struct page *page) {
  if (!page)
    return;
  page->flags |= PAGE_DIRTY;
}

// 清除页脏标志
void clear_page_dirty(struct page *page) {
  if (!page)
    return;
  page->flags &= ~PAGE_DIRTY;
}

// 测试页是否为脏
int test_page_dirty(struct page *page) {
  if (!page)
    return 0;
  return (page->flags & PAGE_DIRTY) != 0;
}

// 锁定页
void lock_page(struct page *page) {
  if (!page)
    return;
  spinlock_lock(&page->page_lock);
  page->flags |= PAGE_LOCKED;
}

// 解锁页
void unlock_page(struct page *page) {
  if (!page)
    return;
  page->flags &= ~PAGE_LOCKED;
  spinlock_unlock(&page->page_lock);
}

// 尝试锁定页
int trylock_page(struct page *page) {
  if (!page)
    return 0;

  if (spinlock_trylock(&page->page_lock)) {
    page->flags |= PAGE_LOCKED;
    return 1;
  }
  return 0;
}
// 获取当前空闲页数量
int get_free_page_count(void) { return free_page_counter; }

