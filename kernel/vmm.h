#ifndef _VMM_H_
#define _VMM_H_

#include "riscv.h"
#include "process.h"

/* --- utility functions for virtual address mapping --- */
int map_pages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
// permission codes.
enum VMPermision {
  PROT_NONE = 0,
  PROT_READ = 1,
  PROT_WRITE = 2,
  PROT_EXEC = 4,
};


// 堆内存管理
typedef struct heap_block_t {
    size_t size;                  // 堆块大小（包含元数据）
    struct heap_block_t * prev;      // 前一个空闲块指针
    struct heap_block_t * next;      // 后一个空闲块指针
    int free;                     // 标志位，表示是否为空闲块
} heap_block;

uint64 prot_to_type(int prot, int user);
pte_t *page_walk(pagetable_t pagetable, uint64 va, int alloc);
uint64 lookup_pa(pagetable_t pagetable, uint64 va);

/* --- kernel page table --- */
// pointer to kernel page directory
extern pagetable_t g_kernel_pagetable;

void kern_vm_map(pagetable_t page_dir, uint64 va, uint64 pa, uint64 sz, int perm);

// Initialize the kernel pagetable
void kern_vm_init(void);

/* --- user page table --- */
void *user_va_to_pa(pagetable_t page_dir, void *va);
void user_vm_map(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa, int perm);
void user_vm_unmap(pagetable_t page_dir, uint64 va, uint64 size, int free);

void* malloc(size_t size);
void free( void* ptr);

// 定义一个对齐宏，确保按 8 字节对齐
#define ALIGN(addr, alignment) (((uintptr_t)(addr) + (alignment) - 1) & ~((alignment) - 1))

#endif
