/*
 * Supervisor-mode startup codes
 */

#include <kernel/elf.h>
#include <kernel/proc_file.h>
#include <kernel/riscv.h>
#include <kernel/sched/process.h>

#include <kernel/sched/sched.h>

#include <kernel/fs/vfs.h>
#include <kernel/types.h>

#include <kernel/mm/kmalloc.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/pagetable.h>

#include <util/spinlock.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h> // 完全迁移到QEMU后移除

static void kernel_vm_init(void) {
  sprint("kernel_vm_init: start\n");
  // extern struct mm_struct init_mm;
  //  映射内核代码段和只读段
  g_kernel_pagetable = (pagetable_t)alloc_page()->paddr;
  // init_mm.pagetable = g_kernel_pagetable;
  //  之后它会被加入内核的虚拟空间，先临时用一个页
  memset(g_kernel_pagetable, 0, PAGE_SIZE);

  extern char _ftext[], _etext[], _fdata[], _end[];
  //sprint("_etext=%lx,_ftext=%lx\n", _etext, _ftext);

  pgt_map_pages(g_kernel_pagetable, (uint64)_ftext, (uint64)_ftext,
                (uint64)(_etext - _ftext),
                prot_to_type(PROT_READ | PROT_EXEC, 0));

  // 映射内核HTIF段
  pgt_map_pages(g_kernel_pagetable, (uint64)_etext, (uint64)_etext,
                (uint64)(_fdata - _etext),
                prot_to_type(PROT_READ | PROT_WRITE, 0));

  // 映射内核数据段
  pgt_map_pages(g_kernel_pagetable, (uint64)_fdata, (uint64)_fdata,
                (uint64)(_end - _fdata),
                prot_to_type(PROT_READ | PROT_WRITE, 0));
  // 映射内核数据段
  pgt_map_pages(g_kernel_pagetable, (uint64)_fdata, (uint64)_fdata,
                (uint64)(_end - _fdata),
                prot_to_type(PROT_READ | PROT_WRITE, 0));

  extern uint64 mem_size;
  // 对于剩余的物理内存空间做直接映射
  pgt_map_pages(g_kernel_pagetable, (uint64)_end, (uint64)_end,
                DRAM_BASE + mem_size,
                prot_to_type(PROT_READ | PROT_WRITE, 0));
  // // satp不通过这层映射找g_kernel_pagetable，但是为了维护它，也需要做一个映射
  // pgt_map_pages(g_kernel_pagetable, (uint64)g_kernel_pagetable,
  //               (uint64)g_kernel_pagetable, PAGE_SIZE,
  //               prot_to_type(PROT_READ | PROT_WRITE, 0));

  // 映射内核栈
  // pgt_map_pages(init_mm.pagetable, (uint64)init_mm.pagetable, )

  // pagetable_dump(g_kernel_pagetable);

  // // 6. 映射MMIO区域（如果有需要）
  // // 例如UART、PLIC等外设的内存映射IO区域

  sprint("kern_vm_init: complete\n");
}

// typedef union {
//   uint64 buf[MAX_CMDLINE_ARGS];
//   char *argv[MAX_CMDLINE_ARGS];
// } arg_buf;

//
// 初始化1号进程
//
static struct task_struct *load_init_process() {
  struct task_struct *task = alloc_init_task();
  extern struct mm_struct init_mm;
  task->mm = &init_mm;
  task->pfiles = alloc_pfm();
  // 8. 初始化标准文件描述符(stdin, stdout, stderr)
  // 这些会指向/dev/console或null设备
  // setup_std_fds(task->pfiles);

  // 9. 初始化信号处理
  // task->sighand = alloc_sighand_struct();
  // if (!task->sighand) {
  //   free_files_struct(task->files);
  //   free_mm(task->mm);
  //   free_kernel_stack(task->stack);
  //   kfree(task);
  //   return NULL;
  // }
  extern struct task_struct idle_task;
  // 10. 初始化父进程指针(初始进程的父进程是内核idle进程)
  task->parent = &idle_task;

  // 11. 初始化子进程列表
  INIT_LIST_HEAD(&task->children);
  INIT_LIST_HEAD(&task->sibling);
  INIT_LIST_HEAD(&task->queue_node);

  // // 12. 初始化CPU亲和性(初始进程可以在任何CPU上运行)
  // for (int i = 0; i < NR_CPUS; i++) {
  //   cpumask_set_cpu(i, &task->cpus_allowed);
  // }

  // 13. 初始化进程凭证(通常是root权限)
  // task->uid = 0;
  // task->gid = 0;
  // task->euid = 0;
  // task->egid = 0;

  // 14. 初始化进程命名空间(如果支持)
  // init_task_namespaces(task);

  // 15. 初始化其他可能的属性(根据系统需求)
  // init_task_timers(task);
  // init_task_io_accounting(task);

  return task;
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
volatile static int sig = 1;
int s_start(void) {
  extern void init_idle_task(void);

  sprint("Enter supervisor mode...\n");
  write_csr(satp, 0);

  int hartid = read_tp();
  if (hartid == 0) {
    init_page_manager();
    kernel_vm_init();
    pagetable_activate(g_kernel_pagetable);
    create_init_mm();
    kmem_init();
		init_scheduler();

    init_idle_task();
    // kmalloc在形式上需要使用init_mm的“用户虚拟空间分配器”
    // 所以我们在启用kmalloc之前，需要先初始化0号进程

    init_scheduler();
    init_fs();
    sig = 0;
  } else {
    while (sig) {
    }
    pagetable_activate(g_kernel_pagetable);
  }

  // sync_barrier(&sync_counter, NCPU);

  //  写入satp寄存器并刷新tlb缓存
  //    从这里开始，所有内存访问都通过MMU进行虚实转换

  sprint("Switch to user mode...\n");
  // the application code (elf) is first loaded into memory, and then put into
  // execution added @lab3_1
  insert_to_ready_queue(load_init_process());
  schedule();

  // we should never reach here.
  return 0;
}
