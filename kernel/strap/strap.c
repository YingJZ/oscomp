/*
 * Utility functions for trap handling in Supervisor mode.
 */

#include <kernel/strap.h>

#include <kernel/memlayout.h>
#include <kernel/pmm.h>
#include <kernel/process.h>
#include <kernel/riscv.h>
#include <kernel/sched.h>
#include <util/string.h>
#include <kernel/syscall.h>
#include "util/functions.h"
#include <kernel/vmm.h>

#include "spike_interface/spike_utils.h"

//
// handling the syscalls. will call do_syscall() defined in kernel/syscall.c
//
static void handle_syscall(trapframe *tf) {
  // tf->epc points to the address that our computer will jump to after the trap
  // handling. for a syscall, we should return to the NEXT instruction after its
  // handling. in RV64G, each instruction occupies exactly 32 bits (i.e., 4
  // Bytes)
  tf->epc += 4;

  // TODO (lab1_1): remove the panic call below, and call do_syscall (defined in
  // kernel/syscall.c) to conduct real operations of the kernel side for a
  // syscall. IMPORTANT: return value should be returned to user app, or else,
  // you will encounter problems in later experiments!
  tf->regs.a0 = do_syscall(tf->regs.a0, tf->regs.a1, tf->regs.a2, tf->regs.a3,
                           tf->regs.a4, tf->regs.a5, tf->regs.a6, tf->regs.a7);
  // panic( "call do_syscall to accomplish the syscall and lab1_1 here.\n" );
}

//
// global variable that store the recorded "ticks". added @lab1_3
static uint64 g_ticks = 0;
//
// added @lab1_3
//
void handle_mtimer_trap() {
  sprint("Ticks %d\n", g_ticks);
  // TODO (lab1_3): increase g_ticks to record this "tick", and then clear the
  // "SIP" field in sip register. hint: use write_csr to disable the SIP_SSIP
  // bit in sip. panic( "lab1_3: increase g_ticks by one, and clear SIP field in
  // sip register.\n" );
  g_ticks++;
  int hartid = read_tp();
  current_percpu[hartid]->tick_count++;

  write_csr(sip, read_csr(sip) & ~SIP_SSIP);
}

//
// the page fault handler. added @lab2_3. parameters:
// sepc: the pc when fault happens;
// stval: the virtual address that causes pagefault when being accessed.
//
void handle_user_page_fault(uint64 mcause, uint64 sepc, uint64 stval) {
  process *ps = CURRENT;
  // int hartid = read_tp();
  sprint("sepc=%lx,handle_page_fault: %lx\n",sepc, stval);
  switch (mcause) {
  case CAUSE_STORE_PAGE_FAULT:
    pte_t *pte = page_walk(ps->pagetable, stval, 0);
    if (((uint64)*pte & PTE_W) == 0 && (*pte & PTE_X) == 0) { // 为只读的共享页
      uint64 page_va = stval & (~0xfff);
      uint64 newpa = (uint64)Alloc_page();
      memcpy((void *)newpa, (void *)PTE2PA(*pte), PGSIZE);
      user_vm_unmap((pagetable_t)ps->pagetable, page_va, PGSIZE, 0);
			user_vm_map((pagetable_t)ps->pagetable, page_va, PGSIZE, newpa, prot_to_type(PROT_WRITE | PROT_READ, 1));
    }else if (stval >= ps->user_stack_bottom - PGSIZE) {
      ps->user_stack_bottom -= PGSIZE;
      void *pa = Alloc_page();
      user_vm_map((pagetable_t)ps->pagetable, ps->user_stack_bottom, PGSIZE,
                  (uint64)pa, prot_to_type(PROT_WRITE | PROT_READ, 1));
      ps->mapped_info[STACK_SEGMENT].va = ps->user_stack_bottom;
      ps->mapped_info[STACK_SEGMENT].npages++;

    } else {
      panic("this address is not available!");
    }

    break;
  default:
    sprint("unknown page fault.\n");
    break;
  }
}

//
// implements round-robin scheduling. added @lab3_3
//
void rrsched() {
  // TODO (lab3_3): implements round-robin scheduling.
  // hint: increase the tick_count member of current process by one, if it is
  // bigger than TIME_SLICE_LEN (means it has consumed its time slice), change
  // its status into READY, place it in the rear of ready queue, and finally
  // schedule next process to run.
  // panic( "You need to further implement the timer handling in lab3_3.\n" );
  int hartid = read_tp();
  if (current_percpu[hartid]->tick_count >= TIME_SLICE_LEN) {
    current_percpu[hartid]->tick_count = 0;
    sys_user_yield();
  }
}

//
// kernel/smode_trap.S will pass control to smode_trap_handler, when a trap
// happens in S-mode.
//
void smode_trap_handler(void) {
  int hartid = read_tp();
  // make sure we are in User mode before entering the trap handling.
  // we will consider other previous case in lab1_3 (interrupt).
  if ((read_csr(sstatus) & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  assert(current_percpu[hartid]);
  // save user process counter.
  current_percpu[hartid]->trapframe->epc = read_csr(sepc);

  // if the cause of trap is syscall from user application.
  // read_csr() and CAUSE_USER_ECALL are macros defined in kernel/riscv.h
  uint64 cause = read_csr(scause);

  // use switch-case instead of if-else, as there are many cases since lab2_3.
  switch (cause) {
  case CAUSE_USER_ECALL:
    handle_syscall(current_percpu[hartid]->trapframe);
    // sprint("coming back from syscall\n");
    break;
  case CAUSE_MTIMER_S_TRAP:
    handle_mtimer_trap();
    // invoke round-robin scheduler. added @lab3_3
    rrsched();
    break;
  case CAUSE_STORE_PAGE_FAULT:
  case CAUSE_LOAD_PAGE_FAULT:
    // the address of missing page is stored in stval
    // call handle_user_page_fault to process page faults
    handle_user_page_fault(cause, read_csr(sepc), read_csr(stval));
    break;
  default:
    sprint("smode_trap_handler(): unexpected scause %p\n", read_csr(scause));
    sprint("            sepc=%p stval=%p\n", read_csr(sepc), read_csr(stval));
    panic("unexpected exception happened.\n");
    break;
  }
  // sprint("calling switch_to, current = 0x%x\n", current);
  // continue (come back to) the execution of current process.
  switch_to(current_percpu[hartid]);
}
