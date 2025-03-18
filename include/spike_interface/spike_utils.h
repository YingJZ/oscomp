#ifndef _SPIKE_UTILS_H_
#define _SPIKE_UTILS_H_

#include <sys/types.h>
#include <spike_interface/spike_file.h>
#include "spike_memory.h"
#include <spike_interface/spike_htif.h>
#include "util/printf.h"

long frontend_syscall(long n, __uint64_t a0, __uint64_t a1, __uint64_t a2, __uint64_t a3, __uint64_t a4, __uint64_t a5,
                      __uint64_t a6);

void poweroff(__uint16_t code) __attribute((noreturn));
// void sprint(const char* s, ...);
// void putstring(const char* s);
void shutdown(int) __attribute__((noreturn));

#define assert(x)                              \
  ({                                           \
    if (!(x)) die("assertion failed: %s", #x); \
  })
#define die(str, ...)                                              \
  ({                                                               \
    sprint("%s:%d: " str "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    poweroff(-1);                                                  \
  })

void do_panic(const char* s, ...) __attribute__((noreturn));
void kassert_fail(const char* s) __attribute__((noreturn));

//void shutdown(int code);

#define panic(s, ...)do_panic(s "\n", ##__VA_ARGS__)


#define kassert(cond)                    \
  do {                                   \
    if (!(cond)) kassert_fail("" #cond); \
  } while (0)

#endif
