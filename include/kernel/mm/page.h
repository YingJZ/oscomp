#ifndef _PAGE_H
#define _PAGE_H

#include <kernel/riscv.h>
#include <kernel/types.h>
#include <util/atomic.h>
#include <util/list.h>
#include <util/spinlock.h>
#include <kernel/mm/pagetable.h>
#include <kernel/dev/dtb.h>

// Forward declarations
struct address_space;

/**
 * 物理页结构体 - Linux风格的页描述符
 */
struct page {
  uint64 flags;       // 页标志

	// 文件页缓存
  struct address_space *mapping; // 所属的address_space
  atomic_t _refcount;						 // 引用计数
  uint64 index;									 // 在映射文件中的页索引

	// kmalloc分配器
  size_t kmalloc_size;           // kmalloc 分配的实际大小

  paddr_t paddr;         
	// 页的物理地址
  struct list_head lru;          // LRU链表节点
  spinlock_t page_lock;          // 页锁，用于同步访问
};

/* 页大小相关定义 */
#define PAGE_SHIFT 12
#ifndef PAGE_SIZE
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif
#define PAGE_MASK (~(PAGE_SIZE - 1))

// 页标志位定义
#define PAGE_DIRTY (1UL << 0)    // 脏页，需要回写
#define PAGE_UPTODATE (1UL << 1) // 页内容是最新的
#define PAGE_LOCKED (1UL << 2)   // 页已锁定，不可回收
#define PAGE_SLAB (1UL << 3)     // 页用于slab分配器
#define PAGE_BUDDY (1UL << 4)    // 页用于buddy系统
#define PAGE_RESERVED (1UL << 5) // 页已被保留，不可分配

// 初始化页管理子系统
void init_page_manager();
int get_free_page_count(void);


struct page *alloc_page(void);     // 分配单个页并返回page结构
void put_page(struct page *page);  // 减少引用计数（并释放）

// 页框号与地址转换函数
struct page *pfn_to_page(uint64 pfn);
struct page *addr_to_page(paddr_t addr);
uint64 page_to_pfn(struct page *page);

// 页引用计数操作
void get_page(struct page *page); // 增加页引用计数

// 页标志位操作
void set_page_dirty(struct page *page);   // 设置页为脏
void clear_page_dirty(struct page *page); // 清除页脏标志
int test_page_dirty(struct page *page);   // 测试页是否为脏

// 页锁操作
void lock_page(struct page *page);   // 锁定页
void unlock_page(struct page *page); // 解锁页
int trylock_page(struct page *page); // 尝试锁定页


// 检查页内容是否最新
static inline int page_uptodate(struct page *page) {
  return (page->flags & PAGE_UPTODATE) != 0;
}

// 将页内容标记为最新
static inline void set_page_uptodate(struct page *page) {
  page->flags |= PAGE_UPTODATE;
}



#endif /* _PAGE_H */