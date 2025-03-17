/*
 * Utilities implemented by using the Spike HTIF.
 *
 * codes are borrowed from riscv-pk (https://github.com/riscv/riscv-pk)
 */

#include <util/atomic.h>
#include <util/spinlock.h>
#include <spike_interface/spike_htif.h>
#include <kernel/types.h>
#include <util/snprintf.h>
#include <spike_interface/spike_utils.h>
#include <spike_interface/spike_file.h>

#include <kernel/dev/sbi.h>

//=============    encapsulating htif syscalls, invoking Spike functions    =============
long frontend_syscall(long n, __uint64_t a0, __uint64_t a1, __uint64_t a2, __uint64_t a3, __uint64_t a4,
      __uint64_t a5, __uint64_t a6) {
  static volatile __uint64_t magic_mem[8];

  static spinlock_t lock = SPINLOCK_INIT;
  spinlock_lock(&lock);

  magic_mem[0] = n;
  magic_mem[1] = a0;
  magic_mem[2] = a1;
  magic_mem[3] = a2;
  magic_mem[4] = a3;
  magic_mem[5] = a4;
  magic_mem[6] = a5;
  magic_mem[7] = a6;

  htif_syscall((uintptr_t)magic_mem);

  long ret = magic_mem[0];

  spinlock_unlock(&lock);
  return ret;
}

//===============    Spike-assisted printf, output string to terminal    ===============
static uintptr_t mcall_console_putchar(__uint8_t ch) {
  if (htif) {
    htif_console_putchar(ch);
  }
  return 0;
}

void vprintk(const char* s, va_list vl) {
  char out[256];
  int res = vsnprintf(out, sizeof(out), s, vl);
  //you need spike_file_init before this call
  // spike_file_write(stderr, out, res < sizeof(out) ? res : sizeof(out));
	for (int i = 0; i < res; i++) { // TODO: 临时实现（最终需要迁移到 uart）
		SBI_PUTCHAR(out[i]);
	}
}

void printk(const char* s, ...) {
  va_list vl;
  va_start(vl, s);

  vprintk(s, vl);

  va_end(vl);
}

void putstring(const char* s) {
  while (*s) SBI_PUTCHAR(*s++); // TODO: 临时实现（最终需要迁移到 uart）
}

void vprintm(const char* s, va_list vl) {
  char buf[256];
  vsnprintf(buf, sizeof buf, s, vl);
  putstring(buf);
}

// 在 QEMU 中调用这个 sprint 会发生报错！
// 因为其最终会调用 HTIFSYS_write，而 QEMU 并没有实现这个系统调用。
void sprint(const char* s, ...) {
  va_list vl;
  va_start(vl, s);

  vprintk(s, vl);

  va_end(vl);
}

//===============    Spike-assisted termination, panic and assert    ===============
void poweroff(uint16_t code) {
  assert(htif);
  sprint("Power off\r\n");
  if (htif) {
    htif_poweroff();
  } else {
    // we consider only one HART case in PKE experiments. May extend this later.
    // send_ipi_many(0, IPI_HALT);
    while (1) {
      asm volatile("wfi\n");
    }
  }
}

void shutdown(int code) {
  sprint("System is shutting down with exit code %d.\n", code);
  frontend_syscall(HTIFSYS_exit, code, 0, 0, 0, 0, 0, 0);
  while (1)
    ;
}

void do_panic(const char* s, ...) {
  va_list vl;
  va_start(vl, s);

  sprint(s, vl);
  shutdown(-1);

  va_end(vl);
}

void kassert_fail(const char* s) {
  register uintptr_t ra asm("ra");
  do_panic("assertion failed @ %p: %s\n", ra, s);
  //    sprint("assertion failed @ %p: %s\n", ra, s);
  shutdown(-1);
}
